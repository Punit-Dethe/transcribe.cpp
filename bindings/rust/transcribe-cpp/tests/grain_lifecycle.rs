//! Explicit local model gate. No download, transcript fixture, or clean skip.
use transcribe_cpp::{
    Backend, CancelToken, Error, Model, ModelOptions, ParakeetTdtWindowOptions, RunExtension,
    RunOptions, SessionOptions, TimestampKind,
};

#[test]
#[ignore = "requires TRANSCRIBE_GRAIN_MODEL and TRANSCRIBE_GRAIN_AUDIO (reviewed GGUF/PCM16 WAV)"]
fn parakeet_abort_invalid_window_and_model_lifetime_allow_reuse() {
    transcribe_cpp::init_backends_default().unwrap();
    let model_path = std::env::var("TRANSCRIBE_GRAIN_MODEL").expect("local reviewed model path");
    let audio_path = std::env::var("TRANSCRIBE_GRAIN_AUDIO").expect("local mono PCM16 16 kHz WAV");
    let mut reader = hound::WavReader::open(audio_path).unwrap();
    let spec = reader.spec();
    assert_eq!(
        (spec.channels, spec.sample_rate, spec.bits_per_sample),
        (1, 16_000, 16)
    );
    assert_eq!(spec.sample_format, hound::SampleFormat::Int);
    let pcm: Vec<f32> = reader
        .samples::<i16>()
        .map(|s| s.unwrap() as f32 / 32768.0)
        .collect();
    assert!(!pcm.is_empty());
    let model = Model::load_with(
        &model_path,
        &ModelOptions {
            backend: Backend::Cpu,
            ..Default::default()
        },
    )
    .unwrap();
    assert_eq!(model.arch(), "parakeet");
    assert!(matches!(
        model.variant().as_str(),
        "tdt-0.6b-v2" | "tdt-0.6b-v3"
    ));
    let mut session = model
        .session_with(&SessionOptions {
            n_threads: 4,
            ..Default::default()
        })
        .unwrap();
    drop(model); // Session must retain native model ownership.
    let options = RunOptions {
        timestamps: TimestampKind::Token,
        ..Default::default()
    };
    let baseline = session.run(&pcm, &options).unwrap();
    assert!(!baseline.tokens.is_empty(), "use a speech fixture");
    let cancel = CancelToken::new();
    session.set_cancel_token(&cancel);
    cancel.cancel();
    assert!(matches!(
        session.run(&pcm, &options),
        Err(Error::Aborted { .. })
    ));
    assert!(session.was_aborted());
    cancel.reset();
    let recovered = session.run(&pcm, &options).unwrap();
    assert!(!session.was_aborted());
    assert_eq!(recovered.text, baseline.text);
    assert_eq!(recovered.tokens, baseline.tokens);
    session.clear_cancel_token();
    let invalid = RunOptions {
        family: Some(RunExtension::ParakeetTdtWindow(ParakeetTdtWindowOptions {
            decode_start_frame: 0,
            decode_end_frame: 0,
            timestamp_offset_frames: 0,
            finalize_tail: false,
        })),
        ..options.clone()
    };
    assert!(matches!(
        session.run(&pcm, &invalid),
        Err(Error::InvalidArgument(_))
    ));
    let recovered = session.run(&pcm, &options).unwrap();
    assert_eq!(recovered.text, baseline.text);
    assert_eq!(recovered.tokens, baseline.tokens);
    let ids: Vec<i32> = baseline.tokens.iter().map(|token| token.id).collect();
    assert_eq!(
        session.model().detokenize(&ids).unwrap().trim(),
        baseline.text.trim()
    );
}
