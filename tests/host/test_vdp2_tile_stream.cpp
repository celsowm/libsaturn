#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "saturn/stage_map2.h"
#include "saturn/vdp2_layers.h"
#include "src/hal/vdp2/nbg_logic.hpp"

namespace {

using saturn::hal::vdp2::nbg::Layer;
using saturn::hal::vdp2::nbg::Status;

Layer one_plane(uint32_t base) {
    Layer l{};
    l.enabled = true;
    l.colors = saturn::hal::vdp2::nbg::Colors::C16;
    l.pn_one_word = true;
    l.char_2x2 = false;
    l.pages_x = 1;
    l.pages_y = 1;
    l.priority = 4;
    l.transparent = true;
    l.char_banks = SAT_VDP2_BANK_B0;
    for (uint8_t p = 0; p < 4; ++p) l.plane_address[p] = base;
    return l;
}

void retarget_moves_all_four_planes() {
    Layer l = one_plane(0x00000u);
    assert(saturn::hal::vdp2::nbg::retarget_plane_base(l, 0x04000u) == Status::Ok);
    for (uint8_t p = 0; p < 4; ++p) assert(l.plane_address[p] == 0x04000u);
    /* The map register value is the base in 0x2000-byte page units. */
    Layer layers[4] = {l, {}, {}, {}};
    const auto regs = saturn::hal::vdp2::nbg::compose(layers);
    assert((regs.mpab[0] & 0x003Fu) == 2u);
    assert((regs.mpcd[0] & 0x003Fu) == 2u);
    assert((regs.mpofn & 0x000Fu) == 0u);
}

void retarget_refuses_illegal_rings() {
    Layer l = one_plane(0x00000u);
    assert(saturn::hal::vdp2::nbg::retarget_plane_base(l, 0x01000u) == Status::BadPlane);
    assert(saturn::hal::vdp2::nbg::retarget_plane_base(l, 0x80000u) == Status::BadPlane);
    assert(saturn::hal::vdp2::nbg::retarget_plane_base(l, 0x7F000u) == Status::BadPlane);

    Layer multi = one_plane(0x00000u);
    multi.plane_address[2] = 0x02000u;
    assert(saturn::hal::vdp2::nbg::retarget_plane_base(multi, 0x04000u) == Status::BadFormat);

    Layer bitmap = one_plane(0x00000u);
    bitmap.bitmap = true;
    assert(saturn::hal::vdp2::nbg::retarget_plane_base(bitmap, 0x20000u) == Status::BadFormat);

    /* 2x2 characters use an 0x800-byte page; 0x1000 is a legal page there. */
    Layer big = one_plane(0x00000u);
    big.char_2x2 = true;
    assert(saturn::hal::vdp2::nbg::retarget_plane_base(big, 0x01000u) == Status::Ok);
    assert(big.plane_address[0] == 0x01000u);
}

/* The double-buffer sequence the example uses: two rings, stage into the
 * inactive one, commit, flip. The active ring must always match the view it
 * was published for, and the inactive ring catches up before the next flip. */
void double_buffer_sequence_keeps_the_active_ring_complete() {
    constexpr uint32_t kRingWords = 32u * 32u;
    uint16_t map[32u * 32u];
    for (uint32_t y = 0; y < 32u; ++y)
        for (uint32_t x = 0; x < 32u; ++x) map[y * 32u + x] = static_cast<uint16_t>((x + y) & 3u);
    const uint16_t cells[4] = {0x0001u, 0x0002u, 0x0003u, 0x0004u};
    sat_stage_map2_tileset_t ts = {cells, 4u, 0u, 0u};
    sat_stage_map2_layer_desc_t descs[2] = {};
    for (uint32_t i = 0; i < 2u; ++i) {
        descs[i].map = map;
        descs[i].map_w = 32u;
        descs[i].map_h = 32u;
        descs[i].fill_word = 0u;
        descs[i].outside = SAT_STAGE_MAP2_OUTSIDE_WRAP;
        descs[i].ratio_x = SAT_FX16_ONE;
        descs[i].ratio_y = SAT_FX16_ONE;
        descs[i].ring.cell_shift = 3u;
        descs[i].ring.page_shift = 5u;
        descs[i].ring.pages_x = 1u;
        descs[i].ring.pages_y = 1u;
        descs[i].ring.page_word[0] = static_cast<uint32_t>(i) * kRingWords;
    }
    sat_stage_map2_config_t config = {};
    config.tileset = &ts;
    config.layers = descs;
    config.layer_count = 1u;
    config.viewport_w = 64u;
    config.viewport_h = 64u;
    std::vector<uint16_t> vram[2];
    std::vector<uint16_t> words[2];
    std::vector<sat_stage_map2_run_t> runs[2];
    sat_stage_map2_storage_t storage[2];
    sat_stage_map2_t stream[2];
    for (uint32_t i = 0; i < 2u; ++i) {
        vram[i].assign(kRingWords, 0xFFFFu);
        words[i].assign(4u * 32u * 32u, 0u);
        runs[i].assign(32u * 32u, {});
        storage[i] = {words[i].data(), static_cast<uint32_t>(words[i].size()),
                      runs[i].data(), static_cast<uint32_t>(runs[i].size())};
        descs[i].vdp2_layer = 0u;
        assert(sat_stage_map2_init(&stream[i], &config, &storage[i]) == SAT_OK);
        assert(sat_stage_map2_set_view(&stream[i], 0, 0) == SAT_OK);
        assert(sat_stage_map2_commit(&stream[i], [](void* user, uint32_t off, const uint16_t* w,
                                                    uint32_t n) {
            auto& v = *static_cast<std::vector<uint16_t>*>(user);
            for (uint32_t k = 0; k < n; ++k) v[off + k] = w[k];
            return SAT_OK;
        }, &vram[i]) == SAT_OK);
    }
    uint8_t active = 0u;
    for (uint32_t frame = 1; frame <= 200u; ++frame) {
        const uint8_t next = static_cast<uint8_t>(active ^ 1u);
        assert(sat_stage_map2_set_view(&stream[next], 0, static_cast<sat_fx16_t>(frame * 8u * SAT_FX16_ONE)) == SAT_OK);
        assert(sat_stage_map2_commit(&stream[next], [](void* user, uint32_t off, const uint16_t* w,
                                                       uint32_t n) {
            auto& v = *static_cast<std::vector<uint16_t>*>(user);
            for (uint32_t k = 0; k < n; ++k) v[off + k] = w[k];
            return SAT_OK;
        }, &vram[next]) == SAT_OK);
        const auto& l = stream[next].layer[0];
        for (int32_t cy = l.y0; cy < l.y0 + l.rows; ++cy) {
            const uint32_t ry = static_cast<uint32_t>(cy) & 31u;
            for (int32_t cx = l.x0; cx < l.x0 + l.cols; ++cx) {
                const uint32_t rx = static_cast<uint32_t>(cx) & 31u;
                const uint16_t want = sat_stage_map2_cell(&stream[next], 0, cx, cy);
                assert(vram[next][ry * 32u + rx] == want);
            }
        }
        active = next;
    }
    assert(sat_stage_map2_stats(&stream[active]).cells_committed > 0u);
}

}  // namespace

int main() {
    retarget_moves_all_four_planes();
    retarget_refuses_illegal_rings();
    double_buffer_sequence_keeps_the_active_ring_complete();
    std::puts("PASS: test_vdp2_tile_stream.cpp");
    return 0;
}
