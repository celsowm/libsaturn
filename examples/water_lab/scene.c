#include "lab.h"

#include "example_util.h"
#include "saturn/color.h"
#include "saturn/vdp2_alloc.h"
#include "saturn/water.h"

#define STONE SAT_RGB555(17, 15, 12)
#define LIGHT_STONE SAT_RGB555(23, 20, 15)
#define SHADE SAT_RGB555(11, 10, 11)
#define BRONZE SAT_RGB555(25, 17, 8)

enum { FLOOR_ROWS = 8, TILE_SIZE = 8 };

static sat_vdp1_texture_t mosaic;
static sat_vdp1_texture_t submerged_mosaic;
static sat_vdp1_texture_t rgb_mosaic;
static uint8_t mosaic_pixels[TILE_SIZE * TILE_SIZE];
static uint16_t mosaic_palette[256];

typedef struct lab_geometry {
    sat_scene_t* scene;
    const lab_water_t* water;
    uint32_t tick;
    sat_fx16_t shear;
    uint16_t faces;
} lab_geometry_t;

static uint16_t unsupported_crossings;

uint16_t lab_unsupported_crossings(void) { return unsupported_crossings; }

static void underwater(lab_geometry_t* g, const sat_quad3_t* quad,
                       const sat_scene3d_material_t* material) {
    sat_water_piece_t pieces[SAT_WATER_SPLIT_PIECES_MAX];
    uint8_t count;
    const sat_fx16_t surface = g->water->height;
    sat_example_must(sat_water_split_quad(quad, surface, pieces, &count));
    for (uint8_t i = 0; i < count; ++i) {
        sat_water_piece_t* piece = &pieces[i];
        sat_scene3d_material_t shaded = *material;
        uint16_t gouraud[4];
        if (piece->submerged) {
            for (unsigned j = 0; j < 4; ++j) {
                sat_vec3_t* v = &piece->quad.v[j];
                const sat_fx16_t depth = surface - v->y;
                const sat_fx16_t strength = depth > SAT_FX16_ONE ? SAT_FX16_ONE : depth;
                const sat_fx16_t phase = 42 * (v->x + v->z) + LAB_UNIT(g->tick % 360u * 4u);
                const sat_fx16_t light = sat_sin_deg(phase) + SAT_FX16_ONE;
                const int caustic = sat_fx16_mul(light, strength) / (SAT_FX16_ONE / 3);
                const int tint = 5 * strength / SAT_FX16_ONE;
                gouraud[j] = SAT_RGB555(16 - tint, 16 + caustic, 16 + tint + caustic);
                /* This affine shear vanishes at the shared waterline. */
                v->x += sat_fx16_mul(depth, g->shear);
            }
            shaded.vertex_gouraud = gouraud;
        }
        sat_example_must(sat_scene_submit_quad(g->scene, &piece->quad, &shaded, 0));
    }
}

static void submit(lab_geometry_t* g, const sat_quad3_t* quad,
                   const sat_scene3d_material_t* material) {
    ++g->faces;
    const int outside_front = quad->v[0].z > LAB_UNIT(8) && quad->v[1].z > LAB_UNIT(8) &&
                              quad->v[2].z > LAB_UNIT(8) && quad->v[3].z > LAB_UNIT(8);
    if (!outside_front && (g->water->mode == LAB_UNDERWATER || g->water->mode == LAB_POOL)) {
        if (material->kind == SAT_SCENE3D_RGB && !material->vertex_gouraud) {
            underwater(g, quad, material);
            return;
        }
        unsigned above = 0;
        unsigned below = 0;
        for (unsigned i = 0; i < 4; ++i) {
            above |= quad->v[i].y > g->water->height;
            below |= quad->v[i].y < g->water->height;
        }
        unsupported_crossings += above && below;
        if (below && !above && material->texture == &mosaic) {
            sat_scene3d_material_t tinted = *material;
            tinted.texture = g->water->mode == LAB_POOL ? &rgb_mosaic : &submerged_mosaic;
            tinted.rgb555 = SAT_RGB555(6, 12, 16);
            sat_example_must(sat_scene_submit_quad(g->scene, quad, &tinted, 0));
            return;
        }
    }
    sat_example_must(sat_scene_submit_quad(g->scene, quad, material, 0));
}

static void solid(lab_geometry_t* g, sat_vec3_t a, sat_vec3_t b,
                  sat_vec3_t c, sat_vec3_t d, uint16_t color) {
    const sat_quad3_t quad = {{a, b, c, d}};
    const sat_scene3d_material_t material = {
        .kind = SAT_SCENE3D_RGB, .rgb555 = color,
        .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE
    };
    submit(g, &quad, &material);
}

static sat_quad3_t horizontal(sat_fx16_t x0, sat_fx16_t x1, sat_fx16_t y,
                              sat_fx16_t z0, sat_fx16_t z1) {
    return (sat_quad3_t){{{x0,y,z0}, {x1,y,z0}, {x1,y,z1}, {x0,y,z1}}};
}

static void box(lab_geometry_t* g, sat_vec3_t lo, sat_vec3_t hi, uint16_t top) {
    const sat_quad3_t q = horizontal(lo.x, hi.x, hi.y, lo.z, hi.z);
    const sat_vec3_t a = {lo.x,lo.y,lo.z};
    const sat_vec3_t b = {hi.x,lo.y,lo.z};
    const sat_vec3_t c = {hi.x,lo.y,hi.z};
    const sat_vec3_t d = {lo.x,lo.y,hi.z};
    solid(g, q.v[0], q.v[1], q.v[2], q.v[3], top);
    solid(g, a, b, q.v[1], q.v[0], STONE);
    solid(g, b, c, q.v[2], q.v[1], SHADE);
    solid(g, c, d, q.v[3], q.v[2], STONE);
    solid(g, d, a, q.v[0], q.v[3], SHADE);
}

static void stone_box(lab_geometry_t* g, int x0, int y0, int z0,
                      int x1, int y1, int z1, uint16_t top) {
    box(g, (sat_vec3_t){LAB_UNIT(x0), LAB_UNIT(y0), LAB_UNIT(z0)},
        (sat_vec3_t){LAB_UNIT(x1), LAB_UNIT(y1), LAB_UNIT(z1)}, top);
}

void lab_scene_init(void) {
    sat_vdp2_palette_range_t dry_palette, wet_palette;
    sat_example_must(sat_vdp2_plan_begin());
    sat_example_must(sat_vdp2_plan_palette_reserve(256, &dry_palette));
    sat_example_must(sat_vdp2_plan_palette_reserve(256, &wet_palette));
    sat_example_must(sat_vdp2_plan_commit());
    for (unsigned y = 0; y < TILE_SIZE; ++y) {
        for (unsigned x = 0; x < TILE_SIZE; ++x) {
            const unsigned dx = x < 4 ? x : 7 - x;
            const unsigned dy = y < 4 ? y : 7 - y;
            mosaic_pixels[y * TILE_SIZE + x] = dx + dy == 3 ? 1 : 2 + ((x ^ y) & 1u);
        }
    }
    mosaic_palette[1] = SAT_RGB555(15, 21, 19);
    mosaic_palette[2] = SAT_RGB555(9, 13, 14);
    mosaic_palette[3] = SAT_RGB555(10, 14, 15);
    sat_example_must(sat_tex_upload_indexed8(
        &mosaic, mosaic_pixels, TILE_SIZE, TILE_SIZE, mosaic_palette, dry_palette.bank));
    submerged_mosaic = mosaic;
    submerged_mosaic.palette = wet_palette.bank;
    mosaic_palette[1] = SAT_RGB555(10, 20, 23);
    mosaic_palette[2] = SAT_RGB555(6, 12, 16);
    mosaic_palette[3] = SAT_RGB555(7, 13, 18);
    sat_example_must(sat_palette_upload_indexed8(mosaic_palette, submerged_mosaic.palette));

    /* An RGB lookup lets VDP1 half-transparency retain the mosaic detail. */
    uint16_t lut;
    uint8_t packed[sizeof(mosaic_pixels) / 2];
    for (unsigned i = 0; i < sizeof(packed); ++i) {
        packed[i] = (uint8_t)((mosaic_pixels[2 * i] << 4) | mosaic_pixels[2 * i + 1]);
    }
    sat_example_must(sat_vdp1_upload_lut(mosaic_palette, &lut));
    sat_example_must(sat_tex_upload_lut4_pixels(&rgb_mosaic, packed, TILE_SIZE, TILE_SIZE, lut));
}

static void temple(lab_geometry_t* g) {
    /* North wall and lintel frame the recessed tunnel. */
    stone_box(g, -12, -3, -11, -2, 5, -10, LIGHT_STONE);
    stone_box(g, 2, -3, -11, 12, 5, -10, LIGHT_STONE);
    stone_box(g, -2, 0, -11, 2, 4, -8, BRONZE);
    const sat_quad3_t recess = {{{LAB_UNIT(-2),LAB_UNIT(-3),LAB_UNIT(-13)},
        {LAB_UNIT(2),LAB_UNIT(-3),LAB_UNIT(-13)}, {LAB_UNIT(2),0,LAB_UNIT(-13)},
        {LAB_UNIT(-2),0,LAB_UNIT(-13)}}};
    const sat_scene3d_material_t dark = {
        .kind = SAT_SCENE3D_RGB, .rgb555 = SAT_RGB555(3, 6, 9),
        .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE
    };
    submit(g, &recess, &dark);
    stone_box(g, -12, -3, -10, -8, 0, 10, LIGHT_STONE);
    stone_box(g, 8, -3, -10, 12, 0, 10, LIGHT_STONE);
    /* Split the long front lip so its paint depth tracks the adjacent water
     * across the orbit. The side walls close both of its ends. */
    for (int x = -8; x < 8; x += 4) {
        const sat_fx16_t a = LAB_UNIT(x), b = LAB_UNIT(x + 4);
        const sat_fx16_t near = LAB_UNIT(9), far = LAB_UNIT(8), bottom = LAB_UNIT(-3);
        const sat_quad3_t top = horizontal(a, b, 0, far, near);
        solid(g, top.v[0], top.v[1], top.v[2], top.v[3], LIGHT_STONE);
        solid(g, (sat_vec3_t){a,bottom,near}, (sat_vec3_t){b,bottom,near},
              top.v[2], top.v[3], STONE);
        solid(g, (sat_vec3_t){b,bottom,far}, (sat_vec3_t){a,bottom,far},
              top.v[0], top.v[1], SHADE);
    }
    stone_box(g, -8, -3, -10, -2, 0, -8, STONE);
    stone_box(g, 2, -3, -10, 8, 0, -8, STONE);
    const sat_quad3_t tunnel_floor = horizontal(
        LAB_UNIT(-2), LAB_UNIT(2), LAB_UNIT(-3), LAB_UNIT(-13), LAB_UNIT(-8));
    const sat_scene3d_material_t bed = {
        .kind = SAT_SCENE3D_RGB, .rgb555 = SHADE,
        .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE
    };
    submit(g, &tunnel_floor, &bed);
    stone_box(g, -2, -3, -2, 2, 0, 1, LIGHT_STONE);
    stone_box(g, -1, -3, 1, 1, -1, 2, BRONZE);
    stone_box(g, -1, -3, 2, 1, -2, 3, STONE);

    for (int x = -7; x <= 7; x += 14) {
        stone_box(g, x, -3, -6, x + 1, 4, -5, BRONZE);
        stone_box(g, x, -3, 1, x + 1, 3, 2, BRONZE);
    }
    const sat_scene3d_material_t glass = {
        .kind = SAT_SCENE3D_RGB, .rgb555 = SAT_RGB555(11, 23, 24),
        .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE,
        .sprite_flags = g->water->mode == LAB_MESH ? SAT_SPRITE_FLAG_MESH :
            g->water->mode >= LAB_HALF ? SAT_SPRITE_FLAG_HALF_TRANSPARENT : 0
    };
    for (int x = -6; x <= 6; x += 12) {
        const sat_fx16_t edge = LAB_UNIT(x);
        const sat_quad3_t panel = {{{edge,LAB_UNIT(-2),LAB_UNIT(-9)},
            {edge,LAB_UNIT(-2),LAB_UNIT(-3)}, {edge,LAB_UNIT(2),LAB_UNIT(-3)},
            {edge,LAB_UNIT(2),LAB_UNIT(-9)}}};
        submit(g, &panel, &glass);
    }
    const sat_scene3d_material_t seal = {
        .kind = SAT_SCENE3D_INDEXED_TEXTURED, .rgb555 = BRONZE,
        .texture = &mosaic, .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE,
        .sprite_flags = g->water->mode == LAB_MULTIPASS ? SAT_SPRITE_FLAG_OPAQUE : 0
    };
    const sat_quad3_t door_seal = {{{LAB_UNIT(5),LAB_UNIT(-2),LAB_UNIT(-7)},
        {LAB_UNIT(6),LAB_UNIT(-2),LAB_UNIT(-7)}, {LAB_UNIT(6),LAB_UNIT(2),LAB_UNIT(-7)},
        {LAB_UNIT(5),LAB_UNIT(2),LAB_UNIT(-7)}}};
    submit(g, &door_seal, &seal);
}

static void actors(lab_geometry_t* g, const lab_player_t* player, uint32_t tick) {
    const sat_fx16_t half = SAT_FX16_ONE / 3;
    const sat_vec3_t p = player->position;
    box(g, (sat_vec3_t){p.x-half, p.y, p.z-half},
        (sat_vec3_t){p.x+half, p.y+LAB_UNIT(1), p.z+half}, SAT_RGB555(31, 24, 10));
    const sat_fx16_t phase = LAB_UNIT(tick % 360u);
    const sat_fx16_t x = 5 * sat_sin_deg(phase);
    const sat_fx16_t y = 2 * sat_cos_deg(phase);
    box(g, (sat_vec3_t){x-half,y-half,LAB_UNIT(4)-half},
        (sat_vec3_t){x+half,y+half,LAB_UNIT(4)+half}, SAT_RGB555(31, 12, 7));
    for (unsigned i = 0; i < 3; ++i) {
        const sat_fx16_t py = LAB_UNIT(-3) +
            (sat_fx16_t)((tick + i * 24u) % 72u) * (SAT_FX16_ONE / 18);
        const sat_fx16_t px = LAB_UNIT(-4) + (sat_fx16_t)i * half;
        solid(g, (sat_vec3_t){px,py,LAB_UNIT(1)},
                 (sat_vec3_t){px+half/2,py,LAB_UNIT(1)},
                 (sat_vec3_t){px+half/2,py+half/2,LAB_UNIT(1)},
                 (sat_vec3_t){px,py+half/2,LAB_UNIT(1)}, SAT_RGB555(31, 25, 12));
    }
}

static void floor_tiles(lab_geometry_t* g, unsigned count) {
    const sat_scene3d_material_t tile = {
        .kind = SAT_SCENE3D_INDEXED_TEXTURED, .rgb555 = SAT_RGB555(10, 14, 15),
        .texture = &mosaic, .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE,
        .sprite_flags = g->water->mode == LAB_MULTIPASS ? SAT_SPRITE_FLAG_OPAQUE : 0
    };
    const sat_scene3d_material_t stone = {
        .kind = SAT_SCENE3D_RGB, .rgb555 = SAT_RGB555(12, 16, 15),
        .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE
    };
    for (unsigned row = 0; row < FLOOR_ROWS; ++row) {
        const unsigned columns = count / FLOOR_ROWS + (row < count % FLOOR_ROWS);
        const sat_fx16_t z = LAB_UNIT(-8) + LAB_UNIT(2) * (int)row;
        for (unsigned column = 0; column < columns; ++column) {
            const sat_fx16_t a = LAB_UNIT(-8) + (sat_fx16_t)(column * LAB_UNIT(16) / columns);
            const sat_fx16_t b = LAB_UNIT(-8) + (sat_fx16_t)((column + 1u) * LAB_UNIT(16) / columns);
            const sat_quad3_t quad = horizontal(a, b, LAB_UNIT(-3), z, z+LAB_UNIT(2));
            submit(g, &quad, row % 4 == 0 ? &stone : &tile);
        }
    }
}

void lab_scene_draw(sat_scene_t* scene, const lab_player_t* player,
                    uint32_t tick, uint16_t workload, const lab_water_t* water) {
    const sat_fx16_t shear = water->mode == LAB_UNDERWATER
        ? sat_sin_deg(LAB_UNIT((tick % 360u) * 3u)) / 9 : 0;
    lab_geometry_t g = {scene, water, tick, shear, 0};
    unsupported_crossings = 0;
    temple(&g);
    actors(&g, player, tick);
    floor_tiles(&g, workload - g.faces);
}
