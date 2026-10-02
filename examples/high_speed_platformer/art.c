#include "art.h"

#include "saturn/color.h"

#include "high_speed_platformer/layout.h"
#include "high_speed_platformer/stage.h"

/* ----- the cell sheet: one 8 x 8, 16-colour character per terrain profile ----- */

/* palette indices of the cell sheet */
#define C_NONE 0
#define C_GRASS 2
#define C_GRASS_DARK 3
#define C_DIRT 4
#define C_DIRT_DARK 5
#define C_DIRT_LIGHT 6
#define C_WOOD 7
#define C_WOOD_DARK 8
#define C_STONE 9

static int solid_at(const sat_terrain_profile2_t* p, int x, int y) {
    const int ext = p->column[x];
    if (ext > 0) return y >= 8 - ext;
    if (ext < 0) return y < -ext;
    return 0;
}

static uint32_t noise(int x, int y, uint32_t n) {
    return (uint32_t)(x * 7 + y * 13 + (int)n * 5 + ((x * y) ^ (int)n)) & 7u;
}

static uint8_t terrain_pixel(uint32_t n, int x, int y) {
    const sat_terrain_profile2_t* p = &stage_profiles[n];
    int depth;
    const int ext = p->column[x];
    if (!solid_at(p, x, y)) return C_NONE;
    if (p->material == 3u) return y == 0 ? C_WOOD_DARK : ((x & 3) == 0 ? C_WOOD_DARK : C_WOOD);
    if (ext > 0) depth = y - (8 - ext);
    else if (ext < 0) depth = (-ext - 1) - y;
    else depth = 0;
    if (p->material == 2u) {
        if (ext > 0) {
            if (depth == 0) return C_GRASS_DARK;
            if (depth < 3) return C_GRASS;
            if (depth == 3) return C_DIRT_DARK;
        } else if (depth < 2) {
            return C_STONE; /* the underside of an overhang */
        }
    }
    {
        const uint32_t v = noise(x, y, n);
        return v == 0u ? C_DIRT_DARK : (v == 1u ? C_DIRT_LIGHT : C_DIRT);
    }
}

/* decor: three characters of a flat-bottomed cloud */
static uint8_t cloud_pixel(uint32_t kind, int x, int y) {
    static const int8_t cap[8] = {8, 8, 5, 3, 2, 1, 0, 0}; /* first solid column of the left cap per row */
    int solid;
    if (y < 2) return C_NONE;
    if (kind == 0u) solid = x >= cap[y];
    else if (kind == 2u) solid = (7 - x) >= cap[y];
    else solid = y >= 3 || (x >= 2 && x <= 5);
    if (!solid) return C_NONE;
    return y == 7 ? C_GRASS_DARK : C_GRASS; /* the cloud's own two tones: white and pale blue */
}

void hsp_art_cells(uint16_t* out) {
    uint32_t n;
    for (n = 0; n < HSP_CHAR_COUNT; ++n) {
        int y;
        for (y = 0; y < 8; ++y) {
            uint8_t px[8];
            int x;
            for (x = 0; x < 8; ++x) {
                if (n == 0u) px[x] = C_NONE;
                else if (n < HSP_DECOR_FIRST) px[x] = terrain_pixel(n, x, y);
                else px[x] = cloud_pixel(n - HSP_DECOR_FIRST, x, y);
            }
            out[n * 16u + (uint32_t)y * 2u] =
                (uint16_t)(((uint32_t)px[0] << 12) | ((uint32_t)px[1] << 8) | ((uint32_t)px[2] << 4) | px[3]);
            out[n * 16u + (uint32_t)y * 2u + 1u] =
                (uint16_t)(((uint32_t)px[4] << 12) | ((uint32_t)px[5] << 8) | ((uint32_t)px[6] << 4) | px[7]);
        }
    }
}

void hsp_art_palettes(uint16_t* out) {
    static const uint16_t foreground[16] = {
        0, SAT_RGB555(6, 4, 2), SAT_RGB555(12, 26, 8), SAT_RGB555(6, 18, 6), SAT_RGB555(19, 13, 8),
        SAT_RGB555(13, 9, 5), SAT_RGB555(23, 17, 11), SAT_RGB555(26, 19, 10), SAT_RGB555(18, 12, 6),
        SAT_RGB555(18, 18, 20), 0, 0, 0, 0, 0, 0};
    static const uint16_t hills[16] = {
        0, SAT_RGB555(4, 8, 8), SAT_RGB555(9, 21, 15), SAT_RGB555(6, 15, 12), SAT_RGB555(8, 16, 12),
        SAT_RGB555(5, 12, 10), SAT_RGB555(11, 19, 15), SAT_RGB555(12, 16, 12), SAT_RGB555(8, 12, 9),
        SAT_RGB555(12, 14, 16), 0, 0, 0, 0, 0, 0};
    static const uint16_t clouds[16] = {
        0, 0, SAT_RGB555(31, 31, 31), SAT_RGB555(24, 28, 31), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t i;
    for (i = 0; i < 16u; ++i) {
        out[i] = foreground[i];
        out[16u + i] = hills[i];
        out[32u + i] = clouds[i];
    }
}

/* ----- the sprite sheet ----- */

#define S_CLEAR 0
#define S_OUTLINE 1
#define S_BLUE 2
#define S_LIGHT_BLUE 3
#define S_SKIN 4
#define S_RED 5
#define S_WHITE 6
#define S_GOLD 7
#define S_GOLD_DARK 8
#define S_METAL 9
#define S_METAL_DARK 10
#define S_GREEN 11
#define S_ORANGE 12

static uint8_t g_sheet[HSP_SHEET_W * HSP_SHEET_H];
static uint16_t g_sheet_palette[256];

static void put(int x, int y, uint8_t c) {
    if (x >= 0 && y >= 0 && x < HSP_SHEET_W && y < HSP_SHEET_H) g_sheet[y * HSP_SHEET_W + x] = c;
}

static void rect(int x, int y, int w, int h, uint8_t c) {
    int i, j;
    for (j = 0; j < h; ++j) {
        for (i = 0; i < w; ++i) put(x + i, y + j, c);
    }
}

/* a rectangle with a one-pixel outline */
static void box(int x, int y, int w, int h, uint8_t fill) {
    rect(x, y, w, h, S_OUTLINE);
    rect(x + 1, y + 1, w - 2, h - 2, fill);
}

static void ellipse(int cx, int cy, int rx, int ry, uint8_t c) {
    int x, y;
    for (y = -ry; y <= ry; ++y) {
        for (x = -rx; x <= rx; ++x) {
            if (x * x * ry * ry + y * y * rx * rx <= rx * rx * ry * ry) put(cx + x, cy + y, c);
        }
    }
}

static void hero_frame(int cell, int pose) {
    const int ox = (cell % 8) * 32;
    const int oy = (cell / 8) * 32;
    if (pose >= 5) { /* a rolled-up ball with a spoke that turns */
        static const int8_t spoke[4][2] = {{9, 0}, {6, 6}, {0, 9}, {-6, 6}};
        const int k = pose - 5;
        int t;
        ellipse(ox + 16, oy + 22, 10, 10, S_OUTLINE);
        ellipse(ox + 16, oy + 22, 9, 9, S_BLUE);
        ellipse(ox + 14, oy + 19, 4, 4, S_LIGHT_BLUE);
        for (t = 0; t <= 8; ++t) {
            put(ox + 16 + spoke[k][0] * t / 9, oy + 22 + spoke[k][1] * t / 9, S_WHITE);
            put(ox + 16 - spoke[k][0] * t / 9, oy + 22 - spoke[k][1] * t / 9, S_WHITE);
        }
        return;
    }
    {
        /* standing and running: pose 0 = idle, 1..4 = the run cycle */
        static const int8_t stride[5] = {0, -3, -1, 3, 1};
        const int bob = (pose == 2 || pose == 4) ? 1 : 0;
        const int s = stride[pose];
        box(ox + 10, oy + 8 + bob, 12, 10, S_BLUE);                     /* head */
        rect(ox + 16, oy + 12 + bob, 5, 5, S_SKIN);                      /* face */
        rect(ox + 19, oy + 12 + bob, 1, 2, S_OUTLINE);                   /* eye */
        box(ox + 11, oy + 18 + bob, 10, 8, S_LIGHT_BLUE);                /* torso */
        box(ox + 11 + s, oy + 26, 4, 6, S_BLUE);                         /* legs */
        box(ox + 17 - s, oy + 26, 4, 6, S_BLUE);
        rect(ox + 11 + s, oy + 29, 5, 3, S_RED);                         /* shoes */
        rect(ox + 17 - s, oy + 29, 5, 3, S_RED);
    }
}

static void ring_frame(int i) {
    static const int8_t half_w[4] = {6, 4, 1, 4};
    const int ox = 16 * i;
    const int oy = 64;
    const int rx = half_w[i];
    ellipse(ox + 8, oy + 8, rx + 1, 7, S_GOLD_DARK);
    ellipse(ox + 8, oy + 8, rx, 6, S_GOLD);
    if (rx >= 3) ellipse(ox + 8, oy + 8, rx - 2, 4, S_CLEAR);
}

static void sparkle_frame(int i) {
    const int ox = 64 + 16 * i;
    const int oy = 64;
    const int r = 2 + 2 * i;
    int k;
    for (k = -r; k <= r; ++k) {
        put(ox + 8 + k, oy + 8, S_WHITE);
        put(ox + 8, oy + 8 + k, S_WHITE);
    }
    put(ox + 8, oy + 8, S_GOLD);
    put(ox + 7, oy + 7, S_GOLD);
    put(ox + 9, oy + 9, S_GOLD);
}

static void spring_frame(int ox, int extended) {
    const int oy = 64;
    const int top = extended ? 2 : 7;
    int y;
    rect(ox + 2, oy + 12, 12, 4, S_METAL_DARK);
    for (y = top + 3; y < 12; y += 2) rect(ox + 4, oy + y, 8, 1, S_METAL);
    box(ox + 1, oy + top, 14, 3, S_RED);
}

static void flag_frame(int ox, int phase) {
    const int oy = 64;
    rect(ox + 7, oy, 2, 32, S_METAL);
    rect(ox + 7, oy, 2, 1, S_WHITE);
    rect(ox + 9, oy + 1 + phase, 7, 7 - phase, S_RED);
    rect(ox + 9, oy + 1 + phase, 7, 1, S_ORANGE);
}

static void dash_frame(int ox, int phase) {
    const int oy = 64;
    int k;
    rect(ox, oy + 4, 16, 4, S_METAL_DARK);
    for (k = 0; k < 3; ++k) {
        const int x = ox + 1 + k * 5 + phase * 2;
        put(x, oy + 4, S_ORANGE);
        put(x + 1, oy + 5, S_ORANGE);
        put(x, oy + 6, S_ORANGE);
        put(x + 2, oy + 5, S_WHITE);
    }
}

sat_result_t hsp_art_sheet(sat_surface_t* out) {
    static const uint16_t colours[13] = {
        0, SAT_RGB555(3, 3, 8), SAT_RGB555(5, 11, 28), SAT_RGB555(14, 20, 31), SAT_RGB555(31, 26, 20),
        SAT_RGB555(28, 6, 6), SAT_RGB555(31, 31, 31), SAT_RGB555(31, 27, 0), SAT_RGB555(24, 17, 0),
        SAT_RGB555(20, 20, 21), SAT_RGB555(11, 11, 12), SAT_RGB555(7, 27, 12), SAT_RGB555(31, 17, 4)};
    uint32_t i;
    int k;
    for (i = 0; i < sizeof(g_sheet); ++i) g_sheet[i] = S_CLEAR;
    for (i = 0; i < 256u; ++i) g_sheet_palette[i] = i < 13u ? colours[i] : 0u;

    hero_frame(0, 0);
    for (k = 1; k <= 4; ++k) hero_frame(k, k);
    for (k = 0; k < 4; ++k) hero_frame(5 + k, 5 + k);
    for (k = 0; k < 4; ++k) ring_frame(k);
    for (k = 0; k < 3; ++k) sparkle_frame(k);
    spring_frame(112, 0);
    spring_frame(128, 1);
    flag_frame(144, 0);
    flag_frame(160, 2);
    dash_frame(176, 0);
    dash_frame(192, 1);
    return sat_surface_init(out, g_sheet, HSP_SHEET_W, HSP_SHEET_H, HSP_SHEET_W, SAT_PIXEL_INDEX8, g_sheet_palette, 256u);
}
