# Grain native fork candidate

This independent repository is co-located at `native/transcribe.cpp/` inside the Grain workspace and ignored by Grain's Git repository. It starts at upstream `v0.2.3`, commit `63a44d9239d610b3908e8a66b384924cd4a77217`.

The candidate carries the independently audited existing Grain vendor patch: sequence-level detokenization, opt-in stateless Parakeet TDT window decoding, safe Rust adaptation, regenerated bindings/ABI digest, bindgen path portability, and FluidAudio notices. Original upstream Cargo/workspace/build layout is retained. C/C++ changes use the upstream pinned formatter. Grain scheduling, journaling, merging, capture, cancellation, and post-processing remain in Grain.

Official source archive checksums:

- `transcribe-cpp-0.2.3`: `b405c121ef674311b9e8eba83e45d859663da681952c7b95b730792ffde358e7`
- `transcribe-cpp-sys-0.2.3`: `00e81030804f0ce2dd83761ba18e90e0c0c0c2794d68ca008e24245ed6d2702e`

This is a candidate extraction, not a released Grain dependency. Grain still consumes the original vendor paths. Before cutover, implement the independently versioned native contract/ABI check, broaden decoder/failure tests, and complete same-model/backend application/resource/package parity. In particular, a later 0.2.4 port must honor newly fallible predictor/joint helpers. Preserve upstream scratch cleanup and matching ggml/backend builds.

Local validation (2026-09-26): Windows MSVC static CPU library and CLI build; all 37 enabled CTest cases; safe-wrapper PKFW materialization test; wrapper all-targets Clippy with warnings denied; pinned C++ formatter/Rust format checks; bindgen `--check` with libclang 18.1.1. Fifteen carried source/binding/notice files match the vendor baseline, allowing canonical C++ formatting. Rust tests intentionally linked this checkout's installed native build through `TRANSCRIBE_DIR`. Real-model/GPU/package parity and runtime identity remain required before release.

The full migration, adjacent-release audit, maintenance consensus, validation gates, and rollback policy are in the parent workspace's `docs/TRANSCRIBE-CPP-FORK-PLAN.md`. Read both repositories' `AGENTS.md` before edits. Use native commits for this tree and Grain commits for integration. Published releases are immutable; upstream updates use unpublished branches and new tested release revisions.

The `upstream` remote points to `handy-computer/transcribe.cpp`; `origin` is `https://github.com/Punit-Dethe/transcribe.cpp.git`. Production consumption must pin both Rust packages to one full tested source SHA; do not rely on the ignored working folder or a moving branch. Preserve existing Git author identity.

## Contract implementation

Contract revision 1, patch identity `grain-flow-v1`, semantic ABI digest `a3b263c9e59d3ca9`. Pure native queries use static storage and source metadata, including when `.git` is absent. Rust `ensure_compatible()` validates version, revision, identity, digest and 16 size/alignment pairs before model params or backend setup; it is cached once. An unpatched DLL missing exports fails in the OS loader; a DLL with incompatible values returns a compatibility error. This is compatibility checking, not cryptographic authenticity; release provenance records the full source SHA and artifact hashes separately.

Tests use tiny deterministic ggml weights against the actual decoder, covering blank and repeated zero-duration advance, duration boundaries, 150-token main/10-token tail limits, fresh predictor state, graph-setup failure, reviewed v2/v3/head/blank/duration gates and extension validation. Exported detokenization tests cover split UTF-8 tokens, two-pass/exact/short buffers, no NUL writes, null/count errors, unavailable tokenizer, C++ exception containment and subsequent reuse. All 39 enabled native tests and 14 Rust unit/no-model tests pass locally on Windows CPU; Clippy and generator/formatter checks pass. This does not establish real-model/backend/package parity.

`.github/workflows/grain-fork.yml` uses standard hosted Windows/Linux/macOS runners, with native tests, source-built Rust, dynamic backend consumption, Apple Metal compilation, generated ABI checks, and a Linux shared source-archive smoke. Upstream workflows use some maintainer-specific runners and publishing infrastructure; fork contract CI is independent of those. Run `cargo run -p transcribe-cpp --locked --no-default-features --example grain_contract` for installed/source-link diagnostics. Shipping identity/CPU discovery uses the same example with `--features dynamic-backends`.

Upgrade checklist: read the parent migration plan's adjacent-version audit; specifically recheck predictor/joint failure contracts on 0.2.4. No decoder algorithm, encoder or ggml upgrade is included here. Model, cancellation, retained memory and installed application gates remain distinct from the synthetic suite. Do not publish a production release tag until those gates have evidence.
