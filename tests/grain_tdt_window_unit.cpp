// Real ggml decoder with tiny deterministic weights. No model downloads or
// alternate production decoder: assertions freeze the existing Fluid policy.
#include "arch/parakeet/decoder.h"
#include "arch/parakeet/parakeet.h"
#include "transcribe-arch.h"
#include "transcribe-backend.h"
#include "transcribe/parakeet.h"

#include <cstdio>
#include <vector>

namespace transcribe::parakeet {
extern const Arch arch;
}

namespace {
using namespace transcribe::parakeet;
int failures = 0;
#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

ggml_tensor * tensor(ggml_context * ctx, int columns, int rows = 1) {
    return ggml_new_tensor_2d(ctx, GGML_TYPE_F32, columns, rows);
}

bool make_weights(HostDecoderWeights & w, int token, int duration) {
    constexpr int H     = 4;
    constexpr int vocab = 3;
    auto &        p     = w.predictor;
    auto &        j     = w.joint;
    w.blank_id          = 2;
    w.n_vocab           = 2;
    w.tdt_durations     = { 0, 1, 2, 3, 4 };
    p.pred_hidden       = H;
    p.pred_vocab        = vocab;
    p.embed_w.assign(vocab * H, 0.0f);
    p.lstm.resize(1);
    j.d_enc       = H;
    j.pred_hidden = H;
    j.joint_h     = H;
    j.joint_n     = vocab + 5;
    j.activation  = "relu";

    const ggml_init_params params = { ggml_tensor_overhead() * 16, nullptr, true };
    p.lstm_w_ctx                  = ggml_init(params);
    j.w_ctx                       = ggml_init(params);
    p.lstm_w_backend              = ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_CPU, nullptr);
    j.w_backend                   = ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_CPU, nullptr);
    if (!p.lstm_w_ctx || !j.w_ctx || !p.lstm_w_backend || !j.w_backend) {
        return false;
    }
    auto & layer = p.lstm[0];
    layer.g_Wx   = tensor(p.lstm_w_ctx, H, 4 * H);
    layer.g_Wh   = tensor(p.lstm_w_ctx, H, 4 * H);
    layer.g_b    = tensor(p.lstm_w_ctx, 4 * H);
    j.g_enc_w    = tensor(j.w_ctx, H, H);
    j.g_enc_b    = tensor(j.w_ctx, H);
    j.g_pred_w   = tensor(j.w_ctx, H, H);
    j.g_pred_b   = tensor(j.w_ctx, H);
    j.gw_w       = tensor(j.w_ctx, H, j.joint_n);
    j.gw_b       = tensor(j.w_ctx, j.joint_n);
    p.lstm_w_buf = ggml_backend_alloc_ctx_tensors(p.lstm_w_ctx, p.lstm_w_backend);
    j.w_buf      = ggml_backend_alloc_ctx_tensors(j.w_ctx, j.w_backend);
    if (!p.lstm_w_buf || !j.w_buf) {
        return false;
    }
    ggml_backend_buffer_clear(p.lstm_w_buf, 0);
    ggml_backend_buffer_clear(j.w_buf, 0);
    std::vector<float> bias(static_cast<size_t>(j.joint_n), -10.0f);
    bias[static_cast<size_t>(token)]            = 10.0f;
    bias[static_cast<size_t>(vocab + duration)] = 10.0f;
    ggml_backend_tensor_set(j.gw_b, bias.data(), 0, bias.size() * sizeof(float));
    p.lstm_ready = true;
    j.w_ready    = true;
    return true;  // HostPredictor/HostJoint own and destroy every allocation.
}

void check_case(int token, int duration, int frames, bool tail, size_t count) {
    HostDecoderWeights w;
    CHECK(make_weights(w, token, duration));
    if (!w.predictor.lstm_ready || !w.joint.w_ready) {
        return;
    }
    const std::vector<float> encoder(static_cast<size_t>(frames * 4), 0.0f);
    std::vector<TdtToken>    out;
    CHECK(decode_tdt_fluid_window(w, encoder.data(), frames, 4, 1, tail, out) == TRANSCRIBE_OK);
    CHECK(out.size() == count);
    for (const auto & emitted : out) {
        CHECK(emitted.id == token);
        CHECK(emitted.step_at_emit >= 0 && emitted.step_at_emit < frames);
    }
    std::vector<TdtToken> again;
    CHECK(decode_tdt_fluid_window(w, encoder.data(), frames, 4, 1, tail, again) == TRANSCRIBE_OK);
    CHECK(again.size() == out.size());
    for (size_t i = 0; i < out.size() && i < again.size(); ++i) {
        CHECK(again[i].id == out[i].id && again[i].step_at_emit == out[i].step_at_emit);
        CHECK(again[i].duration_frames == out[i].duration_frames);
    }
    if (token == 0 && duration == 0 && !tail && frames == 4 && out.size() == 7) {
        const int expected[] = { 0, 0, 1, 1, 2, 2, 3 };
        for (size_t i = 0; i < out.size(); ++i) {
            CHECK(out[i].step_at_emit == expected[i]);
        }
    }
    if (token == 0 && duration == 1 && !tail && frames == 4 && out.size() == 3) {
        for (size_t i = 0; i < out.size(); ++i) {
            CHECK(out[i].step_at_emit == static_cast<int>(i));
        }
    }
}

void check_model_gates() {
    ParakeetModel model;
    model.arch                       = &arch;
    model.variant                    = "tdt-0.6b-v2";
    model.host_decoder.blank_id      = 1024;
    model.host_decoder.tdt_durations = { 0, 1, 2, 3, 4 };
    const auto accepts               = [&]() {
        return transcribe_model_accepts_ext_kind(&model, TRANSCRIBE_EXT_SLOT_RUN,
                                                 TRANSCRIBE_EXT_KIND_PARAKEET_TDT_WINDOW);
    };
    CHECK(accepts());
    CHECK(!transcribe_model_accepts_ext_kind(&model, TRANSCRIBE_EXT_SLOT_STREAM,
                                             TRANSCRIBE_EXT_KIND_PARAKEET_TDT_WINDOW));
    model.variant = "tdt-0.6b-v3";
    CHECK(!accepts());  // v3 cannot reuse the v2 blank projection.
    model.host_decoder.blank_id = 8192;
    CHECK(accepts());
    model.host_decoder.head_kind = HostHeadKind::RNNT;
    CHECK(!accepts());
    model.host_decoder.head_kind        = HostHeadKind::TDT;
    model.host_decoder.tdt_durations[4] = 5;
    CHECK(!accepts());
    model.host_decoder.tdt_durations[4] = 4;
    model.variant                       = "unreviewed-tdt";
    CHECK(!accepts());
    model.variant = "tdt-0.6b-v3";
    ParakeetSession session;
    session.model = &model;
    transcribe_parakeet_tdt_window_ext ext;
    transcribe_parakeet_tdt_window_ext_init(&ext);
    transcribe_run_params params;
    transcribe_run_params_init(&params);
    params.family = &ext.ext;
    CHECK(arch.run_validate(&session, &params) == TRANSCRIBE_ERR_INVALID_ARG);  // Zero range.
    ext.decode_end_frame = 1;
    CHECK(arch.run_validate(&session, &params) == TRANSCRIBE_OK);
    ext.decode_start_frame = -1;
    CHECK(arch.run_validate(&session, &params) == TRANSCRIBE_ERR_INVALID_ARG);
    ext.decode_start_frame      = 0;
    ext.timestamp_offset_frames = -1;
    CHECK(arch.run_validate(&session, &params) == TRANSCRIBE_ERR_INVALID_ARG);
    ext.timestamp_offset_frames = 0;
    ext.ext.size                = sizeof(transcribe_ext);
    CHECK(arch.run_validate(&session, &params) == TRANSCRIBE_ERR_BAD_STRUCT_SIZE);
    ext.ext.size   = sizeof(ext);
    params.diarize = TRANSCRIBE_DIARIZE_MODE_ON;
    CHECK(arch.run_validate(&session, &params) == TRANSCRIBE_ERR_INVALID_ARG);
    params.family = nullptr;
    CHECK(arch.run_validate(&session, &params) == TRANSCRIBE_OK);  // Ordinary Batch unchanged.
}
}  // namespace

int main() {
    check_model_gates();
    check_case(2, 0, 4, false, 0);      // Blank zero-duration must advance.
    check_case(2, 0, 4, true, 0);       // Tail silence terminates after bounded blanks.
    check_case(0, 0, 4, false, 7);      // Repeated zero-duration token forces advance.
    check_case(0, 1, 4, false, 3);      // Emission check occurs after duration advance.
    check_case(0, 4, 4, false, 0);      // Crossing content boundary drops the token.
    check_case(0, 4, 4, true, 10);      // Explicit final drain stays bounded.
    check_case(0, 1, 200, false, 150);  // Main-window token budget.
    check_case(0, 1, 200, true, 160);   // Separate ten-token tail budget.
    HostDecoderWeights    invalid;
    const float           encoder[4] = {};
    std::vector<TdtToken> out;
    CHECK(decode_tdt_fluid_window(invalid, nullptr, 1, 4, 1, false, out) == TRANSCRIBE_ERR_INVALID_ARG);
    invalid.joint.d_enc   = 4;
    invalid.tdt_durations = { 0, 1, 2, 3, 4 };
    CHECK(decode_tdt_fluid_window(invalid, encoder, 1, 4, 1, false, out) == TRANSCRIBE_ERR_BACKEND);
    CHECK(out.empty());  // Failed graph setup emits nothing.
    return failures == 0 ? 0 : 1;
}
