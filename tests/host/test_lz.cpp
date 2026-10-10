#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "saturn/lz.h"
#include "src/storage/compression/lz_stream.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace {

/* tools/satlz.py of: "Saturn " x 20, bytes 0..63, 300 zero bytes, "end" (507 bytes). It has a
 * 15+ literal run, a long overlapping match (offset 7), and an offset-1 run. */
const uint8_t kPacked[] = {
    0x53,0x4c,0x5a,0x31,0x00,0x00,0x01,0xfb,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x56,0x7f,0x53,0x61,0x74,
    0x75,0x72,0x6e,0x20,0x00,0x07,0x72,0xff,0x32,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,
    0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,
    0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x31,0x32,
    0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,0x00,0x00,0x01,0xff,0x19,0x30,0x65,
    0x6e,0x64,
};
constexpr uint32_t kDecoded = 507u;

void expected(uint8_t* out) {
    uint32_t n = 0u;
    for (int i = 0; i < 20; ++i) {
        std::memcpy(out + n, "Saturn ", 7u);
        n += 7u;
    }
    for (uint8_t b = 0u; b < 64u; ++b) out[n++] = b;
    std::memset(out + n, 0, 300u);
    n += 300u;
    std::memcpy(out + n, "end", 3u);
}

}  // namespace

int main() {
    uint8_t want[kDecoded];
    expected(want);

    sat_lz_header_t h{};
    OK(sat_lz_read_header(kPacked, &h) == SAT_OK);
    OK(h.decoded == kDecoded && h.payload == sizeof(kPacked) - SAT_LZ_HEADER_BYTES);

    // Separate buffers.
    uint8_t out[kDecoded + 16u];
    std::memset(out, 0xEE, sizeof(out));
    uint32_t got = 1u;
    OK(sat_lz_decode(kPacked, sizeof(kPacked), out, sizeof(out), &got) == SAT_OK);
    OK(got == kDecoded && std::memcmp(out, want, kDecoded) == 0 && out[kDecoded] == 0xEEu);

    // In place: the file at the (4-aligned) end of decoded + margin bytes, as sat_load_lz does it.
    alignas(4) uint8_t buffer[kDecoded + 64u];
    const uint32_t capacity = kDecoded + h.margin;
    OK(capacity <= sizeof(buffer));
    uint8_t* packed = buffer + ((capacity - sizeof(kPacked)) & ~3u);
    std::memcpy(packed, kPacked, sizeof(kPacked));
    OK(sat_lz_decode(packed, sizeof(kPacked), buffer, kDecoded, &got) == SAT_OK);
    OK(got == kDecoded && std::memcmp(buffer, want, kDecoded) == 0);

    // Streaming, as sat_load_lz decodes while sectors arrive: in place, the file showing up one
    // byte at a time; every step decodes only what lies below the arrived end.
    {
        namespace lz = saturn::storage::lz;
        alignas(4) uint8_t arena[kDecoded + 64u];
        std::memset(arena, 0xEE, sizeof(arena));
        uint8_t* file = arena + ((capacity - sizeof(kPacked)) & ~3u);
        lz::Stream s{};
        std::memcpy(file, kPacked, SAT_LZ_HEADER_BYTES);
        OK(lz::begin(s, file, sizeof(kPacked), arena, kDecoded) == SAT_OK);
        uint32_t steps_with_output = 0u;
        for (uint32_t n = SAT_LZ_HEADER_BYTES; n < sizeof(kPacked); ++n) {
            OK(!lz::done(s));
            file[n] = kPacked[n];  // byte n arrives; nothing past it is there yet
            if (n + 1u < sizeof(kPacked)) file[n + 1u] = 0x5Au;
            uint8_t* before = s.out;
            OK(lz::step(s, file + n + 1u) == SAT_OK);
            OK(s.in <= file + n + 1u);
            steps_with_output += s.out != before;
        }
        OK(lz::done(s) && s.out == arena + kDecoded && std::memcmp(arena, want, kDecoded) == 0);
        OK(steps_with_output >= 3u);  // it really decoded behind the arrival, not at the end

        // A file shorter than its header's payload is refused; a step short of the end waits.
        lz::Stream t{};
        OK(lz::begin(t, kPacked, sizeof(kPacked) - 1u, out, sizeof(out)) == SAT_ERR_IO);
        OK(lz::begin(t, kPacked, sizeof(kPacked), out, sizeof(out)) == SAT_OK);
        OK(lz::step(t, kPacked + 40) == SAT_OK && !lz::done(t));
        OK(lz::step(t, kPacked + sizeof(kPacked)) == SAT_OK && lz::done(t));
        OK(std::memcmp(out, want, kDecoded) == 0);
    }

    // Refusals: no room, not SLZ1, truncated payload, a match before the output start.
    OK(sat_lz_decode(kPacked, sizeof(kPacked), out, kDecoded - 1u, &got) == SAT_ERR_CAPACITY && got == 0u);
    uint8_t bad[sizeof(kPacked)];
    std::memcpy(bad, kPacked, sizeof(bad));
    bad[3] = '2';
    OK(sat_lz_read_header(bad, &h) == SAT_ERR_VERSION);
    OK(sat_lz_decode(bad, sizeof(bad), out, sizeof(out), &got) == SAT_ERR_VERSION);
    OK(sat_lz_decode(kPacked, sizeof(kPacked) - 1u, out, sizeof(out), &got) == SAT_ERR_IO);
    OK(sat_lz_decode(kPacked, 8u, out, sizeof(out), &got) == SAT_ERR_INVALID_ARG);
    std::memcpy(bad, kPacked, sizeof(bad));
    bad[16 + 1 + 7] = 0x40;  // first match offset 7 -> 0x4007 (token, 7 literals, offset): before the start
    OK(sat_lz_decode(bad, sizeof(bad), out, sizeof(out), &got) == SAT_ERR_IO);
    std::memcpy(bad, kPacked, sizeof(bad));
    bad[7] = 0xFAu;  // header claims one byte less than the sequences produce
    OK(sat_lz_decode(bad, sizeof(bad), out, sizeof(out), &got) == SAT_ERR_IO);
    bad[7] = 0xFCu;  // ... one byte more
    OK(sat_lz_decode(bad, sizeof(bad), out, sizeof(out), &got) == SAT_ERR_IO);

    std::puts("lz: OK");
    return 0;
}
