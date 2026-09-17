#include <cstdio>
#include <cstdlib>

#include "src/hal/smpc_input_logic.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

int main() {
    using namespace saturn::hal::smpc;

    OK(intback_mode_for_port(0u) == 0xCAu);
    OK(intback_mode_for_port(1u) == 0x3Au);
    OK(intback_mode_for_port(2u) == 0u);

    const uint8_t d1 = static_cast<uint8_t>(0xFFu & ~0x80u & ~0x04u);
    DigitalPadSample sample = decode_direct_digital_pad(0xF1u, 0x02u, d1, 0xFFu);
    OK(sample.connected);
    OK(sample.held == (SAT_PAD_RIGHT | SAT_PAD_A));

    sample = decode_direct_digital_pad(0xF0u, 0x02u, d1, 0xFFu);
    OK(!sample.connected && sample.held == 0u);

    sample = decode_direct_digital_pad(0xF1u, 0x12u, d1, 0xFFu);
    OK(!sample.connected && sample.held == 0u);

    sample = decode_direct_digital_pad(0xF1u, 0x01u, d1, 0xFFu);
    OK(!sample.connected && sample.held == 0u);

    std::puts("smpc input logic: OK");
    return 0;
}
