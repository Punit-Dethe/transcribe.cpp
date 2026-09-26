# Grain native fork candidate

This independent repository is co-located at `native/transcribe.cpp/` inside the Grain workspace and ignored by Grain's Git repository. It starts at upstream `v0.2.3`, commit `63a44d9239d610b3908e8a66b384924cd4a77217`.

The candidate carries the independently audited existing Grain vendor patch: sequence-level detokenization, opt-in stateless Parakeet TDT window decoding, safe Rust adaptation, regenerated bindings/ABI digest, bindgen path portability, and FluidAudio notices. Original upstream Cargo/workspace/build layout is retained. C/C++ changes use the upstream pinned formatter. Grain scheduling, journaling, merging, capture, cancellation, and post-processing remain in Grain.

Official source archive checksums:

- `transcribe-cpp-0.2.3`: `b405c121ef674311b9e8eba83e45d859663da681952c7b95b730792ffde358e7`
- `transcribe-cpp-sys-0.2.3`: `00e81030804f0ce2dd83761ba18e90e0c0c0c2794d68ca008e24245ed6d2702e`

This is a candidate extraction, not a released Grain dependency. Grain still consumes the original vendor paths. Before cutover, implement the independently versioned native contract/ABI check, broaden decoder/failure tests, and complete same-model/backend application/resource/package parity. In particular, a later 0.2.4 port must honor newly fallible predictor/joint helpers. Preserve upstream scratch cleanup and matching ggml/backend builds.

Local validation (2026-09-26): Windows MSVC static CPU library and CLI build; all 37 enabled CTest cases; safe-wrapper PKFW materialization test; wrapper all-targets Clippy with warnings denied; pinned C++ formatter/Rust format checks; bindgen `--check` with libclang 18.1.1. Fifteen carried source/binding/notice files match the vendor baseline, allowing canonical C++ formatting. Rust tests intentionally linked this checkout's installed native build through `TRANSCRIBE_DIR`. Real-model/GPU/package parity and runtime identity remain required before release.

The full migration, adjacent-release audit, maintenance consensus, validation gates, and rollback policy are in the parent workspace's `docs/TRANSCRIBE-CPP-FORK-PLAN.md`. Read both repositories' `AGENTS.md` before edits. Use native commits for this tree and Grain commits for integration. Published releases are immutable; upstream updates use unpublished branches and new tested release revisions.

The `upstream` remote points to `handy-computer/transcribe.cpp`. Establish the maintainer's GitHub fork as `origin` before publishing. Production consumption must pin both Rust packages to one full tested source SHA; do not rely on the ignored working folder or a moving branch. Preserve existing Git author identity.
