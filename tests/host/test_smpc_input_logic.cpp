#include <cstdio>
#include <cstdlib>

#include "src/hal/smpc/input_logic.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

int main() {
    using namespace saturn::hal::smpc;

    /* Buttons are active low: a cleared bit is a pressed button. */
    OK(translate_standard_pad(0xFFu, 0xFFu) == 0u);
    const uint8_t d1 = static_cast<uint8_t>(0xFFu & ~0x80u & ~0x04u);
    OK(translate_standard_pad(d1, 0xFFu) == (SAT_PAD_RIGHT | SAT_PAD_A));
    OK(translate_standard_pad(0x00u, 0xFFu) ==
       (SAT_PAD_RIGHT | SAT_PAD_LEFT | SAT_PAD_DOWN | SAT_PAD_UP | SAT_PAD_START |
        SAT_PAD_A | SAT_PAD_B | SAT_PAD_C));
    OK(translate_standard_pad(0xFFu, 0x00u) ==
       (SAT_PAD_R | SAT_PAD_X | SAT_PAD_Y | SAT_PAD_Z | SAT_PAD_L));
    /* The three low bits of the second byte are not buttons. */
    OK(translate_standard_pad(0xFFu, 0xF8u) == 0u);

    std::puts("smpc input logic: OK");
    return 0;
}
