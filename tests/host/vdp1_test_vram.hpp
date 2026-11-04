#ifndef LIBSATURN_TESTS_VDP1_TEST_VRAM_HPP
#define LIBSATURN_TESTS_VDP1_TEST_VRAM_HPP

/* VDP1 VRAM for host tests: a 512 KiB array the HAL writes its command banks into, read back through
 * frame_commands() and frame_command_link(). Call use_test_vram() first in main(). */
#include <cstdint>
#include <vector>
#include "src/hal/vdp1/vdp1.hpp"

inline void use_test_vram() {
    alignas(4) static uint8_t vram[512u * 1024u];
    saturn::hal::vdp1::set_vram_window(vram);
}

/* A copy of the first `count` commands of the frame in progress: the bank is rewritten by the next begin_frame. */
inline std::vector<saturn::hal::vdp1::Command> frame_snapshot(uint32_t count) {
    const saturn::hal::vdp1::Command* commands = saturn::hal::vdp1::frame_commands();
    return std::vector<saturn::hal::vdp1::Command>(commands, commands + count);
}

#endif
