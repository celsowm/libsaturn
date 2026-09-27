/* city_walk sky: a 512x128 procedural gradient on NBG0, behind the RBG0 ground.
 *
 * The image repeats every 512 dots. One full turn is 360 degrees of a 73.7
 * degree horizontal field of view across 320 dots, i.e. ~1563 dots, so the sky
 * scrolls 4.34 dots per degree of yaw and tiles ~3 times per turn. Turning left
 * (yaw growing) sweeps the scene right, so the sky scrolls the other way.
 *
 * The bottom image row sits on the horizon row (112): the scroll offset is
 * `height - horizon - 1`, so screen row 112 shows image row 127.
 */
#include <stdint.h>

#include "saturn/example_util.h"
#include "saturn/vdp2.h"

#include "city_walk.h"

#define SKY_W 512u
#define SKY_H 128u
#define SKY_BANDS 24u
#define SKY_HORIZON_ROW 112u
#define SKY_DOTS_PER_DEGREE_FX 284426 /* 4.34 in 16.16 */

/* Deterministic hash for cloud placement: no rand(), the output is the same on
 * every boot. */
static uint32_t hash2(uint32_t x, uint32_t y) {
    uint32_t h = x * 0x9E3779B9u ^ y * 0x85EBCA6Bu;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}

/* scratch: SKY_W * SKY_H pixel bytes, then the 64x64 pattern-name map. */
void background_init(uint8_t* scratch, uint32_t scratch_bytes) {
    static uint16_t palette[256];
    uint8_t* pixels = scratch;
    uint16_t* map = (uint16_t*)(scratch + SKY_W * SKY_H);
    const sat_vdp2_nbg0_config_t sky = {SAT_VDP2_CHAR_SIZE_1X1, SAT_VDP2_COLOR_MODE_256, 0x3Bu, 0u, 0u};

    if (scratch_bytes < SKY_W * SKY_H + SAT_VDP2_NBG0_MAP_CELLS * 2u) sat_example_must(SAT_ERR_CAPACITY);
    /* Colours: 0 unused, 1..SKY_BANDS the gradient from zenith to horizon,
     * then a white-ish cloud shade. */
    for (uint32_t i = 0u; i < 256u; ++i) palette[i] = SAT_BGR555(0, 0, 0);
    for (uint32_t b = 0u; b < SKY_BANDS; ++b) {
        uint32_t t = b * 31u / (SKY_BANDS - 1u);
        palette[1u + b] = SAT_BGR555(6u + (t * 13u) / 31u, 12u + (t * 12u) / 31u, 22u + (t * 8u) / 31u);
    }
    palette[SKY_BANDS + 1u] = SAT_BGR555(29, 30, 31);
    palette[SKY_BANDS + 2u] = SAT_BGR555(26, 28, 31);
    for (uint32_t y = 0u; y < SKY_H; ++y) {
        uint8_t band = (uint8_t)(1u + (y * (SKY_BANDS - 1u)) / (SKY_H - 1u));
        for (uint32_t x = 0u; x < SKY_W; ++x) pixels[y * SKY_W + x] = band;
    }
    /* Clouds: flat-bottomed puffs in the lower two thirds of the sky. */
    for (uint32_t i = 0u; i < 9u; ++i) {
        uint32_t h = hash2(i, 77u);
        int32_t cx = (int32_t)(h % SKY_W);
        int32_t cy = 34 + (int32_t)((h >> 9) % 60u);
        int32_t rx = 22 + (int32_t)((h >> 17) % 26u);
        int32_t ry = 4 + (int32_t)((h >> 23) % 5u);
        for (int32_t dy = -ry; dy <= 0; ++dy) {
            int32_t half = rx * (ry + dy) / ry + rx / 4;
            for (int32_t dx = -half; dx <= half; ++dx) {
                int32_t px = (cx + dx) & (int32_t)(SKY_W - 1u);
                int32_t py = cy + dy;
                if (py >= 0 && py < (int32_t)SKY_H) {
                    pixels[py * SKY_W + px] = (uint8_t)(SKY_BANDS + 1u + (dy < -ry / 2 ? 0u : 1u));
                }
            }
        }
    }
    sat_example_must(sat_vdp2_palette_upload(palette, 256u, CITY_PALETTE_SKY * 256u));
    sat_example_must(sat_vdp2_nbg0_init(&sky));
    sat_example_must(sat_vdp2_nbg0_upload_indexed8(pixels, SKY_W, SKY_H, CITY_PALETTE_SKY, map));
    /* Behind the RBG0 ground (priority 5) and the VDP1 sprites (7). */
    sat_example_must(sat_vdp2_nbg0_set_priority(2u));
}

void background_update(sat_fx16_t yaw) {
    /* Whole dots: the sky is far away and a one-dot step is a sub-pixel angle. */
    int32_t dots = (int32_t)(((int64_t)yaw * SKY_DOTS_PER_DEGREE_FX) >> 32);
    const sat_vdp2_scroll_t scroll = {
        (uint16_t)((uint32_t)(-dots) & (SKY_W - 1u)), 0u,
        (uint16_t)(SKY_H - SKY_HORIZON_ROW - 1u), 0u
    };
    sat_example_must(sat_vdp2_nbg0_set_scroll(&scroll));
}
