//! Version, fork identity, ABI introspection and the load-time compatibility gate.
//!
//! Pre-1.0 the on-disk ABI may break between minor releases, so the binding
//! and the linked library must agree on the base `MAJOR.MINOR.PATCH`. The gate
//! runs once, lazily, on the first model load (and is exposed directly so a
//! host can check up front). Packaging-only suffixes on the runtime string are
//! tolerated — only the leading release segment is compared.

use std::sync::OnceLock;

use transcribe_cpp_sys as sys;

use crate::error::{Error, Result};
use crate::result::owned_str;
use crate::types::AbiStruct;

/// The base version string this crate's bindings were built against.
///
/// Taken from this crate's own `Cargo.toml` (`CARGO_PKG_VERSION`), not the
/// generated FFI macros: a version-only bump must not churn the committed
/// bindings or the abihash (notes/releasing.md §8 P0 #1). The generators no
/// longer emit `TRANSCRIBE_VERSION_*`, so this is also the only source left.
pub fn compiled_version() -> String {
    base(env!("CARGO_PKG_VERSION")).to_string()
}

/// The `MAJOR.MINOR.PATCH` version string of the linked native library.
pub fn version() -> String {
    owned_str(unsafe { sys::transcribe_version() })
}

/// The short git commit the native library was built from (or "unknown").
pub fn version_commit() -> String {
    owned_str(unsafe { sys::transcribe_version_commit() })
}

/// The public-ABI digest the committed bindings were generated against.
pub fn header_hash() -> &'static str {
    sys::PUBLIC_HEADER_HASH
}

/// Revision of the loaded library's Grain detokenize/PKFW contract.
pub fn grain_contract_revision() -> u32 {
    unsafe { sys::transcribe_grain_contract_revision() }
}

/// Semantic patch identity, independent of optional Git build metadata.
pub fn grain_patch_id() -> String {
    owned_str(unsafe { sys::transcribe_grain_patch_id() })
}

/// Public-ABI digest compiled into the loaded native library.
pub fn runtime_header_hash() -> String {
    owned_str(unsafe { sys::transcribe_runtime_header_hash() })
}

/// The native library's `sizeof` for a public ABI struct, or 0 if unknown.
pub fn abi_struct_size(which: AbiStruct) -> usize {
    unsafe { sys::transcribe_abi_struct_size(which.to_raw()) }
}

/// The native library's `alignof` for a public ABI struct, or 0 if unknown.
pub fn abi_struct_align(which: AbiStruct) -> usize {
    unsafe { sys::transcribe_abi_struct_align(which.to_raw()) }
}

/// Leading dotted-numeric release segment ("0.0.1.post1" -> "0.0.1").
fn base(v: &str) -> &str {
    let end = v
        .find(|c: char| !(c.is_ascii_digit() || c == '.'))
        .unwrap_or(v.len());
    v[..end].trim_end_matches('.')
}

static GATE: OnceLock<std::result::Result<(), String>> = OnceLock::new();

fn validate_identity(
    runtime: &str,
    revision: u32,
    identity: &str,
    hash: &str,
) -> std::result::Result<(), String> {
    let compiled = compiled_version();
    if base(runtime) != base(&compiled) {
        return Err(format!(
            "loaded transcribe library is {runtime}, but these bindings require {compiled}"
        ));
    }
    let expected_identity = std::ffi::CStr::from_bytes_with_nul(sys::TRANSCRIBE_GRAIN_PATCH_ID)
        .expect("generated static patch identity")
        .to_str()
        .expect("ASCII patch identity");
    if revision != sys::TRANSCRIBE_GRAIN_CONTRACT_REVISION || identity != expected_identity {
        return Err(format!("loaded transcribe fork contract is {identity:?}/{revision}, expected {expected_identity}/{expected_revision}", expected_revision = sys::TRANSCRIBE_GRAIN_CONTRACT_REVISION));
    }
    if hash != header_hash() {
        return Err(format!(
            "loaded transcribe ABI digest is {hash:?}, expected {}",
            header_hash()
        ));
    }
    Ok(())
}

fn validate_layout(
    which: AbiStruct,
    expected: (usize, usize),
    actual: (usize, usize),
) -> std::result::Result<(), String> {
    if actual != expected {
        return Err(format!("loaded transcribe {which:?} layout is {actual:?}, expected {expected:?} (size, alignment)"));
    }
    Ok(())
}

/// Check the loaded native fork before constructing any model/session params.
/// Cached once per process; also available for startup/package diagnostics.
/// An unpatched shared library lacking the contract exports fails in the OS
/// loader before Rust executes. A library exposing incompatible values fails here.
pub fn ensure_compatible() -> Result<()> {
    let outcome = GATE.get_or_init(|| {
        validate_identity(
            &version(),
            grain_contract_revision(),
            &grain_patch_id(),
            &runtime_header_hash(),
        )?;
        macro_rules! layout {
            ($which:ident, $raw:ty) => {
                (
                    AbiStruct::$which,
                    (std::mem::size_of::<$raw>(), std::mem::align_of::<$raw>()),
                )
            };
        }
        let layouts = [
            layout!(ModelLoadParams, sys::transcribe_model_load_params),
            layout!(SessionParams, sys::transcribe_session_params),
            layout!(RunParams, sys::transcribe_run_params),
            layout!(StreamParams, sys::transcribe_stream_params),
            layout!(Capabilities, sys::transcribe_capabilities),
            layout!(Timings, sys::transcribe_timings),
            layout!(Segment, sys::transcribe_segment),
            layout!(Word, sys::transcribe_word),
            layout!(Token, sys::transcribe_token),
            layout!(StreamUpdate, sys::transcribe_stream_update),
            layout!(StreamText, sys::transcribe_stream_text),
            layout!(SessionLimits, sys::transcribe_session_limits),
            layout!(Ext, sys::transcribe_ext),
            layout!(DeviceInfo, sys::transcribe_device_info),
            layout!(SpeakerSegment, sys::transcribe_speaker_segment),
            layout!(
                ParakeetTdtWindowExt,
                sys::transcribe_parakeet_tdt_window_ext
            ),
        ];
        for (which, expected) in layouts {
            validate_layout(
                which,
                expected,
                (abi_struct_size(which), abi_struct_align(which)),
            )?;
        }
        Ok(())
    });
    outcome.clone().map_err(Error::VersionMismatch)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn linked_native_contract_and_all_layouts_match() {
        ensure_compatible().unwrap();
    }

    #[test]
    fn rejects_wrong_version_identity_revision_and_digest() {
        let version = compiled_version();
        let identity = grain_patch_id();
        let revision = sys::TRANSCRIBE_GRAIN_CONTRACT_REVISION;
        let hash = header_hash();
        assert!(validate_identity(&format!("{version}-grain"), revision, &identity, hash).is_ok());
        assert!(validate_identity("999.0.0", revision, &identity, hash).is_err());
        assert!(validate_identity(&version, revision + 1, &identity, hash).is_err());
        assert!(validate_identity(&version, revision, "upstream", hash).is_err());
        assert!(validate_identity(&version, revision, &identity, "0000000000000000").is_err());
    }

    #[test]
    fn rejects_unknown_or_incompatible_struct_layout() {
        let which = AbiStruct::ParakeetTdtWindowExt;
        assert!(validate_layout(which, (32, 8), (0, 0)).is_err());
        assert!(validate_layout(which, (32, 8), (40, 8)).is_err());
        assert!(validate_layout(which, (32, 8), (32, 4)).is_err());
    }
}
