#include "example_util.h"
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/input.h"
#include "saturn/mesh3d.h"
#include "saturn/render3d.h"
#include "saturn/scene.h"
#include "saturn/time.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2_layers.h"
#include "saturn/water_actor.h"
#include "saturn/water_plane.h"
#include "saturn/water_prepare.h"
#include "saturn/water_regions.h"
#include "saturn/water_view.h"

/* A lake with shores, an island, a bridge and a deep bed, drawn two ways on
 * one scripted camera route. NAIVE models the water as four large quads and
 * lets the painter sort everything by depth, as a level would. FULL draws a
 * camera-centred water plane, the level prepared against the water region,
 * the actor cut at the waterline and the underwater view. Telemetry records
 * the cost of each 30-tick stretch of the route. */

#define UNIT(n) ((n) * SAT_FX16_ONE)

enum {
    WIDTH = 320, HEIGHT = 224,
    FACE_CAPACITY = 1536, OVERLAY_COMMANDS = 64,
    GRID = 16, GRID_VERTICES = (GRID + 1) * (GRID + 1), CELL = 5,
    BRIDGE_SEGMENTS = 10, POSTS = 9,
    LEVEL_VERTICES = GRID_VERTICES + BRIDGE_SEGMENTS * 4 + POSTS * 8,
    LEVEL_FACES = GRID * GRID + BRIDGE_SEGMENTS + POSTS * 2,
    PREPARED_VERTICES = 2560, PREPARED_FACES = 1024,
    SPHERE_SEGMENTS = 12, SPHERE_RINGS = 8,
    SPHERE_VERTICES = (SPHERE_RINGS + 1) * SPHERE_SEGMENTS,
    SPHERE_FACES = SPHERE_RINGS * SPHERE_SEGMENTS,
    SECTION_TICKS = 30, SECTIONS = 13, ROUTE_TICKS = SECTION_TICKS * SECTIONS,
    MODE_NAIVE = 0, MODE_FULL = 1,
    WATER_TILE = 8, WATER_TILES = 6   /* NAIVE textured water: 6 x 6 tiles of 10 units */
};

static const sat_fx16_t SURFACE = 0, BED = -UNIT(10), LAKE_HALF = UNIT(30);
static const sat_fx16_t RADIUS = SAT_FX16_ONE * 3 / 2;
static const sat_fx16_t DECK = UNIT(3);
#define WATER_COLOR SAT_RGB555(9, 23, 28)

typedef struct section_record {
    uint32_t submitted, culled, clipped, fallback, skipped, commands;
    uint32_t preparation_ticks, flush_ticks;
} section_record_t;

/* Big-endian uint32_t words, read through the Ymir harness using the ELF symbol. */
volatile struct {
    uint32_t tick, mode, script, paused;
    uint32_t underwater, fade, region, actor_fraction;
    uint32_t plane_faces, above_faces, below_faces, cut_pieces;
    uint32_t tick_frequency;
    uint32_t submit_errors, last_error;  /* NAIVE submissions the scene refused */
    /* Ticks spent in each stage of the last frame: level, water (build for
     * FULL), water submit, actor split, actor submit. */
    uint32_t stage[5];
    /* Over the replay, counted per route tick: ticks the camera spent under water
     * and the largest fade reached. */
    uint32_t underwater_ticks, max_fade;
    uint32_t textured;      /* the water is drawn with a texture instead of a flat colour */
    uint32_t water_off;     /* the water surface is not drawn: isolates its share of the cost */
    uint32_t low;           /* the camera follows the low skim route instead of the main one */
    int32_t actor_x, actor_y, actor_z;
    section_record_t section[SECTIONS];
} water_lake_telemetry;

static sat_scene_t scene;
static sat_scene3d_face_t faces[FACE_CAPACITY];
static uint32_t keys[FACE_CAPACITY];
static uint16_t order[FACE_CAPACITY];

static sat_vec3_t level_vertices[LEVEL_VERTICES];
static uint16_t level_indices[4 * LEVEL_FACES];
static uint16_t level_face_materials[LEVEL_FACES];
static uint16_t level_light[LEVEL_VERTICES];
static sat_mesh_t level_mesh;
static sat_scene3d_material_t level_materials[4];

static sat_vec3_t prepared_vertices[PREPARED_VERTICES];
static uint16_t prepared_light[PREPARED_VERTICES];
static uint16_t prepared_indices[4 * PREPARED_FACES];
static uint16_t prepared_face_materials[PREPARED_FACES];
static sat_vec3_t prepared_scratch_vertices[PREPARED_VERTICES];
static uint16_t prepared_scratch_light[PREPARED_VERTICES], prepared_scratch_remap[PREPARED_VERTICES];
static sat_projected_vertex_t projected[PREPARED_VERTICES];
static sat_water_prepared_t prepared;

static sat_water_volume_t volumes[1];
static sat_water_regions_t regions;
static sat_water_view_t view;
static sat_water_plane_t plane;

static sat_mesh_t sphere;
static sat_vec3_t sphere_vertices[SPHERE_VERTICES];
static uint16_t sphere_indices[4 * SPHERE_FACES];
static sat_vec3_t sphere_normals[SPHERE_VERTICES];
static uint16_t sphere_light[SPHERE_VERTICES];
static sat_vec3_t actor_world[SPHERE_VERTICES];
static sat_mesh_t actor_mesh;
static const uint16_t actor_face_materials[SPHERE_FACES];
static uint16_t actor_above_indices[4 * SPHERE_FACES], actor_below_indices[4 * SPHERE_FACES];
static uint16_t actor_above_materials[SPHERE_FACES], actor_below_materials[SPHERE_FACES];
static uint16_t actor_wet[SPHERE_VERTICES], actor_piece_materials[SPHERE_FACES];
static sat_water_shaded_piece_t actor_pieces[SPHERE_FACES];
static sat_projected_vertex_t actor_scratch[SPHERE_VERTICES];
static sat_water_actor_workspace_t actor_space;
static sat_scene3d_material_t actor_material, water_material, water_textured_material;
static sat_vdp1_texture_t water_texture;

/* ---- the level ------------------------------------------------------- */

static int abs_int(int v) { return v < 0 ? -v : v; }

static int64_t isqrt64(int64_t v) {
    int64_t x = 0, bit = (int64_t)1 << 62;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= x + bit) { v -= x + bit; x = (x >> 1) + bit; }
        else x >>= 1;
        bit >>= 2;
    }
    return x;
}

/* A square basin: a flat bed to 20 units from the centre, a slope up through
 * the shoreline at 30, then land. An island rises from the bed at (10, -5). */
static sat_fx16_t terrain_height(int x, int z) {
    const int d = abs_int(x) > abs_int(z) ? abs_int(x) : abs_int(z);
    int32_t h;
    if (d <= 20) h = -UNIT(9);
    else if (d < 30) h = -UNIT(9) + UNIT(9) * (d - 20) / 10;
    else h = UNIT(2) * (d - 30) / 10;
    const int dx = x - 10, dz = z + 5, dist2 = dx * dx + dz * dz;
    if (dist2 < 64) {
        const int dist = (int)isqrt64(dist2);
        const int32_t island = -UNIT(9) + (UNIT(23) / 2) * (8 - dist) / 8;
        if (island > h) h = island;
    }
    return h;
}

static uint16_t terrain_light(int x, int z) {
    const int64_t nx = (int64_t)(terrain_height(x - CELL, z) - terrain_height(x + CELL, z)) / (2 * CELL);
    const int64_t nz = (int64_t)(terrain_height(x, z - CELL) - terrain_height(x, z + CELL)) / (2 * CELL);
    // A 32-bit divisor: the SH-2 build has no 64-bit divide by a 64-bit value.
    const int32_t len = (int32_t)isqrt64(nx * nx + (int64_t)SAT_FX16_ONE * SAT_FX16_ONE + nz * nz);
    const int64_t dot = (nx * 26214 + (int64_t)SAT_FX16_ONE * 52429 + nz * 29491) / len;
    const int shade = (int)((dot - SAT_FX16_ONE * 3 / 4) * 22 / SAT_FX16_ONE);
    return sat_gouraud_rgb(shade, shade, shade);
}

static void add_quad(unsigned* vertex, unsigned* face, sat_vec3_t a, sat_vec3_t b, sat_vec3_t c,
                     sat_vec3_t d, uint16_t material, uint16_t light) {
    const sat_vec3_t corners[4] = {a, b, c, d};
    for (unsigned k = 0; k < 4; ++k) {
        level_vertices[*vertex + k] = corners[k];
        level_light[*vertex + k] = light;
        level_indices[4 * *face + k] = (uint16_t)(*vertex + k);
    }
    level_face_materials[*face] = material;
    *vertex += 4;
    ++*face;
}

static void build_level(void) {
    unsigned vertex = 0, face = 0;
    for (int j = 0; j <= GRID; ++j) {
        for (int i = 0; i <= GRID; ++i, ++vertex) {
            const int x = -8 * CELL + i * CELL, z = -8 * CELL + j * CELL;
            level_vertices[vertex] = (sat_vec3_t){UNIT(x), terrain_height(x, z), UNIT(z)};
            level_light[vertex] = terrain_light(x, z);
        }
    }
    for (int j = 0; j < GRID; ++j) {
        for (int i = 0; i < GRID; ++i, ++face) {
            const uint16_t a = (uint16_t)(j * (GRID + 1) + i);
            level_indices[4 * face] = a;
            level_indices[4 * face + 1] = (uint16_t)(a + 1);
            level_indices[4 * face + 2] = (uint16_t)(a + GRID + 2);
            level_indices[4 * face + 3] = (uint16_t)(a + GRID + 1);
            // By the highest corner: a face reaching the surface is shore, one well
            // above it grass, and only faces wholly under water show the bed.
            sat_fx16_t top = INT32_MIN;
            for (unsigned k = 0; k < 4; ++k) {
                const sat_fx16_t y = level_vertices[level_indices[4 * face + k]].y;
                if (y > top) top = y;
            }
            level_face_materials[face] = top < -UNIT(1) / 2 ? 0 : (top < UNIT(3) / 2 ? 1 : 2);
        }
    }
    const uint16_t wood_light = sat_gouraud_rgb(2, 2, 2);
    for (int s = 0; s < BRIDGE_SEGMENTS; ++s) {
        const sat_fx16_t x0 = UNIT(-40 + 8 * s), x1 = UNIT(-32 + 8 * s);
        add_quad(&vertex, &face, (sat_vec3_t){x0, DECK, UNIT(25) / 2}, (sat_vec3_t){x1, DECK, UNIT(25) / 2},
                 (sat_vec3_t){x1, DECK, UNIT(31) / 2}, (sat_vec3_t){x0, DECK, UNIT(31) / 2}, 3, wood_light);
    }
    for (int p = 0; p < POSTS; ++p) {
        const sat_fx16_t x = UNIT(-32 + 8 * p), z = UNIT(14), w = 26214;
        add_quad(&vertex, &face, (sat_vec3_t){x - w, DECK, z}, (sat_vec3_t){x + w, DECK, z},
                 (sat_vec3_t){x + w, -UNIT(19) / 2, z}, (sat_vec3_t){x - w, -UNIT(19) / 2, z}, 3,
                 sat_gouraud_rgb(-1, -1, -1));
        add_quad(&vertex, &face, (sat_vec3_t){x, DECK, z - w}, (sat_vec3_t){x, DECK, z + w},
                 (sat_vec3_t){x, -UNIT(19) / 2, z + w}, (sat_vec3_t){x, -UNIT(19) / 2, z - w}, 3,
                 sat_gouraud_rgb(-3, -3, -3));
    }
    level_mesh = (sat_mesh_t){
        .vertices = level_vertices, .indices = level_indices, .vertex_cap = LEVEL_VERTICES,
        .vertex_count = (uint16_t)vertex, .face_cap = LEVEL_FACES, .face_count = (uint16_t)face
    };
    static const uint16_t colors[4] = {SAT_RGB555(8, 13, 13), SAT_RGB555(26, 23, 15),
                                       SAT_RGB555(9, 21, 8), SAT_RGB555(17, 11, 6)};
    for (unsigned m = 0; m < 4; ++m) {
        level_materials[m] = (sat_scene3d_material_t){
            .kind = SAT_SCENE3D_RGB, .rgb555 = colors[m],
            .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE
        };
    }
}

static void prepare_level(void) {
    prepared = (sat_water_prepared_t){
        .vertices = prepared_vertices, .gouraud = prepared_light, .indices = prepared_indices,
        .face_materials = prepared_face_materials, .scratch_vertices = prepared_scratch_vertices,
        .scratch_gouraud = prepared_scratch_light, .scratch_remap = prepared_scratch_remap,
        .vertex_cap = PREPARED_VERTICES, .face_cap = PREPARED_FACES
    };
    const sat_water_prepare_config_t config = {
        .regions = &regions, .tint = {-9, -3, 3}, .full_depth = UNIT(6)
    };
    sat_example_must(sat_water_prepare_mesh(&level_mesh, level_face_materials, level_materials, 4,
                                            level_light, &config, &prepared));
}

/* A small ripple pattern: the textured water of NAIVE and the rings of FULL. */
static void make_water_texture(void) {
    uint8_t pixels[WATER_TILE * WATER_TILE], packed[WATER_TILE * WATER_TILE / 2];
    uint16_t colors[16] = {0};
    for (unsigned y = 0; y < WATER_TILE; ++y) {
        for (unsigned x = 0; x < WATER_TILE; ++x) {
            const unsigned wave = (x + 2 * y) & 7u;
            pixels[y * WATER_TILE + x] = wave < 2 ? 3 : (wave < 4 ? 2 : 1);
        }
    }
    colors[1] = SAT_RGB555(8, 20, 27);
    colors[2] = SAT_RGB555(11, 24, 29);
    colors[3] = SAT_RGB555(18, 28, 31);
    for (unsigned i = 0; i < sizeof(packed); ++i) packed[i] = (uint8_t)((pixels[2 * i] << 4) | pixels[2 * i + 1]);
    uint16_t lut;
    sat_example_must(sat_vdp1_upload_lut(colors, &lut));
    sat_example_must(sat_tex_upload_lut4_pixels(&water_texture, packed, WATER_TILE, WATER_TILE, lut));
}

static void make_actor(void) {
    sat_example_must(sat_mesh_init(&sphere, sphere_vertices, SPHERE_VERTICES, sphere_indices, SPHERE_FACES));
    const sat_vec3_t origin = {0, 0, 0};
    sat_example_must(sat_mesh_build_sphere(&sphere, &origin, RADIUS, SPHERE_SEGMENTS, SPHERE_RINGS));
    sat_example_must(sat_mesh_vertex_normals(&sphere, sphere_normals, SPHERE_VERTICES));
    const sat_vec3_t light = {26214, 45875, 26214};
    sat_example_must(sat_gouraud_lambert(sphere_normals, sphere.vertex_count, &light,
                                         SAT_FX16_ONE / 3, sphere_light));
    actor_mesh = (sat_mesh_t){
        .vertices = actor_world, .indices = sphere_indices, .vertex_cap = SPHERE_VERTICES,
        .vertex_count = SPHERE_VERTICES, .face_cap = SPHERE_FACES, .face_count = SPHERE_FACES
    };
    actor_space = (sat_water_actor_workspace_t){
        .above_indices = actor_above_indices, .above_materials = actor_above_materials,
        .below_indices = actor_below_indices, .below_materials = actor_below_materials,
        .wet_light = actor_wet, .pieces = actor_pieces, .piece_materials = actor_piece_materials,
        .scratch = actor_scratch, .face_cap = SPHERE_FACES, .vertex_cap = SPHERE_VERTICES,
        .piece_cap = SPHERE_FACES
    };
    actor_material = (sat_scene3d_material_t){
        .kind = SAT_SCENE3D_RGB, .rgb555 = SAT_RGB555(31, 21, 8),
        .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE, .vertex_gouraud = sphere_light
    };
    water_material = (sat_scene3d_material_t){
        .kind = SAT_SCENE3D_RGB, .rgb555 = WATER_COLOR,
        .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE, .sprite_flags = SAT_SPRITE_FLAG_MESH
    };
    // A textured face with a corner behind the near plane is dropped unless it
    // has a fallback colour; the water colour makes it a flat polygon instead.
    water_textured_material = (sat_scene3d_material_t){
        .kind = SAT_SCENE3D_INDEXED_TEXTURED, .rgb555 = WATER_COLOR, .texture = &water_texture,
        .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE
    };
}

/* ---- the camera route and the actor ---------------------------------- */

typedef struct waypoint {
    int16_t tick;
    int16_t eye[3], target[3];
} waypoint_t;

/* High orbit, a skim over the water, the bridge, then a dive and a rise. */
static const waypoint_t route[] = {
    {0, {30, 20, 30}, {0, -2, 0}},    {90, {-30, 20, 30}, {0, -2, 0}},
    {135, {-36, 2, -6}, {0, 0, -6}},  {180, {24, 2, -6}, {40, 0, -6}},
    {225, {-34, 5, 14}, {0, 3, 14}},  {270, {30, 5, 14}, {40, 3, 14}},
    {300, {0, 3, 6}, {0, -2, -2}},    {330, {0, -4, 4}, {0, -4, -10}},
    {360, {6, -6, 0}, {-4, -4, -10}}, {ROUTE_TICKS, {0, 3, 6}, {0, -2, -2}}
};

/* A second route that stays a quarter of a unit above the water and looks along
 * it: the grazing view where the plane's near cells straddle the near plane.
 * Coordinates are in quarter units. */
static const waypoint_t route_low[] = {
    {0, {-120, 1, -24}, {0, 0, -24}},   {130, {100, 1, -24}, {160, 0, -24}},
    {260, {100, 1, 24}, {-40, 0, 24}},  {ROUTE_TICKS, {-100, 1, 24}, {40, 0, 0}}
};

static sat_fx16_t lerp_int(int a, int b, int num, int den) {
    return (sat_fx16_t)((int64_t)UNIT(a) + (int64_t)UNIT(b - a) * num / den);
}

static void route_pose(uint32_t tick, uint8_t low, sat_vec3_t* eye, sat_vec3_t* target) {
    const waypoint_t* table = low ? route_low : route;
    const unsigned count = low ? sizeof(route_low) / sizeof(route_low[0]) : sizeof(route) / sizeof(route[0]);
    unsigned s = 0;
    while (s + 2 < count && (int)tick >= table[s + 1].tick) ++s;
    const waypoint_t* a = &table[s];
    const waypoint_t* b = &table[s + 1];
    const int num = (int)tick - a->tick > b->tick - a->tick ? b->tick - a->tick : (int)tick - a->tick;
    const int den = b->tick - a->tick;
    *eye = (sat_vec3_t){lerp_int(a->eye[0], b->eye[0], num, den), lerp_int(a->eye[1], b->eye[1], num, den),
                        lerp_int(a->eye[2], b->eye[2], num, den)};
    *target = (sat_vec3_t){lerp_int(a->target[0], b->target[0], num, den),
                           lerp_int(a->target[1], b->target[1], num, den),
                           lerp_int(a->target[2], b->target[2], num, den)};
    if (low) {
        eye->x /= 4; eye->y /= 4; eye->z /= 4;
        target->x /= 4; target->y /= 4; target->z /= 4;
    }
}

/* The actor walks in from the west shore, wades and floats half under. */
static sat_vec3_t actor_at(sat_fx16_t x, sat_fx16_t z, uint32_t tick) {
    const sat_fx16_t ground = terrain_height(x >> 16, z >> 16) + RADIUS;
    const int32_t bob = sat_sin_deg_whole((int32_t)tick * 9) / 8;
    const sat_fx16_t floating = SURFACE - RADIUS * 2 / 5 + bob;
    return (sat_vec3_t){x, ground > floating ? ground : floating, z};
}

static sat_vec3_t scripted_actor(uint32_t tick) {
    return actor_at(-UNIT(38) + (sat_fx16_t)((int64_t)UNIT(42) * tick / ROUTE_TICKS), UNIT(4), tick);
}

/* ---- drawing --------------------------------------------------------- */

/* Horizontal bands stand in for the sky: VDP2's backdrop is one colour. */
static void draw_sky(void) {
    enum { BANDS = 16 };
    for (unsigned band = 0; band < BANDS; ++band) {
        sat_example_must(sat_draw_rect_screen(
            0, (int16_t)(band * HEIGHT / BANDS), WIDTH, HEIGHT / BANDS + 1,
            SAT_RGB555(8 + band, 17 + band * 3 / 4, 28 + band / 6)));
    }
}

static void update_actor_world(const sat_vec3_t* at) {
    for (unsigned v = 0; v < SPHERE_VERTICES; ++v) {
        actor_world[v] = (sat_vec3_t){sphere_vertices[v].x + at->x, sphere_vertices[v].y + at->y,
                                      sphere_vertices[v].z + at->z};
    }
}

static void submit_level(const sat_mesh_t* mesh, uint16_t* indices, uint16_t* face_materials,
                         uint16_t count, const uint16_t* light, uint16_t pass) {
    if (!count) return;
    sat_scene3d_material_t materials[4];
    for (unsigned m = 0; m < 4; ++m) {
        materials[m] = level_materials[m];
        materials[m].vertex_gouraud = light;
    }
    sat_mesh_t part = *mesh;
    part.indices = indices;
    part.face_count = count;
    part.face_cap = count;
    const sat_scene3d_instance_t instance = {
        .mesh = &part, .materials = materials, .material_count = 4,
        .face_materials = face_materials, .pass = pass
    };
    sat_example_must(sat_scene_submit_instance(&scene, &instance, SAT_SCENE3D_SLOT_INHERIT, projected, 0));
}

/* The lake as a level would model it: four large quads for flat water, or a
 * grid of tiles at the texture's density when it is textured, since VDP1 maps
 * one texture onto each quad. */
static void draw_naive_water(uint8_t textured) {
    const int grid = textured ? WATER_TILES : 2;
    const sat_fx16_t step = 2 * LAKE_HALF / grid;
    const sat_scene3d_material_t* material = textured ? &water_textured_material : &water_material;
    for (int j = 0; j < grid; ++j) {
        for (int i = 0; i < grid; ++i) {
            const sat_fx16_t x0 = -LAKE_HALF + i * step, x1 = x0 + step;
            const sat_fx16_t z0 = -LAKE_HALF + j * step, z1 = z0 + step;
            const sat_quad3_t quad = {{{x0, SURFACE, z0}, {x1, SURFACE, z0}, {x1, SURFACE, z1}, {x0, SURFACE, z1}}};
            // The baseline reports what the scene refuses instead of stopping.
            const sat_result_t result = sat_scene_submit_quad(&scene, &quad, material, 0);
            if (result != SAT_OK) {
                ++water_lake_telemetry.submit_errors;
                water_lake_telemetry.last_error = (uint32_t)result;
            }
        }
    }
}

static void draw_naive(const sat_vec3_t* actor, uint8_t textured, uint8_t water_off) {
    uint64_t t = sat_time_ticks();
    submit_level(&level_mesh, level_indices, level_face_materials, level_mesh.face_count, level_light, 0);
    water_lake_telemetry.stage[0] = (uint32_t)(sat_time_ticks() - t);
    t = sat_time_ticks();
    if (!water_off) draw_naive_water(textured);
    water_lake_telemetry.stage[1] = (uint32_t)(sat_time_ticks() - t);
    water_lake_telemetry.stage[2] = water_lake_telemetry.stage[3] = 0;
    t = sat_time_ticks();
    update_actor_world(actor);
    const sat_scene3d_instance_t instance = {
        .mesh = &actor_mesh, .materials = &actor_material, .material_count = 1,
        .face_materials = actor_face_materials
    };
    sat_example_must(sat_scene_submit_instance(&scene, &instance, SAT_SCENE3D_SLOT_INHERIT, actor_scratch, 0));
    water_lake_telemetry.stage[4] = (uint32_t)(sat_time_ticks() - t);
}

static sat_fx16_t magnitude(sat_fx16_t v) { return v < 0 ? -v : v; }

static void draw_full(const sat_vec3_t* eye, const sat_vec3_t* target, const sat_vec3_t* actor,
                      const sat_water_view_state_t* state, uint32_t tick, uint8_t textured,
                      uint8_t water_off) {
    const sat_water_view_passes_t passes = sat_water_view_passes(state->underwater);
    uint64_t t = sat_time_ticks();
    // Each side has its own block of vertices, so an instance projects only its own.
    sat_mesh_t mesh = level_mesh;
    mesh.vertices = prepared_vertices;
    mesh.vertex_cap = prepared.above_vertices;
    mesh.vertex_count = prepared.above_vertices;
    submit_level(&mesh, prepared_indices, prepared_face_materials, prepared.above_faces,
                 prepared_light, passes.above);
    mesh.vertices = prepared_vertices + prepared.below_first;
    mesh.vertex_cap = prepared.below_vertices;
    mesh.vertex_count = prepared.below_vertices;
    submit_level(&mesh, prepared_indices + 4u * prepared.above_faces,
                 prepared_face_materials + prepared.above_faces, prepared.below_faces,
                 prepared_light + prepared.below_first, passes.below);
    water_lake_telemetry.stage[0] = (uint32_t)(sat_time_ticks() - t);
    t = sat_time_ticks();

    sat_fx16_t forward_x = target->x - eye->x, forward_z = target->z - eye->z;
    const int32_t length = (int32_t)isqrt64((int64_t)forward_x * forward_x + (int64_t)forward_z * forward_z);
    if (length > 0) {
        forward_x = (sat_fx16_t)((int64_t)forward_x * SAT_FX16_ONE / length);
        forward_z = (sat_fx16_t)((int64_t)forward_z * SAT_FX16_ONE / length);
    }
    const sat_fx16_t height = magnitude(eye->y - SURFACE);
    const sat_water_plane_config_t config = {
        .min_x = volumes[0].min_x, .max_x = volumes[0].max_x,
        .min_z = volumes[0].min_z, .max_z = volumes[0].max_z,
        .surface = SURFACE, .cell = UNIT(3), .cells = 4, .rings = 4,
        .underside = state->underwater, .wobble = SAT_FX16_ONE / 6, .period = UNIT(7), .speed = 7,
        .tick = tick, .near_color = sat_gouraud_rgb(0, 2, 3), .far_color = sat_gouraud_rgb(6, 6, 4),
        .glint = 3, .forward_x = forward_x, .forward_z = forward_z, .behind = height + UNIT(3)
    };
    sat_example_must(sat_water_plane_build(&plane, &config, eye->x, eye->z));
    water_lake_telemetry.stage[1] = (uint32_t)(sat_time_ticks() - t);
    t = sat_time_ticks();
    const sat_scene3d_material_t* water = textured ? &water_textured_material : &water_material;
    const sat_scene3d_material_t rings[4] = {*water, *water, *water, *water};
    if (!water_off) sat_example_must(sat_water_plane_submit(&scene, &plane, rings, passes.water));
    water_lake_telemetry.stage[2] = (uint32_t)(sat_time_ticks() - t);
    t = sat_time_ticks();

    update_actor_world(actor);
    const sat_water_actor_params_t params = {
        .surface = SURFACE, .tint = {-6, -2, 3}, .full_depth = UNIT(3),
        .pass_below = passes.below, .pass_above = passes.above
    };
    sat_water_actor_split_t split;
    sat_example_must(sat_water_actor_split(&actor_mesh, 0, &actor_material, 1, sphere_light, &params,
                                           &actor_space, &split));
    water_lake_telemetry.stage[3] = (uint32_t)(sat_time_ticks() - t);
    t = sat_time_ticks();
    sat_example_must(sat_water_actor_submit(&scene, &actor_mesh, &actor_material, 1, sphere_light, 0,
                                            &params, &actor_space, &split));
    water_lake_telemetry.stage[4] = (uint32_t)(sat_time_ticks() - t);
    water_lake_telemetry.cut_pieces = split.pieces;
}

/* Switching to NAIVE while the underwater tint is up releases the layers. */
static void set_mode(uint8_t* mode, uint8_t next) {
    if (*mode == next) return;
    *mode = next;
    if (view.fade) {
        const sat_water_view_state_t off = {.release = 1};
        sat_example_must(sat_water_view_apply(&view, &off));
        sat_example_must(sat_vdp2_layers_commit());
        view.fade = 0;
        view.region = SAT_WATER_REGION_NONE;
    }
}

static sat_fx16_t clamp(sat_fx16_t value, sat_fx16_t lo, sat_fx16_t hi) {
    return value < lo ? lo : (value > hi ? hi : value);
}

int main(void) {
    const sat_video_config_t video = {WIDTH, HEIGHT, SAT_VIDEO_AUTO, 0};
    sat_camera3d_t camera;
    uint32_t tick = 0, script = 0;
    uint8_t paused = 0, mode = MODE_FULL, textured = 0, water_off = 0, low = 0;
    uint32_t stop_section = 0;
    int32_t orbit = 30;
    sat_vec3_t free_actor = actor_at(-UNIT(34), UNIT(4), 0);

    sat_example_must(sat_init(&video));
    sat_example_must(sat_vdp1_set_frame_mode(SAT_VDP1_FRAME_ON_SUBMIT));
    sat_example_must(sat_vdp2_back_color_set(SAT_RGB555(14, 22, 28)));
    sat_example_must(sat_set_clear_color(SAT_RGB555(14, 22, 28)));
    sat_example_must(sat_scene_init(&scene, faces, keys, order, FACE_CAPACITY));
    volumes[0] = (sat_water_volume_t){
        .min_x = -LAKE_HALF, .max_x = LAKE_HALF, .min_z = -LAKE_HALF, .max_z = LAKE_HALF,
        .bottom = BED, .surface = SURFACE, .buoyancy = SAT_FX16_ONE / 26, .drag = SAT_FX16_ONE / 5
    };
    sat_example_must(sat_water_regions_init(&regions, volumes, 1));
    const sat_water_view_config_t view_config = {
        .regions = &regions, .hysteresis = SAT_FX16_ONE / 8, .bank = SAT_VDP2_COLOR_OFFSET_A,
        .layers = SAT_VDP2_LAYER_SPRITE | SAT_VDP2_LAYER_BACK, .tint = {-110, -55, -10}, .fade_frames = 6
    };
    sat_example_must(sat_water_view_init(&view, &view_config));
    build_level();
    prepare_level();
    make_water_texture();
    make_actor();
    water_lake_telemetry.tick_frequency = sat_time_frequency_hz();

    for (;;) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_begin_frame());
        sat_example_must(sat_pad_poll(&pad));
        if (pad.pressed & SAT_PAD_START) {
            tick = script = 0;
            paused = 0;
            stop_section = 0;
            orbit = 30;
            free_actor = actor_at(-UNIT(34), UNIT(4), 0);
        } else {
            // C counts up the route section a replay pauses at (0: run to the end).
            if ((pad.pressed & SAT_PAD_C) && !script) stop_section = (stop_section + 1u) % SECTIONS;
            if ((pad.pressed & (SAT_PAD_X | SAT_PAD_Y)) && (pad.held & SAT_PAD_Z)) {
                /* Z + Y replays the route from tick zero in FULL mode and Z + X in
                 * NAIVE; the pad is then ignored, so the replay is the same in NTSC
                 * and PAL. It pauses at the start of section `stop_section`, set with
                 * C beforehand, so a capture is the same tick however fast the frames run. */
                set_mode(&mode, (pad.pressed & SAT_PAD_X) ? MODE_NAIVE : MODE_FULL);
                textured = water_off = low = 0;
                tick = 0;
                script = 1;
                paused = 0;
                pad.held = 0;
            } else if ((pad.pressed & (SAT_PAD_A | SAT_PAD_B)) && (pad.held & SAT_PAD_Z)) {
                /* Z + A replays in FULL mode and Z + B in NAIVE, with textured water. */
                set_mode(&mode, (pad.pressed & SAT_PAD_B) ? MODE_NAIVE : MODE_FULL);
                textured = 1;
                water_off = low = 0;
                tick = 0;
                script = 1;
                paused = 0;
                pad.held = 0;
            } else if ((pad.pressed & (SAT_PAD_L | SAT_PAD_R)) && (pad.held & SAT_PAD_Z)) {
                /* Z + R replays in FULL mode and Z + L in NAIVE with the water surface
                 * left out, so the rest of the frame can be subtracted from a run. */
                set_mode(&mode, (pad.pressed & SAT_PAD_L) ? MODE_NAIVE : MODE_FULL);
                textured = 0;
                water_off = 1;
                low = 0;
                tick = 0;
                script = 1;
                paused = 0;
                pad.held = 0;
            } else if ((pad.pressed & (SAT_PAD_UP | SAT_PAD_DOWN)) && (pad.held & SAT_PAD_Z)) {
                /* Z + Up replays the low skim route in FULL mode and Z + Down in NAIVE. */
                set_mode(&mode, (pad.pressed & SAT_PAD_DOWN) ? MODE_NAIVE : MODE_FULL);
                textured = water_off = 0;
                low = 1;
                tick = 0;
                script = 1;
                paused = 0;
                pad.held = 0;
            } else if (pad.pressed & SAT_PAD_Y) {
                paused ^= 1u;
            } else if (pad.pressed & SAT_PAD_X) {
                set_mode(&mode, mode == MODE_FULL ? MODE_NAIVE : MODE_FULL);
            }
            if (pad.held & SAT_PAD_L) orbit = (orbit + 358) % 360;
            if (pad.held & SAT_PAD_R) orbit = (orbit + 2) % 360;
            if (script) pad.held = pad.pressed = 0;
            if (!paused && !script) {
                const sat_fx16_t speed = SAT_FX16_ONE / 4;
                const int x = !!(pad.held & SAT_PAD_RIGHT) - !!(pad.held & SAT_PAD_LEFT);
                const int z = !!(pad.held & SAT_PAD_DOWN) - !!(pad.held & SAT_PAD_UP);
                free_actor = actor_at(clamp(free_actor.x + x * speed, -UNIT(38), UNIT(38)),
                                      clamp(free_actor.z + z * speed, -UNIT(38), UNIT(38)), tick);
            }
        }

        sat_vec3_t actor = free_actor, eye, target;
        if (script) {
            route_pose(script - 1u, low, &eye, &target);
            actor = scripted_actor(script - 1u);
        } else {
            const sat_fx16_t yaw = UNIT(orbit);
            target = (sat_vec3_t){actor.x, SURFACE, actor.z};
            eye = (sat_vec3_t){target.x + sat_fx16_mul(UNIT(16), sat_sin_deg(yaw)), UNIT(6),
                               target.z + sat_fx16_mul(UNIT(16), sat_cos_deg(yaw))};
        }

        // NAIVE has no underwater view: the state stays clear and nothing is applied.
        sat_water_view_state_t state = {.region = SAT_WATER_REGION_NONE};
        if (mode == MODE_FULL) {
            sat_example_must(sat_water_view_update(&view, &eye, &state));
            sat_example_must(sat_water_view_apply(&view, &state));
            sat_example_must(sat_vdp2_layers_commit());
        }
        const sat_vec3_t up = {0, SAT_FX16_ONE, 0};
        sat_example_must(sat_camera3d_init(&camera, &eye, &target, &up, UNIT(51),
            sat_fx16_div(UNIT(WIDTH), UNIT(HEIGHT)), SAT_FX16_ONE / 2, UNIT(120)));

        draw_sky();
        const uint64_t prepare = sat_time_ticks();
        sat_example_must(sat_scene_begin(&scene, &camera, SAT_FX16_ONE / 2, WIDTH, HEIGHT, OVERLAY_COMMANDS));
        if (mode == MODE_FULL) draw_full(&eye, &target, &actor, &state, tick, textured, water_off);
        else draw_naive(&actor, textured, water_off);
        const uint64_t flush = sat_time_ticks();
        sat_example_must(sat_scene_flush(&scene));
        sat_scene_stats_t stats;
        sat_example_must(sat_scene_stats(&scene, &stats));
        sat_example_must(sat_app_frame_end());

        sat_water_sample_t sample;
        uint16_t region;
        const sat_vec3_t base = {actor.x, actor.y - RADIUS, actor.z};
        sat_example_must(sat_water_regions_sample(&regions, &base, 2 * RADIUS, &sample, &region));
        water_lake_telemetry.tick = tick;
        water_lake_telemetry.mode = mode;
        water_lake_telemetry.textured = textured;
        water_lake_telemetry.water_off = water_off;
        water_lake_telemetry.low = low;
        water_lake_telemetry.script = script;
        water_lake_telemetry.paused = paused;
        water_lake_telemetry.underwater = state.underwater;
        water_lake_telemetry.fade = state.fade;
        water_lake_telemetry.region = state.region;
        water_lake_telemetry.actor_fraction = (uint32_t)sample.fraction;
        water_lake_telemetry.above_faces = prepared.above_faces;
        water_lake_telemetry.below_faces = prepared.below_faces;
        uint16_t plane_faces = 0;
        if (mode == MODE_FULL) sat_example_must(sat_water_plane_face_count(&plane, &plane_faces));
        water_lake_telemetry.plane_faces = plane_faces;
        water_lake_telemetry.actor_x = actor.x;
        water_lake_telemetry.actor_y = actor.y;
        water_lake_telemetry.actor_z = actor.z;
        if (script) {
            if (script == 1) water_lake_telemetry.underwater_ticks = water_lake_telemetry.max_fade = 0;
            if (!paused && state.underwater) ++water_lake_telemetry.underwater_ticks;
            if (!paused && state.fade > water_lake_telemetry.max_fade) water_lake_telemetry.max_fade = state.fade;
            const unsigned s = (script - 1u) / SECTION_TICKS;
            section_record_t* r = (section_record_t*)&water_lake_telemetry.section[s < SECTIONS ? s : SECTIONS - 1];
            r->submitted = stats.submitted_faces;
            r->culled = stats.culled_faces;
            r->clipped = stats.clipped_faces;
            r->fallback = stats.fallback_faces;
            r->skipped = stats.skipped_faces;
            r->commands = stats.world_commands;
            r->preparation_ticks = (uint32_t)(flush - prepare);
            r->flush_ticks = (uint32_t)(sat_time_ticks() - flush);
        }
        if (!paused) {
            ++tick;
            if (script) {
                ++script;
                if (stop_section && script - 1u == stop_section * SECTION_TICKS) paused = 1;
                if (script > ROUTE_TICKS) {
                    script = 0;
                    paused = 1;
                }
            }
        }
    }
}
