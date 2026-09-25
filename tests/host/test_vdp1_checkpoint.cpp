#include <cassert>
#include <cstdio>
#include "src/hal/vdp1/vdp1.hpp"
#include "src/hal/scu/scu.hpp"

// wait_draw_end's frame clock; these tests never submit.
namespace saturn::hal::scu {
uint16_t ticks_per_frame() { return 0u; }
uint64_t elapsed_ticks() { return 0u; }
}

int main() {
    namespace vd=saturn::hal::vdp1;
    vd::Command commands[12]{};
    vd::begin_frame(commands,12u);
    sat_vdp1_command_checkpoint_t first{};
    assert(vd::command_checkpoint(&first)==SAT_OK);
    assert(first.used==2u && first.gouraud_tables==0u);
    vd::PolygonRequest p{};
    p.color=0x801Fu;
    assert(vd::push_polygon(p)==SAT_OK);
    assert(vd::push_polygon(p)==SAT_OK);
    uint16_t used=0u,cap=0u,hud=0u;bool overlay=false;
    vd::command_stats(used,cap,hud,overlay);
    assert(used==4u && cap==12u);
    assert(vd::command_rollback(&first)==SAT_OK);
    vd::command_stats(used,cap,hud,overlay);
    assert(used==2u && hud==0u);
    assert(vd::reserve_overlay_commands(3u)==SAT_OK);
    assert(vd::command_rollback(&first)==SAT_ERR_INVALID_ARG);
    sat_vdp1_command_checkpoint_t after_reservation{};
    assert(vd::command_checkpoint(&after_reservation)==SAT_OK);
    assert(after_reservation.overlay_reserved==3u);
    uint16_t painted=0u;
    while(vd::push_polygon(p)==SAT_OK)++painted;
    assert(painted==6u); // 12 total - 2 setup - 3 HUD - 1 END.
    vd::command_stats(used,cap,hud,overlay);
    assert(used==8u && hud==3u);
    assert(vd::command_rollback(&after_reservation)==SAT_OK);
    vd::command_stats(used,cap,hud,overlay);
    assert(used==2u && hud==3u);
    assert(vd::push_polygon(p)==SAT_OK);
    vd::begin_frame(commands,12u);
    assert(vd::command_rollback(&after_reservation)==SAT_ERR_INVALID_ARG);
    std::puts("VDP1 staged command checkpoint: OK");
}
