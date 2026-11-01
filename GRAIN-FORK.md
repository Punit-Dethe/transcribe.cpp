# Grain native fork — 0.4.0 integration candidate

This branch starts at Handy transcribe.cpp **v0.4.0**, commit
`f1dcd15858c4bbba87ca97862b4e448b2cf15b8f`. It ports the cumulative Grain
patch from `2d336ea878ded786f63dd53520beed8fc6192c61`; obsolete intermediate
implementations are not replayed. Both Rust packages retain version 0.4.0.
Grain must pin both to the same tested full source SHA.

## Maintained patch

- Sequence detokenization through the loaded model tokenizer, including split
  UTF-8 tokens, bounded two-pass buffers and C ABI exception containment.
- Opt-in stateless Parakeet TDT window decoding (PKFW) for reviewed v2/v3
  artifacts. Fresh predictor state, context filtering, frame offsets, symbol
  budgets, final-tail handling and graph failure propagation remain tested.
- Safe Rust adapters and serialized run options for worker transport; generated
  Rust, Python and TypeScript bindings, Swift digest and FluidAudio notices.
- Runtime contract checked before native setup/model use: revision **3**,
  identity **`grain-flow-v1`**, semantic ABI **`3669c8465a11b2f5`**, and
  **30** size/alignment pairs. Upstream ABI IDs 0–28 are preserved; PKFW is 29.

Upstream 0.4 owns the encoder, ggml, vocabulary/prompt/prefix APIs, VAD,
language identification and diarization. Predictor/joint failure handling is
already upstream and is not duplicated. Grain scheduling, bounded journals,
overlap merging, capture, post-processing and model ownership stay in Grain.
No persistent native Flow handle or second inference engine is introduced.

## Local verification (Windows x64, 2026-10-17)

The static CPU Release build and CLI pass **59 native tests**; a separate
diarization CLI smoke skips because its fixture is absent. Tiny ggml tests
exercise actual decoder dispatch failures, recovery and PKFW semantics.
Safe Rust with `serde` passes **13 library and 10 no-model tests**, including
JSON and binary worker-option roundtrips. Formatting, Clippy, generated-binding
drift, Swift ABI and source-package checks pass. The sys source package is
approximately 4.3 MiB compressed (1,718 files).

The explicit real-model lifecycle gate passes with catalog-pinned v2 Q8 and
v3 Q4 models and public `samples/jfk.wav`: cancellation/reset, invalid-window
recovery, model lifetime and detokenization. The maintained Grain Flow audit
compares 11/35/83-second public repeated-JFK fixtures against the old 0.2.3
fork. Full cased/punctuated transcript strings agree for Batch, Flow,
independent windows and diagnostic windows. Confidence and some timestamps
differ; this upgrade does not claim bit-exact extraction parity. These fixtures
do not establish multilingual accuracy or general sentence-seam correctness.
Overlapping build/inference runs are not performance benchmarks.

Shared backend, hosted platform, source-built application, package and retained
resource evidence must be recorded before promotion. The maintained workflow
covers hosted Windows/Linux/macOS native contracts, Rust source builds,
serialization, shared consumption, generators, packages, Apple Metal and a
Git-free source archive. A workflow definition is not a passing run. No
production release tag is published here.

## Maintenance

This ignored co-located checkout is development-only. Production builds use the
immutable Git source pair in Grain's `native/transcribe-fork.json`. Runtime
compatibility checks are not authenticity checks: source SHA and artifact
hashes belong in release provenance. Read both repositories' `AGENTS.md`,
Grain's `Upstream/UPSTREAM.md` and `docs/TRANSCRIBE-CPP-FORK.md` before the
next port. Keep generated outputs, notices, layouts, backend features and all
consumption pins synchronized.

Use `uv run` for Python, clang-format 22.1.5 and libclang 18.1.1 for generators.
Run `cargo run --locked -p transcribe-cpp --no-default-features --example
grain_contract` for runtime diagnostics; add `--features dynamic-backends`
for shared consumption. Build dynamic CMake backends with
**`TRANSCRIBE_GGML_BACKEND_DL=ON`**, not only `GGML_BACKEND_DL`.

## Second patch: Nemotron-3 Diarization (2026-11-02)

Ported from `glimpse-hq/transcribe.cpp` (MIT) as taken at its 0.4.0 merge
`4b3f456`; the diarization work originates in Glimpse commits `a1e9f53`
(add Nemotron 3), `b893ed2` (Transformers and AMI validation), `7cd197e`
(push-audio streaming) and `ba087c2` (Rust live streaming). Core ML, Parakeet,
Whisper, Qwen and every other non-diarization Glimpse change is excluded.

- New arch `nemotron3_diar` (`src/arch/nemotron3_diar/`), registered beside
  Sortformer; the Glimpse GGUF schema and arch key are kept unchanged.
- The offline path is the existing `transcribe_diarize_run` with the `SFDR`
  extension; `VERY_LOW_LATENCY` and `ULTRA_LOW_LATENCY` presets are added
  (Nemotron-3 only, rejected by Sortformer v2.1).
- Push-audio live diarization: `transcribe_diarize_stream_*`, the
  `DIARIZE_STREAM` extension slot (3) and the `SFLV` kind. Glimpse numbers
  `TRANSCRIBE_ABI_DIARIZE_STREAM_PARAMS` 29, which this fork already uses for
  PKFW, so it is **31** here.
- Contract revision **6** (identity unchanged, `grain-flow-tdt`), semantic ABI
  **`54c13920a14bb4b6`**, 32 size/alignment pairs in the Rust gate.
- Safe Rust: `DiarizeSession::stream`, `DiarizeStream`, `OwnedDiarizeStream`,
  `DiarizeStreamOptions`, `DiarizeStreamExtension::SortformerLive`.
- Artifact: `Glimpse-Dictation/Nemotron-3-Diarization-gguf@273ebb0`,
  `nemotron-3-diarization-Q8_0.gguf`, 105,936,416 bytes (OpenMDW-1.1).

Local verification (Windows x64, static CPU Release): 60 native tests pass
(one CLI diarize smoke skipped without its fixture env). The CLI diarizes
`samples/sortformer-2spk-mix.wav` into 2 speakers / 5 segments at 73x real
time, and an 86 s three-speaker composite (Jobs, JFK, FLEURS, each speaking
twice) into exactly three speakers with consistent identities across returns.
Safe Rust passes 14 library and 10 no-model tests, Clippy with `-D warnings`,
`rustfmt`, regenerated Rust/Python/TypeScript bindings, the Swift ABI pin and
`grain_contract` (`contract=6 abi=54c13920a14bb4b6`). Model-gated native
Nemotron tests (`TRANSCRIBE_BUILD_REAL_MODEL_TESTS`) use POSIX `setenv` and do
not build on MSVC, the same as the existing Sortformer gated test.
