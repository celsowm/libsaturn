#include <cassert>
#include <iostream>
#include "examples/skybridge_3d/game.h"
#include "src/core/fade3d_logic.hpp"
#include "src/core/vdp1_color_calc_logic.hpp"
#include "src/core/vdp2_color_calc_logic.hpp"

/* These are production pure helpers, not a separately reimplemented fade. */
int main() {
    const sat_fade3d_t fade={
        SB_FADE_START,SB_FADE_END,8u,SAT_FADE3D_CULL_AFTER_END,0u
    };
    sat_fade3d_result_t state={};
    for (int d=1;d<=66;++d) {
        assert(saturn::core::fade3d::eval(&fade,SB_F(d),&state)==SAT_OK);
        assert(state.level==0u && state.culled==0u);
    }
    uint8_t prev=0u, observed=0u;
    for (int d=67;d<134;++d) {
        assert(saturn::core::fade3d::eval(&fade,SB_F(d),&state)==SAT_OK);
        assert(state.level>=prev && state.level<8u && state.culled==0u);
        observed=(uint8_t)(observed|(1u<<state.level));
        prev=state.level;
    }
    assert(observed==0xFFu); /* All 8 quantized ratios are reachable. */
    assert(saturn::core::fade3d::eval(&fade,SB_F(134),&state)==SAT_OK);
    assert(state.culled==1u); /* Cull only after nearly transparent slot 7. */

    /* The example's actual configuration, now that quantization, the
       level-to-slot mapping and the anti-flicker band are all library-side.
       Only "the deck under the player stays opaque" is left in the game. */
    sat_fade3d_slots_t slots={};
    slots.policy=fade;
    slots.hysteresis=SB_F(2);
    slots.base_slot=0u;
    slots.slot_stride=1u;
    slots.opaque_before_start=1u;
    uint8_t resolved=0u;
    assert(saturn::core::fade3d::slot(
        &slots,SB_F(30),nullptr,&resolved)==SAT_OK);
    assert(resolved==SAT_INDEXED_SOLID_OPAQUE); /* Nearer than FADE_START. */
    assert(saturn::core::fade3d::slot(
        &slots,SB_F(134),nullptr,&resolved)==SAT_OK);
    assert(resolved==SAT_FADE3D_SLOT_CULLED);
    for(int d=67;d<134;++d) {
        assert(saturn::core::fade3d::slot(
            &slots,SB_F(d),nullptr,&resolved)==SAT_OK);
        assert(resolved<8u); /* Every drawn deck lands on a real slot. */
    }

    /* A deck must not flicker between two slots while the chase camera eases
       across a transition, which is the whole point of carrying per-deck
       state. Slot 3 spans depths [92,100) for this policy. */
    uint8_t deck=SAT_INDEXED_SOLID_OPAQUE;
    assert(saturn::core::fade3d::slot(&slots,SB_F(96),&deck,&resolved)==SAT_OK);
    const uint8_t settled=resolved;
    for(int sweep=0;sweep<3;++sweep) {
        for(int d=99;d<=101;++d) {
            assert(saturn::core::fade3d::slot(
                &slots,SB_F(d),&deck,&resolved)==SAT_OK);
            assert(resolved==settled);
        }
        for(int d=101;d>=99;--d) {
            assert(saturn::core::fade3d::slot(
                &slots,SB_F(d),&deck,&resolved)==SAT_OK);
            assert(resolved==settled);
        }
    }

    /* Sprite selector 0 remains normal/opaque priority 7, selector 1 fades
       at priority 6. RBG0 in Skybridge is priority 5, so remains blendable. */
    assert(saturn::core::vdp2_color_calc::compose_prisa(7u)==0x0607u);
    uint16_t selector=0u;
    for(uint8_t slot=0u;slot<8u;++slot) {
        assert(saturn::core::vdp1_color_calc::encode_palette_selector(
            4u,slot,&selector)==SAT_OK);
        assert(selector==(uint16_t)(0x44u+(slot<<3u)));
        assert((selector&0x40u)!=0u);
    }
    assert(saturn::core::vdp1_color_calc::encode_palette_selector(
        4u,8u,&selector)==SAT_ERR_INVALID_ARG);

    std::cout<<"skybridge distance-fade quantization and hardware selectors: OK\n";
    return 0;
}
