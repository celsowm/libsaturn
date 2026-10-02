#include "saturn/stage_map2.h"

/* The only hardware-facing part of the stage map: the commit writer and the scroll hand-off. */

namespace {

sat_result_t vdp2_write(void*, uint32_t word_offset, const uint16_t* words, uint32_t count) {
    return sat_vdp2_vram_write_words(word_offset, words, count);
}

}  // namespace

extern "C" sat_result_t sat_stage_map2_commit_vdp2(sat_stage_map2_t* map) {
    return sat_stage_map2_commit(map, vdp2_write, nullptr);
}

extern "C" sat_result_t sat_stage_map2_apply_scroll_vdp2(const sat_stage_map2_t* map) {
    if (!map) return SAT_ERR_INVALID_ARG;
    for (uint8_t i = 0; i < map->layer_count; ++i) {
        sat_fx16_t x = 0, y = 0;
        sat_result_t r = sat_stage_map2_scroll(map, i, &x, &y);
        if (r != SAT_OK) return r;
        r = sat_vdp2_layer_set_scroll(static_cast<sat_vdp2_layer_t>(map->layer[i].desc.vdp2_layer), x, y);
        if (r != SAT_OK) return r;
    }
    return SAT_OK;
}
