#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "saturn/audio.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

static int16_t s16(const uint8_t* p) { return static_cast<int16_t>((p[0] << 8) | p[1]); }

int main() {
    static_assert(SAT_AUDIO_IMA_BLOCK_BYTES(2) == 1032, "stereo block");
    static_assert(SAT_AUDIO_IMA_BLOCK_BYTES(1) == 516, "mono block");
    uint8_t block[1032];
    uint8_t out[2 * 2048];
    std::memset(block, 0, sizeof block);
    /* Left: first sample 1000, step index 0 (step 7); right: first sample -2000, step index 10 (step 19). */
    block[0] = 0x03; block[1] = 0xE8; block[2] = 0;
    block[4] = 0xF8; block[5] = 0x30; block[6] = 10;
    /* Frame 1: left code 4 (+step + step/8), right code 0xC (negative, +step/8 only... sign bit 8, magnitude 4). */
    block[8] = static_cast<uint8_t>((4u << 4) | 0xCu);
    OK(sat_audio_ima_decode_block(block, 2, out) == SAT_OK);
    OK(s16(out) == 1000 && s16(out + 2048) == -2000);
    /* Left: step 7, code 4: 7 >> 3 = 0, plus 7 = +7 -> 1007; the index moves by +2. */
    OK(s16(out + 2) == 1007);
    /* Right: step 19, code 0xC: sign set, magnitude code 4: 19 >> 3 = 2, plus 19 = 21 -> -2021. */
    OK(s16(out + 2048 + 2) == -2021);
    /* Frame 2 has code 0 on both: left step is now 9 (index 2): 9 >> 3 = 1 -> 1008. */
    OK(s16(out + 4) == 1008);
    OK(sat_audio_ima_decode_block(nullptr, 2, out) == SAT_ERR_INVALID_ARG);
    OK(sat_audio_ima_decode_block(block, 3, out) == SAT_ERR_INVALID_ARG);
    block[2] = 89;
    OK(sat_audio_ima_decode_block(block, 2, out) == SAT_ERR_INVALID_ARG);
    std::puts("test_ima_block: OK");
    return 0;
}
