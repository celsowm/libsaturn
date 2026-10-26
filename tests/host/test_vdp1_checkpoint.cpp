#include <cassert>
#include <cstdio>
#include "src/hal/vdp1/vdp1.hpp"
#include "src/hal/scu/scu.hpp"

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
    // Disjoint physical preparation, painter traversal, unused holes and HUD.
    assert(vd::reserve_overlay_commands(2u)==SAT_OK);
    sat_vdp1_command_batch_t batch{};
    assert(vd::command_batch_reserve(4u,&batch)==SAT_OK);
    assert(vd::command_batch_pending());
    assert(vd::begin_overlay_pass()==SAT_ERR_BUSY);
    assert(vd::command_rollback(&after_reservation)==SAT_ERR_BUSY);
    const auto serial=batch.checkpoint.frame_serial;
    assert(vd::command_batch_link(&batch,3u)==SAT_ERR_BUSY);
    batch.commands[0]={};batch.commands[0].ctrl=4u;batch.commands[0].colr=0x801fu;batch.commands[0].pad=1u;
    batch.commands[2]={};batch.commands[2].ctrl=4u;batch.commands[2].colr=0x83e0u;batch.commands[2].pad=1u;
    assert(vd::push_polygon(p)==SAT_OK); // Cold fallback at absolute index 6.
    assert(vd::command_batch_link(&batch,4u)==SAT_OK);
    assert(vd::command_batch_link(&batch,6u)==SAT_OK);
    assert(vd::command_batch_link(&batch,2u)==SAT_OK);
    assert(vd::command_batch_link(&batch,2u)==SAT_ERR_BUSY); // already consumed
    assert(vd::command_batch_finish(&batch)==SAT_OK);
    assert(!vd::command_batch_pending());
    assert((commands[1].ctrl&0x7000u)==0x1000u && commands[1].link==4u*4u);
    assert(commands[4].link==6u*4u && commands[6].link==2u*4u && commands[2].link==7u*4u);
    assert(vd::begin_overlay_pass()==SAT_OK);
    assert(vd::push_polygon(p)==SAT_OK);
    assert(vd::command_batch_link(&batch,7u)==SAT_ERR_INVALID_ARG);
    vd::begin_frame(commands,12u);
    assert(serial!=batch.checkpoint.frame_serial+1u || !batch.active);
    assert(vd::command_batch_finish(&batch)==SAT_ERR_INVALID_ARG);
    assert(vd::reserve_overlay_commands(3u)==SAT_OK);
    assert(vd::command_batch_reserve(7u,&batch)==SAT_ERR_CAPACITY);
    assert(vd::command_batch_reserve(5u,&batch)==SAT_OK);
    assert(vd::command_batch_cancel(&batch)==SAT_OK);
    vd::command_stats(used,cap,hud,overlay);
    assert(used==2u && hud==3u && !overlay);
    assert(vd::command_batch_reserve(1u,&batch)==SAT_OK);
    assert(vd::command_batch_finish(&batch)==SAT_OK); // empty scene skips span
    assert(commands[1].link==3u*4u);
    std::puts("VDP1 staged command checkpoint: OK");
}
