// Exercise the exported two-pass API with split UTF-8 bytes and failing models.
#include "transcribe-model.h"
#include "transcribe-tokenizer.h"

#include <climits>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
int failures = 0;
#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

struct TokenModel final : transcribe_model {
    transcribe::Tokenizer tok;

    const transcribe::Tokenizer * tokenizer() const override { return &tok; }
};

struct ThrowModel final : transcribe_model {
    const transcribe::Tokenizer * tokenizer() const override { throw std::runtime_error("test tokenizer failure"); }
};
}  // namespace

int main() {
    TokenModel model;
    // The Euro sign is deliberately split over three token boundaries.
    CHECK(model.tok.load_decode_only_raw_bytes({ "\xe2", "\x82", "\xac", " hello" }) == TRANSCRIBE_OK);
    const int32_t    ids[]      = { 0, 1, 2, 3 };
    const char       expected[] = "\xe2\x82\xac hello";
    constexpr size_t bytes      = sizeof(expected) - 1;
    char             buffer[32];
    std::memset(buffer, '#', sizeof(buffer));
    CHECK(transcribe_detokenize(&model, ids, 4, nullptr, 0) == -static_cast<int>(bytes));
    CHECK(transcribe_detokenize(&model, ids, 4, buffer, bytes - 1) == -static_cast<int>(bytes));
    CHECK(buffer[0] == '#' && buffer[bytes] == '#');
    CHECK(transcribe_detokenize(&model, ids, 4, buffer, bytes) == static_cast<int>(bytes));
    CHECK(std::memcmp(buffer, expected, bytes) == 0);
    CHECK(buffer[bytes] == '#');  // No implicit NUL, including exact-size buffers.
    CHECK(transcribe_detokenize(&model, nullptr, 0, nullptr, 0) == 0);
    CHECK(transcribe_detokenize(nullptr, ids, 4, buffer, sizeof(buffer)) == INT_MIN);
    CHECK(transcribe_detokenize(&model, nullptr, 1, buffer, sizeof(buffer)) == INT_MIN);
    CHECK(transcribe_detokenize(&model, ids, -1, buffer, sizeof(buffer)) == INT_MIN);
    CHECK(transcribe_detokenize(&model, ids, 4, nullptr, bytes) == INT_MIN);
    transcribe_model no_tokenizer;
    CHECK(transcribe_detokenize(&no_tokenizer, ids, 4, buffer, sizeof(buffer)) == INT_MIN);
    ThrowModel throws;
    std::memset(buffer, '#', sizeof(buffer));
    CHECK(transcribe_detokenize(&throws, ids, 4, buffer, sizeof(buffer)) == INT_MIN);
    CHECK(buffer[0] == '#');  // C++ exceptions stay behind the C boundary.
    // Failure has no persistent state; a subsequent call still succeeds.
    CHECK(transcribe_detokenize(&model, ids, 4, buffer, bytes) == static_cast<int>(bytes));
    return failures == 0 ? 0 : 1;
}
