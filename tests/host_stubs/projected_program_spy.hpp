#ifndef SATURN_TESTS_PROJECTED_PROGRAM_SPY_HPP
#define SATURN_TESTS_PROJECTED_PROGRAM_SPY_HPP

#include "src/core/runtime/state.hpp"
#include "src/graphics/vdp1/projected.hpp"
#include "src/hal/vdp1/vdp1.hpp"

// Scene tests that link faces.cpp without the VDP1 HAL. Compiled program
// faces are written straight into HAL command slots; on commit each written
// record is replayed through the test's draw_projected_sprite spy, carrying
// the CMDSRCA and mesh bit the record holds, so the existing emission checks
// still see every program face in order.
namespace saturn::core {
RuntimeState g_state = [] { RuntimeState s{}; s.initialized = true; return s; }();
}

namespace saturn::hal::vdp1 {
namespace {
Command g_spy_program_slots[256];
}

Command* projected_program_slots(uint32_t* available) {
    *available = sizeof(g_spy_program_slots) / sizeof(g_spy_program_slots[0]);
    return g_spy_program_slots;
}

void commit_projected_programs(uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        const Command& c = g_spy_program_slots[i];
        sat_vdp1_texture_t texture{};
        texture.srca = c.srca;
        texture.valid = 1u;
        const sat_quad2_t quad{{c.xa, c.xb, c.xc, c.xd}, {c.ya, c.yb, c.yc, c.yd}};
        (void)saturn::internal::draw_projected_sprite(quad, texture,
            (c.pmod & 0x0100u) ? SAT_SPRITE_FLAG_MESH : 0u);
    }
}
}

#endif
