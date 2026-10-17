//! Downstream/package smoke: validates the loaded library before model use.
fn main() -> transcribe_cpp::Result<()> {
    transcribe_cpp::ensure_compatible()?;
    transcribe_cpp::init_backends_default()?;
    if !transcribe_cpp::backend_available(transcribe_cpp::Backend::Cpu) {
        return Err(transcribe_cpp::Error::VersionMismatch(
            "native package has no CPU backend".into(),
        ));
    }
    println!(
        "{} contract={} abi={} devices={}",
        transcribe_cpp::grain_patch_id(),
        transcribe_cpp::grain_contract_revision(),
        transcribe_cpp::runtime_header_hash(),
        transcribe_cpp::device_count()
    );
    Ok(())
}
