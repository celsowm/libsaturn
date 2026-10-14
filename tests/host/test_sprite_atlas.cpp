#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>

#include "saturn/resource_plan.h"
#include "saturn/sprite_atlas.h"
#include "saturn/vdp1.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

/* ---- fake VDP1 VRAM behind the public calls the atlas makes ------------- */
namespace {
constexpr uint32_t kVram = 512u * 1024u;
constexpr uint32_t kTextureBase = 80u * 1024u;
std::vector<uint8_t> g_vram;
uint32_t g_cursor = kTextureBase;
uint32_t g_write_calls = 0, g_lut_calls = 0, g_palette_calls = 0, g_bytes_written = 0;
uint16_t g_palette_bank = 0xFFFFu;
uint16_t g_palette[256];
void reset_vram() {
    g_vram.assign(kVram, 0u);
    g_cursor = kTextureBase;
    g_write_calls = g_lut_calls = g_palette_calls = g_bytes_written = 0u;
}
}  // namespace

extern "C" sat_result_t sat_vdp1_vram_reserve(uint32_t bytes, uint32_t* out_offset) {
    const uint32_t at = (g_cursor + 7u) & ~7u;
    if (at + bytes > kVram) return SAT_ERR_CAPACITY;
    *out_offset = at;
    g_cursor = at + bytes;
    return SAT_OK;
}
extern "C" sat_result_t sat_vdp1_vram_write(uint32_t offset, const void* src, uint32_t bytes) {
    if (offset < kTextureBase || offset + bytes > g_cursor) return SAT_ERR_INVALID_ARG;
    std::memcpy(&g_vram[offset], src, bytes);
    ++g_write_calls;
    g_bytes_written += bytes;
    return SAT_OK;
}
extern "C" sat_result_t sat_vdp1_upload_lut(const uint16_t* lut, uint16_t* out_colr) {
    const uint32_t at = (g_cursor + 31u) & ~31u;
    g_cursor = at + 32u;
    for (int i = 0; i < 16; ++i) {   /* big-endian words in VRAM */
        g_vram[at + 2 * i] = static_cast<uint8_t>(lut[i] >> 8);
        g_vram[at + 2 * i + 1] = static_cast<uint8_t>(lut[i]);
    }
    *out_colr = static_cast<uint16_t>(at / 8u);
    ++g_lut_calls;
    return SAT_OK;
}
extern "C" sat_result_t sat_palette_upload_indexed8(const uint16_t* palette, uint16_t bank) {
    std::memcpy(g_palette, palette, sizeof(g_palette));
    g_palette_bank = bank;
    ++g_palette_calls;
    return SAT_OK;
}

/* ---- SATL builder (the packer's layout, written independently) ---------- */
namespace {

void put16(std::vector<uint8_t>& v, size_t at, uint32_t x) { v[at] = uint8_t(x >> 8); v[at + 1] = uint8_t(x); }
void put32(std::vector<uint8_t>& v, size_t at, uint32_t x) {
    v[at] = uint8_t(x >> 24); v[at + 1] = uint8_t(x >> 16); v[at + 2] = uint8_t(x >> 8); v[at + 3] = uint8_t(x);
}

uint32_t crc32(const uint8_t* d, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        c ^= d[i];
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

struct FrameSpec { uint16_t w, h, fmt, lut, group; int16_t px, py; uint8_t fill; };

struct Atlas {
    std::vector<uint8_t> blob;
    uint32_t table_bytes = 0, group_off[8] = {}, group_bytes[8] = {};
    uint32_t frame_data_off[64] = {};
    uint32_t group_off_table = 0, frame_off = 64, lut_off = 0, pal_off = 0;
    uint16_t frames = 0, luts = 0, groups = 0;
};

Atlas build(const std::vector<FrameSpec>& specs, uint16_t luts, uint16_t groups) {
    Atlas a;
    a.frames = static_cast<uint16_t>(specs.size());
    a.luts = luts;
    a.groups = groups;
    a.lut_off = 64u + 32u * a.frames;
    a.pal_off = a.lut_off + 32u * luts;
    a.group_off_table = a.pal_off + 512u;
    a.table_bytes = (a.group_off_table + 16u * groups + 2047u) / 2048u * 2048u;
    std::vector<uint8_t> data;                       /* all groups' data, after the tables */
    uint32_t first[8] = {}, count[8] = {};
    for (uint16_t g = 0; g < groups; ++g) {
        a.group_off[g] = a.table_bytes + static_cast<uint32_t>(data.size());
        bool seen = false;
        for (size_t i = 0; i < specs.size(); ++i) {
            if (specs[i].group != g) continue;
            if (!seen) { first[g] = static_cast<uint32_t>(i); seen = true; }
            ++count[g];
            const uint32_t bytes = specs[i].fmt == SAT_VDP1_TEXTURE_LUT4 ? specs[i].w * specs[i].h / 2u
                                                                         : specs[i].w * specs[i].h;
            a.frame_data_off[i] = a.table_bytes + static_cast<uint32_t>(data.size());
            data.insert(data.end(), bytes, specs[i].fill);
            data.insert(data.end(), (8u - bytes % 8u) % 8u, 0u);
        }
        data.insert(data.end(), (2048u - data.size() % 2048u) % 2048u, 0u);
        a.group_bytes[g] = a.table_bytes + static_cast<uint32_t>(data.size()) - a.group_off[g];
    }
    a.blob.assign(a.table_bytes + data.size(), 0u);
    std::memcpy(&a.blob[a.table_bytes], data.data(), data.size());
    std::vector<uint8_t>& b = a.blob;
    std::memcpy(&b[0], "SATL", 4);
    put16(b, 4, 1); put16(b, 8, a.frames); put16(b, 10, luts); put16(b, 12, groups); put16(b, 14, 256);
    put32(b, 16, 64); put32(b, 20, a.lut_off); put32(b, 24, a.pal_off); put32(b, 28, a.group_off_table);
    put32(b, 32, static_cast<uint32_t>(b.size()));
    put16(b, 40, 0);
    for (size_t i = 0; i < specs.size(); ++i) {
        const size_t r = 64 + 32 * i;
        const uint32_t bytes = specs[i].fmt == SAT_VDP1_TEXTURE_LUT4 ? specs[i].w * specs[i].h / 2u
                                                                     : specs[i].w * specs[i].h;
        put16(b, r, specs[i].w); put16(b, r + 2, specs[i].h);
        put16(b, r + 4, static_cast<uint16_t>(specs[i].px)); put16(b, r + 6, static_cast<uint16_t>(specs[i].py));
        put16(b, r + 8, specs[i].fmt); put16(b, r + 10, specs[i].lut);
        put32(b, r + 12, a.frame_data_off[i]); put32(b, r + 16, bytes);
        put16(b, r + 20, specs[i].group); put16(b, r + 22, 3 + static_cast<uint32_t>(i)); put16(b, r + 24, 3);
    }
    for (uint16_t l = 0; l < luts; ++l)
        for (int e = 0; e < 16; ++e) put16(b, a.lut_off + 32u * l + 2u * e, e == 0 ? 0u : 0x8000u | (l * 16u + e));
    for (int e = 0; e < 256; ++e) put16(b, a.pal_off + 2u * e, 0x8000u | static_cast<uint32_t>(e * 7));
    for (uint16_t g = 0; g < groups; ++g) {
        const size_t r = a.group_off_table + 16u * g;
        put32(b, r, a.group_off[g]); put32(b, r + 4, a.group_bytes[g]);
        put16(b, r + 8, first[g]); put16(b, r + 10, count[g]);
    }
    put32(b, 36, crc32(&b[64], b.size() - 64));
    return a;
}

void refresh_crc(std::vector<uint8_t>& b) { put32(b, 36, crc32(&b[64], b.size() - 64)); }

std::vector<FrameSpec> default_specs() {
    return {
        {16, 16, SAT_VDP1_TEXTURE_INDEXED8, 0, 0, 8, 8, 0x11},
        {24, 20, SAT_VDP1_TEXTURE_LUT4, 1, 0, 12, 10, 0x22},
        {8, 8, SAT_VDP1_TEXTURE_LUT4, 0, 0, 4, 4, 0x33},
        {32, 32, SAT_VDP1_TEXTURE_INDEXED8, 0, 1, 16, 16, 0x44},
        {16, 8, SAT_VDP1_TEXTURE_LUT4, 1, 1, 8, 4, 0x55},
    };
}

sat_result_t open_blob(const std::vector<uint8_t>& blob, sat_sprite_atlas_t* a) {
    return sat_sprite_atlas_open(a, blob.data(), static_cast<uint32_t>(blob.size()));
}

}  // namespace

/* test_sprite_atlas FILE.ATL: the same loader on a real packer output. Every
 * group goes to VRAM, every frame's srca must name that frame's own bytes. */
static int check_real_atlas(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", path); return 2; }
    std::vector<uint8_t> blob;
    uint8_t buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) blob.insert(blob.end(), buf, buf + n);
    std::fclose(f);
    reset_vram();
    sat_sprite_atlas_t a{};
    OK(sat_sprite_atlas_open(&a, blob.data(), static_cast<uint32_t>(blob.size())) == SAT_OK && a.crc_checked == 1u);
    for (uint32_t g = 0; g < sat_sprite_atlas_group_count(&a); ++g) OK(sat_sprite_atlas_reserve_slot(&a, g, 0u) == SAT_OK);
    OK(sat_sprite_atlas_upload_palette(&a, 1u) == SAT_OK && sat_sprite_atlas_upload_luts(&a) == SAT_OK);
    std::vector<sat_sprite_frame_t> frames(sat_sprite_atlas_frame_count(&a));
    for (uint32_t g = 0; g < sat_sprite_atlas_group_count(&a); ++g) {
        sat_sprite_atlas_group_info_t gi{};
        OK(sat_sprite_atlas_group_info(&a, g, &gi) == SAT_OK);
        OK(sat_sprite_atlas_load_group(&a, g, g, &blob[gi.data_offset], gi.data_bytes) == SAT_OK);
        OK(sat_sprite_atlas_build_frames(&a, g, 0u, frames.data(), static_cast<uint32_t>(frames.size())) == SAT_OK);
    }
    uint64_t pixel_bytes = 0;
    for (uint32_t i = 0; i < frames.size(); ++i) {
        sat_sprite_atlas_frame_info_t fi{};
        OK(sat_sprite_atlas_frame_info(&a, i, &fi) == SAT_OK);
        OK(frames[i].width == fi.width && frames[i].height == fi.height);
        OK(std::memcmp(&g_vram[frames[i].srca * 8u], &blob[fi.data_offset], fi.data_bytes) == 0);
        pixel_bytes += fi.data_bytes;
    }
    std::printf("real atlas %s: %u frames, %u groups, %u luts, %u bytes uploaded (%llu bytes of frames), %u writes\n", path,
                static_cast<unsigned>(frames.size()), static_cast<unsigned>(sat_sprite_atlas_group_count(&a)),
                static_cast<unsigned>(sat_sprite_atlas_lut_count(&a)), g_bytes_written,
                static_cast<unsigned long long>(pixel_bytes), g_write_calls);
    OK(g_write_calls == sat_sprite_atlas_group_count(&a));      /* one write per group, none per frame */
    OK(a.stats.lut_uploads == sat_sprite_atlas_lut_count(&a));
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1) return check_real_atlas(argv[1]);
    const Atlas good = build(default_specs(), 2, 2);

    /* Open, info, peek. */
    {
        sat_sprite_atlas_t a{};
        OK(open_blob(good.blob, &a) == SAT_OK && a.crc_checked == 1u);
        OK(sat_sprite_atlas_frame_count(&a) == 5u && sat_sprite_atlas_group_count(&a) == 2u &&
           sat_sprite_atlas_lut_count(&a) == 2u);
        sat_sprite_atlas_frame_info_t f{};
        OK(sat_sprite_atlas_frame_info(&a, 1u, &f) == SAT_OK);
        OK(f.width == 24u && f.height == 20u && f.format == SAT_VDP1_TEXTURE_LUT4 && f.lut == 1u &&
           f.pivot_x == 12 && f.pivot_y == 10 && f.group == 0u && f.data_bytes == 240u && f.radius == 4u);
        OK(sat_sprite_atlas_frame_info(&a, 5u, &f) == SAT_ERR_INVALID_ARG);
        sat_sprite_atlas_group_info_t g{};
        OK(sat_sprite_atlas_group_info(&a, 1u, &g) == SAT_OK);
        OK(g.first_frame == 3u && g.frame_count == 2u && g.data_offset == good.group_off[1] &&
           g.data_bytes == good.group_bytes[1] && g.data_offset % 2048u == 0u);
        OK(sat_sprite_atlas_max_group_bytes(&a) == 2048u);
        uint32_t table = 0, total = 0;
        OK(sat_sprite_atlas_peek(good.blob.data(), 64u, &table, &total) == SAT_OK);
        OK(table == good.table_bytes && total == good.blob.size());

        /* Tables alone are enough to open; the CRC then cannot be checked. */
        sat_sprite_atlas_t t{};
        OK(sat_sprite_atlas_open(&t, good.blob.data(), good.table_bytes) == SAT_OK && t.crc_checked == 0u);
        uint16_t pal[256];
        uint32_t n = 0;
        OK(sat_sprite_atlas_palette(&a, pal, 256u, &n) == SAT_OK && n == 256u && pal[3] == (0x8000u | 21u));
        OK(sat_sprite_atlas_palette(&a, pal, 100u, &n) == SAT_ERR_INVALID_ARG);
    }

    /* Malformed blobs are refused, never half-used. */
    {
        using Blob = std::vector<uint8_t>;
        struct Case { const char* name; std::function<void(Blob&)> mutate; sat_result_t want; };
        const size_t f0 = 64, f1 = 64 + 32, f3 = 64 + 32 * 3;
        const size_t g0 = good.group_off_table, g1 = good.group_off_table + 16;
        const std::vector<Case> cases = {
            {"bad magic", [](Blob& b) { b[0] = 'X'; }, SAT_ERR_INVALID_ARG},
            {"bad version", [](Blob& b) { put16(b, 4, 2); }, SAT_ERR_INVALID_ARG},
            {"zero frames", [](Blob& b) { put16(b, 8, 0); }, SAT_ERR_INVALID_ARG},
            {"too many luts", [](Blob& b) { put16(b, 10, 99); }, SAT_ERR_INVALID_ARG},
            {"zero groups", [](Blob& b) { put16(b, 12, 0); }, SAT_ERR_INVALID_ARG},
            {"palette too large", [](Blob& b) { put16(b, 14, 257); }, SAT_ERR_INVALID_ARG},
            {"frame table moved", [](Blob& b) { put32(b, 16, 128); }, SAT_ERR_INVALID_ARG},
            {"orientation", [](Blob& b) { put16(b, 40, 3); }, SAT_ERR_INVALID_ARG},
            {"total below tables", [](Blob& b) { put32(b, 32, 100); }, SAT_ERR_INVALID_ARG},
            {"total smaller than the bytes given", [](Blob& b) { put32(b, 32, uint32_t(b.size()) - 2048u); }, SAT_ERR_INVALID_ARG},
            {"crc", [](Blob& b) { b[b.size() - 1] ^= 1; }, SAT_ERR_VERIFY_FAILED},
            {"width not multiple of 8", [=](Blob& b) { put16(b, f0, 12); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"width zero", [=](Blob& b) { put16(b, f0, 0); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"height zero", [=](Blob& b) { put16(b, f0 + 2, 0); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"height over 255", [=](Blob& b) { put16(b, f0 + 2, 256); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"unknown format", [=](Blob& b) { put16(b, f0 + 8, 2); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"lut out of range", [=](Blob& b) { put16(b, f1 + 10, 2); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"size mismatch", [=](Blob& b) { put32(b, f0 + 16, 100); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"frame group out of range", [=](Blob& b) { put16(b, f0 + 20, 9); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"frame claims another group", [=](Blob& b) { put16(b, f0 + 20, 1); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"frame offset misaligned", [=](Blob& b) { put32(b, f0 + 12, good.frame_data_off[0] + 4); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"frame data before its group", [=](Blob& b) { put32(b, f3 + 12, good.group_off[0]); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"frame data past its group", [=](Blob& b) { put32(b, f0 + 12, good.group_off[0] + good.group_bytes[0] - 8u); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"group offset unaligned", [=](Blob& b) { put32(b, g0, good.group_off[0] + 8u); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"group data inside the tables", [=](Blob& b) { put32(b, g0, 0); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"groups overlap", [=](Blob& b) { put32(b, g1, good.group_off[0]); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"group past the file", [=](Blob& b) { put32(b, g1 + 4, 1u << 20); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"group frame range gap", [=](Blob& b) { put16(b, g1 + 8, 4); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"group without frames", [=](Blob& b) { put16(b, g1 + 10, 0); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
            {"frames not all owned", [=](Blob& b) { put16(b, g1 + 10, 1); refresh_crc(b); }, SAT_ERR_INVALID_ARG},
        };
        for (const Case& c : cases) {
            Blob b = good.blob;
            c.mutate(b);
            sat_sprite_atlas_t a{};
            const sat_result_t got = open_blob(b, &a);
            if (got != c.want) std::fprintf(stderr, "case '%s': got %d want %d\n", c.name, got, c.want);
            OK(got == c.want);
            OK(sat_sprite_atlas_frame_count(&a) == 0u);                  /* left unusable */
        }
        sat_sprite_atlas_t a{};
        OK(sat_sprite_atlas_open(&a, good.blob.data(), 40u) == SAT_ERR_INVALID_ARG);                    /* truncated header */
        OK(sat_sprite_atlas_open(&a, good.blob.data(), good.table_bytes - 1u) == SAT_ERR_INVALID_ARG);  /* truncated tables */
        OK(sat_sprite_atlas_open(&a, nullptr, 4096u) == SAT_ERR_INVALID_ARG);
        OK(sat_sprite_atlas_open(nullptr, good.blob.data(), 4096u) == SAT_ERR_INVALID_ARG);
        OK(sat_sprite_atlas_peek(good.blob.data(), 10u, nullptr, nullptr) == SAT_ERR_INVALID_ARG);
    }

    /* Residency, frames from a slot, no redundant uploads. */
    {
        reset_vram();
        sat_sprite_atlas_t a{};
        OK(open_blob(good.blob, &a) == SAT_OK);
        sat_sprite_frame_t frames[5];
        std::memset(frames, 0xEE, sizeof(frames));

        OK(sat_sprite_atlas_load_group(&a, 0u, 0u, &good.blob[good.group_off[0]], good.group_bytes[0]) ==
           SAT_ERR_NOT_INITIALIZED);                                        /* slot not reserved */
        OK(sat_sprite_atlas_reserve_slot(&a, 0u, 0u) == SAT_OK);
        OK(sat_sprite_atlas_reserve_slot(&a, 0u, 0u) == SAT_ERR_ALREADY_EXISTS);
        OK(sat_sprite_atlas_reserve_slot(&a, 1u, 0u) == SAT_OK);
        OK(sat_sprite_atlas_reserve_slot(&a, 9u, 0u) == SAT_ERR_INVALID_ARG);
        OK(a.slot[0].capacity == 2048u && a.slot[1].vram_offset == a.slot[0].vram_offset + 2048u);

        OK(sat_sprite_atlas_build_frames(&a, 0u, 0u, frames, 5u) == SAT_ERR_NOT_FOUND);      /* not resident */
        OK(sat_sprite_atlas_load_group(&a, 0u, 0u, &good.blob[good.group_off[0]], 100u) == SAT_ERR_INVALID_ARG);
        OK(sat_sprite_atlas_load_group(&a, 0u, 0u, &good.blob[good.group_off[0]], good.group_bytes[0]) == SAT_OK);
        OK(g_write_calls == 1u && g_bytes_written == 2048u);
        OK(std::memcmp(&g_vram[a.slot[0].vram_offset], &good.blob[good.group_off[0]], 2048u) == 0);

        /* INDEX8 needs the shared bank, LUT4 the tables: refused before anything is written. */
        OK(sat_sprite_atlas_build_frames(&a, 0u, 0u, frames, 5u) == SAT_ERR_NOT_INITIALIZED);
        OK(static_cast<uint8_t>(frames[0].srca) == 0xEEu);
        OK(sat_sprite_atlas_upload_palette(&a, 5u) == SAT_OK && g_palette_bank == 5u && g_palette[2] == (0x8000u | 14u));
        OK(sat_sprite_atlas_build_frames(&a, 0u, 0u, frames, 5u) == SAT_ERR_NOT_INITIALIZED);
        OK(sat_sprite_atlas_upload_luts(&a) == SAT_OK && sat_sprite_atlas_upload_luts(&a) == SAT_OK &&
           sat_sprite_atlas_upload_luts(&a) == SAT_OK);
        OK(g_lut_calls == 2u && a.stats.lut_uploads == 2u);                       /* once each, however often asked */
        OK(sat_sprite_atlas_build_frames(&a, 0u, 0u, frames, 4u) == SAT_ERR_CAPACITY);
        OK(sat_sprite_atlas_build_frames(&a, 0u, SAT_SPRITE_FLAG_OPAQUE, frames, 5u) == SAT_OK);

        const uint32_t base = a.slot[0].vram_offset;
        /* frame 0: INDEX8 16x16 at the start; frame 1: LUT4 24x20 after 256 bytes; frame 2 after 240 */
        OK(frames[0].srca == base / 8u && frames[0].colr == (5u << 8) && frames[0].size == ((2u << 8) | 16u));
        OK(frames[1].srca == (base + 256u) / 8u && frames[1].colr == a.lut_colr[1] && (frames[1].pmod & 0x0008u) != 0u);
        OK(frames[2].srca == (base + 256u + 240u) / 8u && frames[2].colr == a.lut_colr[0]);
        OK(frames[0].pivot_x == 8 && frames[1].pivot_y == 10 && (frames[0].pmod & 0x0040u) != 0u);
        OK(static_cast<uint8_t>(frames[3].srca) == 0xEEu);                       /* other group untouched */
        for (int i = 0; i < 3; ++i) OK(frames[i].srca * 8u >= base && frames[i].srca * 8u < base + 2048u);
        /* The VRAM the srca names holds the frame's pixels (a LUT4 frame is nibbles of 0x22). */
        OK(g_vram[frames[1].srca * 8u] == 0x22u && g_vram[frames[2].srca * 8u] == 0x33u);
        /* The LUT the colr names holds the table: entry 3 of LUT 1. */
        OK(g_vram[frames[1].colr * 8u + 6u] == 0x80u && g_vram[frames[1].colr * 8u + 7u] == 0x13u);

        /* Asking again for what is already there writes nothing. */
        const uint32_t writes = g_write_calls;
        OK(sat_sprite_atlas_load_group(&a, 0u, 0u, &good.blob[good.group_off[0]], good.group_bytes[0]) == SAT_OK);
        OK(g_write_calls == writes && a.stats.loads_skipped == 1u && a.stats.group_loads == 1u);
        uint32_t slot = 9u;
        OK(sat_sprite_atlas_group_slot(&a, 0u, &slot) == SAT_OK && slot == 0u);
        OK(sat_sprite_atlas_group_slot(&a, 1u, &slot) == SAT_ERR_NOT_FOUND);

        /* Prefetch group 1 into slot 1 in odd-sized chunks while slot 0 stays drawn. */
        const std::vector<uint8_t> slot0_before(g_vram.begin() + a.slot[0].vram_offset,
                                                g_vram.begin() + a.slot[0].vram_offset + 2048u);
        uint8_t skipped = 7u;
        OK(sat_sprite_atlas_upload_begin(&a, 1u, 1u, &skipped) == SAT_OK && skipped == 0u);
        OK(sat_sprite_atlas_upload_begin(&a, 1u, 1u, &skipped) == SAT_ERR_BUSY);
        OK(sat_sprite_atlas_upload_remaining(&a) == 2048u);
        OK(sat_sprite_atlas_upload_end(&a) == SAT_ERR_INVALID_ARG);              /* incomplete */
        const uint8_t* g1 = &good.blob[good.group_off[1]];
        uint32_t sent = 0u;
        const uint32_t chunks[] = {1u, 7u, 500u, 512u, 1028u};
        for (uint32_t c : chunks) {
            OK(sat_sprite_atlas_upload_chunk(&a, g1 + sent, c) == SAT_OK);
            sent += c;
            OK(sat_sprite_atlas_upload_remaining(&a) == 2048u - sent);
            OK(sat_sprite_atlas_build_frames(&a, 1u, 0u, frames, 5u) == SAT_ERR_NOT_FOUND);   /* not yet */
        }
        OK(sent == 2048u);
        OK(sat_sprite_atlas_upload_chunk(&a, g1, 1u) == SAT_ERR_INVALID_ARG);   /* past the end */
        OK(sat_sprite_atlas_upload_end(&a) == SAT_OK);
        OK(std::memcmp(&g_vram[a.slot[0].vram_offset], slot0_before.data(), 2048u) == 0);    /* slot 0 never touched */
        OK(std::memcmp(&g_vram[a.slot[1].vram_offset], g1, 2048u) == 0);
        OK(sat_sprite_atlas_build_frames(&a, 1u, 0u, frames, 5u) == SAT_OK);
        OK(frames[3].srca == a.slot[1].vram_offset / 8u && frames[4].colr == a.lut_colr[1]);
        OK(frames[0].srca == base / 8u);                                        /* group 0 frames kept */
        OK(sat_sprite_atlas_upload_remaining(&a) == 0u);

        /* Evict and reuse: the slot's old group is gone at once. */
        OK(sat_sprite_atlas_evict(&a, 0u) == SAT_OK);
        OK(sat_sprite_atlas_build_frames(&a, 0u, 0u, frames, 5u) == SAT_ERR_NOT_FOUND);
        OK(sat_sprite_atlas_upload_begin(&a, 0u, 1u, &skipped) == SAT_OK && skipped == 0u);
        OK(sat_sprite_atlas_evict(&a, 0u) == SAT_OK);                           /* abandons the upload */
        OK(sat_sprite_atlas_upload_begin(&a, 1u, 0u, &skipped) == SAT_OK);      /* BUSY would mean evict failed */
        OK(sat_sprite_atlas_evict(&a, 1u) == SAT_OK);
    }

    /* A slot too small for the group, and a failed upload leaves the slot empty. */
    {
        reset_vram();
        sat_sprite_atlas_t a{};
        OK(open_blob(good.blob, &a) == SAT_OK);
        OK(sat_sprite_atlas_reserve_slot(&a, 0u, 1024u) == SAT_OK);
        uint8_t skipped = 0u;
        OK(sat_sprite_atlas_upload_begin(&a, 0u, 0u, &skipped) == SAT_ERR_CAPACITY);
        OK(sat_sprite_atlas_load_group(&a, 0u, 0u, &good.blob[good.group_off[0]], good.group_bytes[0]) == SAT_ERR_CAPACITY);
        OK(g_write_calls == 0u);
        OK(sat_sprite_atlas_reserve_slot(&a, 1u, 2048u) == SAT_OK);
        OK(sat_sprite_atlas_load_group(&a, 1u, 0u, nullptr, good.group_bytes[0]) == SAT_ERR_INVALID_ARG);
        uint32_t slot = 0u;
        OK(sat_sprite_atlas_group_slot(&a, 0u, &slot) == SAT_ERR_NOT_FOUND);
        OK(a.loading_slot == -1);
    }

    /* VRAM budget through the resource plan. */
    {
        sat_sprite_atlas_t a{};
        OK(open_blob(good.blob, &a) == SAT_OK);
        sat_resource_plan_entry_t entries[8];
        sat_resource_plan_t plan{};
        OK(sat_resource_plan_init(&plan, entries, 8u) == SAT_OK);
        OK(sat_resource_plan_set_limit(&plan, SAT_RESOURCE_VDP1_VRAM, 2u * 2048u + 64u) == SAT_OK);
        OK(sat_sprite_atlas_plan_add(&a, &plan, 2u) == SAT_OK);
        OK(sat_resource_plan_finalize(&plan) == SAT_OK);
        uint32_t used = 0u, limit = 0u;
        OK(sat_resource_plan_usage(&plan, SAT_RESOURCE_VDP1_VRAM, &used, &limit) == SAT_OK && used == 2u * 2048u + 64u);

        sat_resource_plan_t tight{};
        sat_resource_plan_entry_t entries2[8];
        OK(sat_resource_plan_init(&tight, entries2, 8u) == SAT_OK);
        OK(sat_resource_plan_set_limit(&tight, SAT_RESOURCE_VDP1_VRAM, 2048u + 64u) == SAT_OK);
        sat_result_t st = sat_sprite_atlas_plan_add(&a, &tight, 2u);
        if (st == SAT_OK) st = sat_resource_plan_finalize(&tight);
        OK(st != SAT_OK);                                                       /* two slots do not fit one */
        OK(sat_sprite_atlas_plan_add(&a, &plan, 0u) == SAT_ERR_INVALID_ARG);
    }

    std::puts("sprite atlas: OK");
    return 0;
}
