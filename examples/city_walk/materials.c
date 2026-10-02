/* city_walk materials: the archive's shade table -> one solid-colour pool.
 *
 * Every entry of the archive's material table is one already-lit colour (the
 * chunker bakes a Lambert shade per face), so a face needs no lighting at
 * runtime: it is just an index. The pool draws each as an 8x8 INDEX8 sprite
 * stretched over the polygon, which is what gives VDP2 colour-calc slots (for
 * fading the far ring into the sky) that an RGB polygon does not have.
 */
#include <stdint.h>

#include "example_util.h"

#include "city_walk.h"

static sat_vdp1_texture_t g_textures[CITY_MATERIAL_MAX];
static sat_scene3d_material_t g_materials[CITY_MATERIAL_MAX];
static uint16_t g_colors[CITY_MATERIAL_MAX];
static uint8_t g_pixels[64];
static sat_scene3d_solid_pool_t g_pool;

/* Archive material index -> pool handle. The pool merges identical colours, so
 * two shades that quantise to the same RGB555 share a handle and later ones
 * shift down; the decoder translates through this table. */
uint16_t g_material_map[CITY_MATERIAL_MAX];

void materials_init(void) {
    const uint16_t count = g_archive.header.material_count;
    sat_example_must(sat_scene3d_solid_pool_init(&g_pool, g_materials, g_textures, g_colors,
                                                    g_pixels, CITY_MATERIAL_MAX,
                                                    CITY_PALETTE_SOLID));
    for (uint16_t i = 0u; i < count; ++i) {
        uint16_t handle = 0u;
        /* The table stores BGR555 without the RGB code bit; a VDP1 colour
         * needs it, and the pool's palette entry is derived from this value. */
        uint16_t rgb = (uint16_t)(0x8000u | city_be16(g_archive.materials + i * CITY_MATERIAL_ENTRY_BYTES));
        sat_example_must(sat_scene3d_solid_pool_register(&g_pool, rgb, &handle));
        g_material_map[i] = handle;
    }
    sat_example_must(sat_scene3d_solid_pool_upload_palette(&g_pool));
}

uint16_t materials_rgb(uint16_t handle) {
    return handle < g_pool.count ? g_colors[handle] : (uint16_t)0x8000u;
}

const sat_scene3d_material_t* materials_table(uint16_t* out_count) {
    *out_count = g_pool.count;
    return g_materials;
}
