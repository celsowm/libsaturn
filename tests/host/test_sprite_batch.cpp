#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "src/core/runtime/logic.hpp"
#include "src/hal/vdp1/submit_copy.hpp"
#include "src/hal/vdp1/vdp1.hpp"

#include "saturn/sprite_batch.h"
#include "saturn/vdp1.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

using namespace saturn::hal::vdp1;

namespace {

uint32_t g_rng = 0x1234567u;
uint32_t rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }

/* The command the general path (hal push_sprite, what sat_draw_texture ends in) emits. */
Command reference_command(const sat_sprite_frame_t& f, const sat_sprite_instance_t& in,
                          uint16_t flags, uint16_t format, uint16_t palette) {
    Command buf[8]{};
    begin_frame(buf, 8u);
    SpriteRequest req{};
    req.x = static_cast<int16_t>(in.x - f.pivot_x - 160);
    req.y = static_cast<int16_t>(in.y - f.pivot_y - 112);
    req.width = sat_sprite_frame_width(&f);
    req.height = sat_sprite_frame_height(&f);
    req.srca = f.srca;
    req.palette = palette;
    req.flags = flags;
    req.format = format;
    OK(push_sprite(req) == SAT_OK);
    return buf[2];
}

}  // namespace

int main() {
    /* Frame preparation. */
    sat_sprite_frame_t f{};
    OK(sat_sprite_frame_init_raw(&f, 0x100u, 16u, 16u, SAT_VDP1_TEXTURE_INDEXED8, 3u, 0u, 8, 8) == SAT_OK);
    OK(f.size == ((2u << 8) | 16u) && f.colr == (3u << 8) && f.pmod == 0x00A0u && f.ctrl == 0u);
    OK(sat_sprite_frame_init_raw(&f, 0x100u, 16u, 16u, SAT_VDP1_TEXTURE_LUT4, 0x2345u, 0u, 0, 0) == SAT_OK);
    OK(f.pmod == 0x0088u && f.colr == 0x2345u);
    OK(sat_sprite_frame_init_raw(&f, 0, 12u, 16u, 0u, 0u, 0u, 0, 0) == SAT_ERR_INVALID_ARG);   /* width % 8 */
    OK(sat_sprite_frame_init_raw(&f, 0, 16u, 0u, 0u, 0u, 0u, 0, 0) == SAT_ERR_INVALID_ARG);
    OK(sat_sprite_frame_init_raw(&f, 0, 16u, 16u, 0u, 0u, 0u, 20, -3) == SAT_OK && f.pivot_x == 20 && f.pivot_y == -3);
    OK(sat_sprite_frame_init_raw(&f, 0, 16u, 16u, 7u, 0u, 0u, 0, 0) == SAT_ERR_INVALID_ARG);   /* format */
    /* User clip is the CMDPMOD clip-enable bit (inside mode); the rectangle is a separate command. */
    OK(sat_sprite_frame_init_raw(&f, 0, 16u, 16u, 0u, 0u, SAT_SPRITE_FLAG_USER_CLIP, 0, 0) == SAT_OK);
    OK(f.pmod == 0x04A0u);
    OK(sat_sprite_frame_init_raw(&f, 0, 16u, 16u, 0u, 0u, SAT_SPRITE_FLAG_SHADOW, 0, 0) == SAT_ERR_UNSUPPORTED);
    OK(sat_sprite_frame_init(&f, nullptr, 0u, 0, 0) == SAT_ERR_INVALID_ARG);
    sat_vdp1_texture_t tex{0x44u, 24u, 24u, 2u, 1u, SAT_VDP1_TEXTURE_INDEXED8};
    OK(sat_sprite_frame_init(&f, &tex, SAT_SPRITE_FLAG_FLIP_V | SAT_SPRITE_FLAG_OPAQUE, 12, 12) == SAT_OK);
    OK(f.ctrl == 0x0020u && (f.pmod & 0x0040u) != 0u && f.srca == 0x44u);
    tex.valid = 0u;
    OK(sat_sprite_frame_init(&f, &tex, 0u, 0, 0) == SAT_ERR_INVALID_ARG);

    /* Byte-identical commands to push_sprite for random frames and positions
     * (KEEP_PRECLIP), and identical but for Pclp without it. A normal sprite is compared over the
     * 16 bytes the VDP1 reads (ctrl .. A): the batch leaves the vertices B, C and D unwritten. */
    {
        const size_t kHead = 16u;
        struct Spec { uint16_t flags, format, palette; } specs[] = {
            {0u, SAT_VDP1_TEXTURE_INDEXED8, 1u},
            {SAT_SPRITE_FLAG_OPAQUE | SAT_SPRITE_FLAG_FLIP_V, SAT_VDP1_TEXTURE_INDEXED8, 7u},
            {SAT_SPRITE_FLAG_FLIP_H | SAT_SPRITE_FLAG_MESH, SAT_VDP1_TEXTURE_INDEXED8, 0u},
            {0u, SAT_VDP1_TEXTURE_LUT4, 0x1234u},
        };
        std::vector<sat_sprite_frame_t> frames;
        for (const Spec& s : specs) {
            sat_sprite_frame_t fr{};
            OK(sat_sprite_frame_init_raw(&fr, static_cast<uint16_t>(0x80 + frames.size() * 9u), 16u, 16u,
                                         s.format, s.palette, s.flags, 8, 8) == SAT_OK);
            frames.push_back(fr);
        }
        sat_sprite_frame_t big{};
        OK(sat_sprite_frame_init_raw(&big, 0x500u, 64u, 48u, SAT_VDP1_TEXTURE_INDEXED8, 2u, 0u, 32, 24) == SAT_OK);
        frames.push_back(big);

        std::vector<sat_sprite_instance_t> inst;
        for (int i = 0; i < 200; ++i) {
            sat_sprite_instance_t in{};
            in.x = static_cast<int16_t>(static_cast<int>(rnd() % 400u) - 40);
            in.y = static_cast<int16_t>(static_cast<int>(rnd() % 300u) - 40);
            in.frame = static_cast<uint16_t>(rnd() % frames.size());
            inst.push_back(in);
        }
        std::vector<Command> list(512);
        begin_frame(list.data(), 512u);
        sat_sprite_batch_stats_t st{};
        OK(sat_sprite_batch_draw(frames.data(), static_cast<uint32_t>(frames.size()), inst.data(),
                                 static_cast<uint32_t>(inst.size()), SAT_SPRITE_BATCH_KEEP_PRECLIP, &st) == SAT_OK);
        OK(st.no_preclip == 0u && st.dropped == 0u && st.invalid == 0u);
        OK(st.submitted + st.culled == inst.size());
        std::vector<Command> list_fast(512);
        begin_frame(list_fast.data(), 512u);
        sat_sprite_batch_stats_t fast{};
        OK(sat_sprite_batch_draw(frames.data(), static_cast<uint32_t>(frames.size()), inst.data(),
                                 static_cast<uint32_t>(inst.size()), 0u, &fast) == SAT_OK);
        OK(fast.submitted == st.submitted && fast.culled == st.culled);

        const Spec spec_of_frame[] = {specs[0], specs[1], specs[2], specs[3], {0u, SAT_VDP1_TEXTURE_INDEXED8, 2u}};
        uint32_t slot = 2u, expect_off = 0u, culled = 0u;
        for (const sat_sprite_instance_t& in : inst) {
            const sat_sprite_frame_t& fr = frames[in.frame];
            const int left = in.x - fr.pivot_x, top = in.y - fr.pivot_y;
            const int right = left + sat_sprite_frame_width(&fr) - 1, bottom = top + sat_sprite_frame_height(&fr) - 1;
            if (right < 0 || bottom < 0 || left >= 320 || top >= 224) { ++culled; continue; }
            const Spec& s = spec_of_frame[in.frame];
            const Command want = reference_command(fr, in, s.flags, s.format, s.palette);
            OK(std::memcmp(&list[slot], &want, kHead) == 0);
            Command want_fast = want;
            const bool inside = left >= 0 && top >= 0 && right < 320 && bottom < 224;
            const bool hflip = (s.flags & SAT_SPRITE_FLAG_FLIP_H) != 0u;
            if (inside && !hflip) { want_fast.pmod |= 0x0800u; ++expect_off; }
            OK(std::memcmp(&list_fast[slot], &want_fast, kHead) == 0);
            ++slot;
        }
        OK(culled == st.culled && expect_off == fast.no_preclip && expect_off != 0u);
        OK(fast.no_preclip < fast.submitted);   /* edge sprites keep pre-clipping */

        /* NO_CULL submits everything. */
        std::vector<Command> all(512);
        begin_frame(all.data(), 512u);
        sat_sprite_batch_stats_t nc{};
        OK(sat_sprite_batch_draw(frames.data(), static_cast<uint32_t>(frames.size()), inst.data(),
                                 static_cast<uint32_t>(inst.size()), SAT_SPRITE_BATCH_NO_CULL, &nc) == SAT_OK);
        OK(nc.culled == 0u && nc.submitted == inst.size());

        /* The direct-write window emits the same commands as sat_sprite_batch_draw. */
        std::vector<Command> viawin(512);
        begin_frame(viawin.data(), 512u);
        sat_sprite_batch_window_t win;
        OK(sat_sprite_batch_begin(&win, static_cast<uint32_t>(inst.size()), 0u) == SAT_OK);
        sat_sprite_batch_window_t local = win;
        for (const sat_sprite_instance_t& in : inst) {
            sat_sprite_batch_emit(&local, &frames[in.frame], in.x, in.y);
        }
        sat_sprite_batch_stats_t ws{};
        OK(sat_sprite_batch_end(&local, &ws) == SAT_OK);
        OK(ws.submitted == fast.submitted && ws.culled == fast.culled && ws.no_preclip == fast.no_preclip);
        for (uint32_t i = 0; i < fast.submitted; ++i) {
            OK(std::memcmp(&viawin[2u + i], &list_fast[2u + i], kHead) == 0);
        }
        /* A window smaller than the sprites drops the rest and reports it. */
        begin_frame(viawin.data(), 512u);
        OK(sat_sprite_batch_begin(&win, 10u, 0u) == SAT_OK);
        local = win;
        for (const sat_sprite_instance_t& in : inst) sat_sprite_batch_emit(&local, &frames[in.frame], 160, 112);
        OK(sat_sprite_batch_end(&local, &ws) == SAT_ERR_CAPACITY);
        OK(ws.submitted == 10u && ws.dropped == inst.size() - 10u);
        OK(sat_sprite_batch_begin(&win, 1u, 0x80u) == SAT_ERR_INVALID_ARG);
    }

    /* A user-clipped frame inside the screen keeps pre-clipping: it can still cross the user
     * rectangle, which the Pclp bit would not handle. */
    {
        sat_sprite_frame_t pair[2]{};
        OK(sat_sprite_frame_init_raw(&pair[0], 1u, 16u, 16u, 0u, 0u, 0u, 8, 8) == SAT_OK);
        OK(sat_sprite_frame_init_raw(&pair[1], 1u, 16u, 16u, 0u, 0u, SAT_SPRITE_FLAG_USER_CLIP, 8, 8) == SAT_OK);
        const sat_sprite_instance_t two[2] = {{100, 100, 0u, 0u}, {100, 100, 1u, 0u}};
        std::vector<Command> list(16);
        begin_frame(list.data(), 16u);
        sat_sprite_batch_stats_t st{};
        OK(sat_sprite_batch_draw(pair, 2u, two, 2u, 0u, &st) == SAT_OK);
        OK(st.submitted == 2u && st.no_preclip == 1u);
        OK((list[2].pmod & 0x0800u) != 0u && (list[2].pmod & 0x0400u) == 0u);
        OK((list[3].pmod & 0x0800u) == 0u && (list[3].pmod & 0x0400u) != 0u);
    }

    /* Edge touching: one texel on screen is on screen, one texel past is culled. */
    {
        sat_sprite_frame_t fr{};
        OK(sat_sprite_frame_init_raw(&fr, 1u, 16u, 16u, 0u, 0u, 0u, 0, 0) == SAT_OK);
        const sat_sprite_instance_t cases[] = {
            {-15, 0, 0, 0},   /* right column at x=0: visible */
            {-16, 0, 0, 0},   /* right column at x=-1: off */
            {319, 0, 0, 0},   /* left column at 319: visible */
            {320, 0, 0, 0},   /* off */
            {0, -15, 0, 0}, {0, -16, 0, 0}, {0, 223, 0, 0}, {0, 224, 0, 0},
        };
        const bool visible[] = {true, false, true, false, true, false, true, false};
        for (size_t i = 0; i < 8; ++i) {
            Command buf[8]{};
            begin_frame(buf, 8u);
            sat_sprite_batch_stats_t st{};
            OK(sat_sprite_batch_draw(&fr, 1u, &cases[i], 1u, 0u, &st) == SAT_OK);
            OK(st.submitted == (visible[i] ? 1u : 0u) && st.culled == (visible[i] ? 0u : 1u));
            /* partially off screen: pre-clipping stays on */
            OK(st.no_preclip == 0u);
        }
        /* exactly filling the corner: inside, pre-clipping off */
        Command buf[8]{};
        begin_frame(buf, 8u);
        sat_sprite_batch_stats_t st{};
        const sat_sprite_instance_t corner[] = {{0, 0, 0, 0}, {304, 208, 0, 0}};
        OK(sat_sprite_batch_draw(&fr, 1u, corner, 2u, 0u, &st) == SAT_OK);
        OK(st.submitted == 2u && st.no_preclip == 2u);
        OK(buf[2].xa == -160 && buf[2].ya == -112);
        OK(buf[3].xa == 144 && buf[3].ya == 96);
    }

    /* Capacity: the END slot and overlay quota are never consumed, the
     * instances that fit are drawn, the rest counted. */
    {
        sat_sprite_frame_t fr{};
        OK(sat_sprite_frame_init_raw(&fr, 1u, 8u, 8u, 0u, 0u, 0u, 0, 0) == SAT_OK);
        std::vector<sat_sprite_instance_t> inst(20, sat_sprite_instance_t{10, 10, 0, 0});
        Command buf[12]{};
        begin_frame(buf, 12u);
        OK(reserve_overlay_commands(3u) == SAT_OK);
        sat_sprite_batch_stats_t st{};
        /* 12 slots - 2 setup - 1 END - 3 overlay = 6 */
        OK(sat_sprite_batch_draw(&fr, 1u, inst.data(), 20u, 0u, &st) == SAT_ERR_CAPACITY);
        OK(st.submitted == 6u && st.dropped == 14u);
        SpriteRequest one{};
        one.width = 8u; one.height = 8u;
        OK(push_sprite(one) == SAT_ERR_CAPACITY);          /* same answer as the single path */
        OK(begin_overlay_pass() == SAT_OK);
        OK(sat_sprite_batch_draw(&fr, 1u, inst.data(), 1u, 0u, &st) == SAT_OK);   /* overlay may use its quota */
        OK(st.submitted == 1u);

        /* Equivalent to calling push_sprite until it says CAPACITY. */
        Command a[16]{}, b[16]{};
        for (uint32_t cap = 4u; cap <= 16u; ++cap) {
            begin_frame(a, static_cast<uint16_t>(cap));
            uint32_t single = 0u;
            while (push_sprite(one) == SAT_OK) ++single;
            begin_frame(b, static_cast<uint16_t>(cap));
            sat_sprite_batch_stats_t s2{};
            sat_sprite_batch_draw(&fr, 1u, inst.data(), 20u, SAT_SPRITE_BATCH_KEEP_PRECLIP, &s2);
            OK(s2.submitted == single);
        }
    }

    /* Invalid frame index: skipped, the rest drawn, error reported. */
    {
        sat_sprite_frame_t fr{};
        OK(sat_sprite_frame_init_raw(&fr, 1u, 8u, 8u, 0u, 0u, 0u, 0, 0) == SAT_OK);
        const sat_sprite_instance_t inst[] = {{10, 10, 0, 0}, {20, 20, 5, 0}, {30, 30, 0, 0}};
        Command buf[12]{};
        begin_frame(buf, 12u);
        sat_sprite_batch_stats_t st{};
        OK(sat_sprite_batch_draw(&fr, 1u, inst, 3u, 0u, &st) == SAT_ERR_INVALID_ARG);
        OK(st.submitted == 2u && st.invalid == 1u);
        OK(sat_sprite_batch_draw(&fr, 1u, nullptr, 3u, 0u, &st) == SAT_ERR_INVALID_ARG);
        OK(sat_sprite_batch_draw(&fr, 1u, inst, 0u, 0u, &st) == SAT_OK && st.submitted == 0u);
        OK(sat_sprite_batch_draw(&fr, 1u, inst, 1u, 0x80u, &st) == SAT_ERR_INVALID_ARG);   /* unknown option */
    }

    /* The copy into VRAM: a normal sprite without Gouraud shading moves its first 16 bytes only, any
     * other command all 32, and the slot's other bytes stay as they were. */
    {
        using saturn::hal::vdp1::submit_copy::copy_commands;
        enum { kCount = 64 };
        std::vector<uint32_t> src(kCount * 8u), dst(kCount * 8u, 0xDEADBEEFu);
        uint32_t whole = 0u, head = 0u;
        for (uint32_t i = 0u; i < kCount; ++i) {
            for (uint32_t k = 0u; k < 8u; ++k) src[i * 8u + k] = rnd() * 2654435761u + k;
            const uint32_t kind = rnd() % 4u;
            /* CMDCTRL command in bits 19-16 of word 0, CMDPMOD Gouraud bit 2 in bit 18 of word 1 */
            src[i * 8u] = (src[i * 8u] & ~0x000F0000u) | (kind == 1u ? 0x00020000u : kind == 2u ? 0x00080000u : 0u);
            src[i * 8u + 1u] = (src[i * 8u + 1u] & ~0x00040000u) | (kind == 3u ? 0x00040000u : 0u);
            if (kind == 0u) ++head; else ++whole;
        }
        OK(head != 0u && whole != 0u);
        const uint32_t stores = copy_commands(src.data(), dst.data(), kCount);
        OK(stores == head * 4u + whole * 8u);
        for (uint32_t i = 0u; i < kCount; ++i) {
            const bool only_head = saturn::hal::vdp1::submit_copy::reads_only_head(src[i * 8u], src[i * 8u + 1u]);
            for (uint32_t k = 0u; k < 8u; ++k) {
                if (k < 4u || !only_head) OK(dst[i * 8u + k] == src[i * 8u + k]);
                else OK(dst[i * 8u + k] == 0xDEADBEEFu);
            }
        }
        OK(copy_commands(src.data(), dst.data(), 0u) == 0u);
    }

    std::puts("sprite batch: OK");
    return 0;
}
