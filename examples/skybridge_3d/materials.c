/* Skybridge 3D: world palette, shared solid-colour pool and deck inset textures. */
#include "skybridge.h"

static const uint16_t g_world_colors[SB_WORLD_COLOR_COUNT]={
#define SB_PALETTE_RGB(name,r,g,b) SAT_RGB555(r,g,b),
    SB_WORLD_PALETTE(SB_PALETTE_RGB)
#undef SB_PALETTE_RGB
};
static sat_vdp1_texture_t g_tile_textures[3];
/* 2x2 8x8 slices of each 16x16 patterned top inset, uploaded at startup.
 * Extra VRAM: 3 themes * 4 tiles * 64 bytes = 768 bytes (INDEX8). */
static sat_vdp1_texture_t g_tile_quadrants[3][4];
sat_indexed_tiled_quad3_t g_tile_regions[3];
/* Average colour of each slice: a slice cut by the near plane is drawn as a
 * clipped solid of this colour instead of leaving a hole in the pattern. */
static uint16_t g_tile_quadrant_rgb[3][4];
static uint8_t g_tile_quadrant_pixels[8u*8u];
/* One bounded colour/texture pool across world, gems and pig. Local pig
 * shade-to-material view keeps the imported animation's immutable shade IDs. */
static sat_vdp1_texture_t g_solid_textures[SCENE_MATERIAL_CAP];
sat_scene3d_material_t g_scene_materials[SCENE_MATERIAL_CAP];
sat_scene3d_material_t g_pig_materials[SKYBRIDGE_PIG_SHADE_COUNT];
static uint16_t g_solid_colors[SCENE_MATERIAL_CAP];
static uint8_t g_solid_pixels[8u*8u];
sat_scene3d_solid_pool_t g_solid_pool;
static uint8_t g_tile_pixels[16u*16u];

void init_tile_texture(void) {
    static const uint8_t banks[3]={3u,5u,6u};
    static const uint8_t colors[3][3][3]={
        {{9u,23u,18u},{17u,28u,22u},{26u,30u,27u}},
        {{8u,18u,24u},{13u,25u,28u},{23u,29u,30u}},
        {{21u,17u,9u},{27u,23u,14u},{31u,29u,22u}}
    };
    uint16_t palette[256];
    uint16_t x,y;
    uint8_t theme;
    /* Broad 4x4 paving motifs, a quiet border and sparse highlights:
     * distinguish the three regions without a noisy repeating checker. */
    for (y=0u;y<16u;++y) for (x=0u;x<16u;++x) {
        uint8_t cell=(uint8_t)(((x>>2u)+(y>>2u))&1u);
        uint8_t grout=(uint8_t)((x&7u)==0u || (y&7u)==0u);
        uint8_t glint=(uint8_t)((x==5u && y==4u)||(x==13u && y==12u));
        g_tile_pixels[y*16u+x]=glint?3u:(grout?2u:(uint8_t)(1u+cell));
    }
    for(theme=0u;theme<3u;++theme) {
        for(x=0u;x<256u;++x)palette[x]=SAT_BGR555(0,0,0);
        for(x=0u;x<3u;++x)
            palette[x+1u]=SAT_RGB555(colors[theme][x][0],
                                     colors[theme][x][1],
                                     colors[theme][x][2]);
        sat_example_must(sat_tex_upload_indexed8(
            &g_tile_textures[theme],g_tile_pixels,16u,16u,palette,banks[theme]));
        /* The renderer's asset preparation owns source-region packing.
         * No example-local crop/stride logic, and no per-frame VRAM writes. */
        sat_example_must(sat_upload_indexed8_grid(
            g_tile_pixels,16u,16u,16u,banks[theme],2u,
            g_tile_quadrants[theme],g_tile_quadrant_pixels,
            sizeof(g_tile_quadrant_pixels),palette,g_tile_quadrant_rgb[theme]));
        g_tile_regions[theme].full=&g_tile_textures[theme];
        g_tile_regions[theme].grid=2u;
        g_tile_regions[theme].cells=g_tile_quadrants[theme];
        g_tile_regions[theme].cell_rgb555=g_tile_quadrant_rgb[theme];
    }
}
void init_scene_materials(void) {
    sat_example_must(sat_scene3d_solid_pool_init(
        &g_solid_pool,g_scene_materials,g_solid_textures,
        g_solid_colors,g_solid_pixels,SCENE_MATERIAL_CAP,FADE_PALETTE_BANK));
    /* Fade colours are registered first; world/gem material selectors share
     * the same handles and every colour is uploaded once, regardless of
     * how many platform faces, gems or pig shades reference it. */
    for(uint16_t i=0u;i<SB_WORLD_COLOR_COUNT;++i) {
        uint16_t handle=0u;
        sat_example_must(sat_scene3d_solid_pool_register(
            &g_solid_pool,g_world_colors[i],&handle));
        /* Registration order IS the SB_C_* order, so a palette name can be
         * used directly as a material index. The pool deduplicates, so two
         * identical colours in the list would silently shift every later
         * name; fail loudly instead. */
        sat_example_must(handle==i?SAT_OK:SAT_ERR_INVALID_ARG);
    }
    for(uint16_t i=0u;i<SKYBRIDGE_PIG_SHADE_COUNT;++i) {
        uint16_t handle=0u;
        sat_example_must(sat_scene3d_solid_pool_register(
            &g_solid_pool,skybridge_pig_shade_palette[i],&handle));
        g_pig_materials[i]=g_scene_materials[handle];
    }
    sat_example_must(sat_scene3d_solid_pool_upload_palette(&g_solid_pool));
}
