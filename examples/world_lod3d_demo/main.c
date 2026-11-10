/* Procedural outdoor acceptance scene for the reusable world LOD planner.
 * All scenery is generated from small original meshes; the scene renderer
 * remains the only painter and VDP1 command emitter. */
#include <stdint.h>

#include "example_util.h"
#include "world_lod3d_demo.h"
#include "world_lod3d_demo_hlod.h"
#include "saturn/collide3d.h"
#include "saturn/fade3d.h"
#include "saturn/font.h"
#include "saturn/hud.h"
#include "saturn/input.h"
#include "saturn/math3d.h"
#include "saturn/mesh3d.h"
#include "saturn/scene3d.h"
#include "saturn/scene3d_faces.h"
#include "saturn/time.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/vdp2_color_calc.h"
#include "saturn/vdp2_environment.h"
#include "saturn/vdp2_rbg0_ground.h"
#include "saturn/world_lod3d.h"
#include "saturn/world_lod_fade3d.h"
#include "saturn/world_lod3d_governor.h"

enum {
    SCREEN_W = 320,
    SCREEN_H = 224,
    HORIZON = 112,
    FOCAL_PX = 215,
    GROUND_BITMAP_WORD = 0x00000,
    GROUND_ROTATION_WORD = 0x10000,
    GROUND_COEFFICIENT_WORD = 0x12000,
    SKY_PALETTE = 1,
    FONT_PALETTE = 3,
    OVERLAY_COMMANDS = 128,
    SCENE_FACE_CAPACITY = 512,
    MESH_KINDS = 2,
    MATERIAL_KINDS = 3,
    MESH_LODS = 3,
    HLOD_CELL_COUNT = SAT_WORLD_HLOD_DEMO_CELL_COUNT,
    HLOD_ASSET_BASE = MESH_KINDS * MESH_LODS,
    TREE_COUNT = 16,
    SCENERY_COUNT = TREE_COUNT + HLOD_CELL_COUNT,
    OBJECT_COUNT = SCENERY_COUNT + 1,
    MAX_LEVELS = OBJECT_COUNT * MESH_LODS,
    MESH_VERTEX_CAPACITY = (SAT_WORLD_HLOD_DEMO_MAX_VERTICES > 64u ?
                            SAT_WORLD_HLOD_DEMO_MAX_VERTICES : 64u),
    MESH_FACE_CAPACITY = (SAT_WORLD_HLOD_DEMO_MAX_FACES > 30u ?
                          SAT_WORLD_HLOD_DEMO_MAX_FACES : 30u),
    CAMERA_TARGET_Y = 6,
    CAMERA_DISTANCE_DEFAULT = 12,
    CAMERA_DISTANCE_MIN = 8,
    CAMERA_DISTANCE_MAX = 16,
    GOVERNOR_BASE_FACES = 256,
    GOVERNOR_BASE_COMMANDS = 384
};

enum { KIND_TREE = 0, KIND_AVATAR = 1, KIND_STONE = 2, KIND_HLOD = 3 };

_Static_assert(SAT_WORLD_HLOD_DEMO_PACKAGE_BYTES > 0u,
               "the synthetic world HLOD package must be generated");
_Static_assert(SAT_WORLD_HLOD_DEMO_CELL_COUNT > 0u,
               "the synthetic world HLOD package must contain cells");
_Static_assert(SAT_WORLD_HLOD_DEMO_MAX_LOCAL_COORD_FX16 <= 0x7FFFFFFFLL,
               "HLOD cell-relative coordinates must fit signed 16.16");

typedef struct demo_object {
    sat_fx16_t x, y, z;
    uint8_t kind;
    uint8_t cell_index;
    uint8_t reserved[2];
} demo_object_t;

typedef struct demo_mesh {
    sat_mesh_t mesh;
    sat_vec3_t vertices[MESH_VERTEX_CAPACITY];
    uint16_t indices[MESH_FACE_CAPACITY * 4];
    sat_aabb3_t bounds;
} demo_mesh_t;

typedef struct demo_hlod_level {
    uint16_t vertex_count;
    uint16_t face_count;
    uint32_t data_offset;
    uint32_t data_bytes;
} demo_hlod_level_t;

static const sat_vdp2_rbg0_ground_config_t g_ground_base = {
    512u, 256u, SCREEN_W / 2u, HORIZON, 3u, 1u, FOCAL_PX,
    GROUND_COEFFICIENT_WORD
};

static demo_object_t g_definitions[OBJECT_COUNT];
static sat_world_lod3d_object_t g_objects[OBJECT_COUNT];
static sat_world_lod3d_level_t g_levels[MAX_LEVELS];
static sat_world_lod3d_state_t g_states[OBJECT_COUNT];
static sat_world_lod3d_draw_t g_draws[OBJECT_COUNT];
static sat_world_lod3d_request_t g_requests[OBJECT_COUNT];
static sat_world_lod3d_work_t g_work[OBJECT_COUNT];
static sat_world_lod3d_stats_t g_planner_stats;
static sat_world_lod3d_view_t g_view;
static sat_world_lod3d_budget_t g_base_budget;
static sat_world_lod3d_budget_t g_frame_budget;
static sat_world_lod3d_governor_config_t g_governor_config;
static sat_world_lod3d_governor_state_t g_governor_state;
static sat_world_lod3d_governor_stats_t g_governor_stats;
static sat_world_lod3d_fade_state_t g_fade_states[OBJECT_COUNT];
static sat_world_lod3d_fade_composition_t g_fade_composition = {
    1u, 1u, 7u, 5u, SAT_WORLD_LOD3D_FADE_FALLBACK_OPAQUE, {0u, 0u, 0u}
};
static sat_fade3d_slots_t g_fade = {
    {SAT_FX16(24), SAT_FX16(88), 8u, 0u, 0u},
    SAT_FX16(2), 0u, 1u, 1u, 0u
};
static demo_mesh_t g_meshes[MESH_KINDS][MESH_LODS];
static demo_mesh_t g_hlod_meshes[HLOD_CELL_COUNT][MESH_LODS];
static sat_scene3d_material_t g_kind_materials[MATERIAL_KINDS];
static sat_scene3d_material_t g_hlod_materials[5];
static sat_scene3d_material_t g_hlod_instance_materials[HLOD_CELL_COUNT][MESH_LODS][5];
static sat_vdp1_texture_t g_kind_textures[MATERIAL_KINDS];
static sat_scene3d_material_t g_hlod_window_material;
static sat_vdp1_texture_t g_hlod_window_texture;
static sat_scene3d_material_t g_hlod_door_material;
static sat_vdp1_texture_t g_hlod_door_texture;
static sat_scene3d_face_t g_scene_faces[SCENE_FACE_CAPACITY];
static uint32_t g_scene_keys[SCENE_FACE_CAPACITY];
static uint16_t g_scene_order[SCENE_FACE_CAPACITY];
static uint16_t g_face_materials[MESH_FACE_CAPACITY];
static uint16_t g_hlod_face_materials[HLOD_CELL_COUNT][MESH_LODS][MESH_FACE_CAPACITY];
static sat_fx16_t g_hlod_origins[HLOD_CELL_COUNT][3];
static uint16_t g_hlod_faces[HLOD_CELL_COUNT][MESH_LODS];
static sat_projected_vertex_t g_projected[MESH_VERTEX_CAPACITY];
static sat_vec3_t g_world_vertices[MESH_VERTEX_CAPACITY];
static sat_scene3d_instance_t g_instance;
static sat_scene_t g_scene;
static sat_camera3d_t g_camera;
static sat_ascii_font_t g_font;
static sat_hud_t g_hud;
static world_lod3d_demo_telemetry_t g_demo;
static uint16_t g_sky_map[SAT_VDP2_NBG0_MAP_CELLS];
static uint16_t g_ground_palette[256];
static uint16_t g_sky_palette[256];
static uint16_t g_ground_row[256];
static uint16_t g_coefficients[SCREEN_H * 2];
static uint16_t g_rotation[48];
static sat_vdp2_ground_environment_t g_ground_environment;
static uint8_t g_sky_pixels[512 * 128];
static uint32_t g_view_generation = 1u;
static sat_fx16_t g_camera_focus_x;
static sat_fx16_t g_camera_focus_z;
static sat_fx16_t g_yaw;
static sat_fx16_t g_pitch;
static sat_fx16_t g_camera_distance = SAT_FX16(CAMERA_DISTANCE_DEFAULT);
static uint16_t g_frame_start;
static uint8_t g_fade_enabled = 1u;
static uint8_t g_hlod_lod0_missing;
static sat_fx16_t g_last_ground_pitch;
static sat_fx16_t g_last_ground_eye_y;
static uint8_t g_ground_coefficients_valid;

volatile world_lod3d_demo_telemetry_t g_world_lod3d_demo;

static sat_fx16_t clamp_fx(sat_fx16_t value, sat_fx16_t low, sat_fx16_t high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static uint16_t package_u16(const uint8_t* data) {
    return (uint16_t)(((uint16_t)data[0] << 8u) | data[1]);
}

static uint32_t package_u32(const uint8_t* data) {
    return ((uint32_t)data[0] << 24u) | ((uint32_t)data[1] << 16u) |
           ((uint32_t)data[2] << 8u) | data[3];
}

static int32_t package_i32(const uint8_t* data) {
    const uint32_t value = package_u32(data);
    const int64_t signed_value = (value & 0x80000000u) != 0u ?
        (int64_t)value - 4294967296LL : (int64_t)value;
    return (int32_t)signed_value;
}

static uint8_t ground_pixel(void* user, uint16_t x, uint16_t y) {
    const uint32_t stripe = ((uint32_t)x >> 3u) ^ ((uint32_t)y >> 3u);
    (void)user;
    return (uint8_t)(32u + (stripe & 31u));
}

static void build_background(void) {
    const sat_vdp2_nbg0_config_t sky = {
        SAT_VDP2_CHAR_SIZE_1X1, SAT_VDP2_COLOR_MODE_256, 0x3Bu, 0u, 0u
    };
    const sat_vdp2_rbg0_mode7_config_t ground = {
        SAT_VDP2_RBG0_BITMAP_512x256, SAT_VDP2_COLOR_MODE_256,
        GROUND_BITMAP_WORD, GROUND_ROTATION_WORD, SAT_COLOR_BLACK, 5u, 7u
    };

    for (uint32_t i = 0u; i < 256u; ++i) {
        const uint32_t shade = i & 31u;
        g_ground_palette[i] = SAT_BGR555(8u + shade / 6u,
                                         12u + shade / 5u,
                                         5u + shade / 8u);
        g_sky_palette[i] = SAT_BGR555(shade / 2u, shade * 2u / 3u, 20u + shade / 3u);
    }
    g_ground_palette[0] = SAT_COLOR_BLACK;
    for (uint32_t y = 0u; y < 128u; ++y) {
        const uint8_t band = (uint8_t)(1u + y * 30u / 127u);
        for (uint32_t x = 0u; x < 512u; ++x) {
            g_sky_pixels[y * 512u + x] = band;
        }
    }
    /* Procedural cloud bands are deterministic and wrap with the sky map. */
    for (uint32_t puff = 0u; puff < 8u; ++puff) {
        const int32_t cx = (int32_t)((puff * 71u + 29u) & 511u);
        const int32_t cy = 28 + (int32_t)((puff * 23u) % 60u);
        for (int32_t y = -3; y <= 0; ++y) {
            for (int32_t x = -18; x <= 18; ++x) {
                const int32_t px = (cx + x) & 511;
                const int32_t py = cy + y;
                if ((x * x) / 4 + y * y * 12 < 82 && py >= 0 && py < 128)
                    g_sky_pixels[(uint32_t)py * 512u + (uint32_t)px] = 40u;
            }
        }
    }

    sat_example_must(sat_vdp2_palette_upload(g_ground_palette, 256u, 0u));
    sat_example_must(sat_vdp2_palette_upload(g_sky_palette, 256u, 256u));
    sat_example_must(sat_vdp2_nbg0_init(&sky));
    sat_example_must(sat_vdp2_nbg0_upload_indexed8(
        g_sky_pixels, 512u, 128u, SKY_PALETTE, g_sky_map));
    sat_example_must(sat_vdp2_bitmap_upload_indexed8(
        GROUND_BITMAP_WORD, 512u, 256u, ground_pixel, 0, 0,
        g_ground_row, 256u));
    sat_example_must(sat_vdp2_ground_environment_init(
        &g_ground_environment, &g_ground_base, &ground, g_coefficients,
        SCREEN_H * 2u, g_rotation, SCREEN_H));
    sat_example_must(sat_vdp2_ground_environment_upload_coefficients(
        &g_ground_environment));
    sat_example_must(sat_vdp2_nbg0_set_priority(2u));
    sat_example_must(sat_vdp2_rbg0_set_priority(5u));
    sat_example_must(sat_vdp2_rbg0_set_transparent_code_enabled(1u));
    sat_example_must(sat_vdp2_sprite_color_calc_configure_alpha(7u));
}

static void build_mesh(demo_mesh_t* out) {
    sat_example_must(sat_mesh_init(&out->mesh, out->vertices, MESH_VERTEX_CAPACITY,
                                   out->indices, MESH_FACE_CAPACITY));
}

static void add_box(demo_mesh_t* out, sat_fx16_t x, sat_fx16_t y, sat_fx16_t z,
                    sat_fx16_t hx, sat_fx16_t hy, sat_fx16_t hz) {
    const sat_fx16_t v[8][3] = {
        {x-hx,y-hy,z-hz},{x+hx,y-hy,z-hz},{x+hx,y+hy,z-hz},{x-hx,y+hy,z-hz},
        {x-hx,y-hy,z+hz},{x+hx,y-hy,z+hz},{x+hx,y+hy,z+hz},{x-hx,y+hy,z+hz}
    };
    static const uint8_t faces[6][4] = {
        {4,5,6,7},{1,0,3,2},{5,1,2,6},
        {0,4,7,3},{7,6,2,3},{0,1,5,4}
    };
    uint16_t index[8];
    for (uint8_t i = 0u; i < 8u; ++i)
        sat_example_must(sat_mesh_add_vertex(&out->mesh, v[i][0], v[i][1], v[i][2], &index[i]));
    for (uint8_t i = 0u; i < 6u; ++i)
        sat_example_must(sat_mesh_add_face(&out->mesh, index[faces[i][0]], index[faces[i][1]],
                                           index[faces[i][2]], index[faces[i][3]]));
}

static void build_meshes(void) {
    demo_mesh_t* m;
    for (uint8_t lod = 0u; lod < MESH_LODS; ++lod) build_mesh(&g_meshes[KIND_TREE][lod]);
    m = &g_meshes[KIND_TREE][0];
    add_box(m,0,SAT_FX16(2),0,SAT_FX16(1)/2,SAT_FX16(2),SAT_FX16(1)/2);
    add_box(m,0,SAT_FX16(5),0,SAT_FX16(3),SAT_FX16(2),SAT_FX16(3));
    add_box(m,0,SAT_FX16(8),0,SAT_FX16(2),SAT_FX16(2),SAT_FX16(2));
    add_box(m,0,SAT_FX16(11),0,SAT_FX16(1),SAT_FX16(2),SAT_FX16(1));
    m->bounds = (sat_aabb3_t){{0,SAT_FX16(7),0},{SAT_FX16(4),SAT_FX16(7),SAT_FX16(4)}};
    m = &g_meshes[KIND_TREE][1];
    add_box(m,0,SAT_FX16(2),0,SAT_FX16(1)/2,SAT_FX16(2),SAT_FX16(1)/2);
    add_box(m,0,SAT_FX16(7),0,SAT_FX16(3),SAT_FX16(4),SAT_FX16(3));
    m->bounds = (sat_aabb3_t){{0,SAT_FX16(6),0},{SAT_FX16(3),SAT_FX16(6),SAT_FX16(3)}};
    m = &g_meshes[KIND_TREE][2];
    add_box(m,0,SAT_FX16(7),0,SAT_FX16(3),SAT_FX16(7),SAT_FX16(3));
    m->bounds = (sat_aabb3_t){{0,SAT_FX16(7),0},{SAT_FX16(3),SAT_FX16(7),SAT_FX16(3)}};

    build_mesh(&g_meshes[KIND_AVATAR][0]);
    add_box(&g_meshes[KIND_AVATAR][0],0,SAT_FX16(1),0,SAT_FX16(3)/4,SAT_FX16(1),SAT_FX16(1)/2);
    add_box(&g_meshes[KIND_AVATAR][0],0,SAT_FX16(2) + SAT_FX16(1)/2,0,
            SAT_FX16(1)/2,SAT_FX16(1)/2,SAT_FX16(1)/2);
    g_meshes[KIND_AVATAR][0].bounds =
        (sat_aabb3_t){{0,SAT_FX16(2),0},{SAT_FX16(1),SAT_FX16(3),SAT_FX16(1)}};
}

static void build_textures(void) {
    static const uint8_t pixels[64] = {
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1
    };
    static const uint16_t colors[MATERIAL_KINDS] = {
        SAT_RGB555(10u,17u,7u), SAT_RGB555(30u,22u,12u), SAT_RGB555(22u,19u,14u)
    };
    uint16_t palette[256] = {0u};
    for (uint8_t material = 0u; material < MATERIAL_KINDS; ++material) {
        palette[1] = colors[material];
        /* CRAM banks 0/1 carry the ground/sky and bank 3 is reserved for the
         * HUD font. Use the high sprite banks for three authored materials. */
        sat_example_must(sat_tex_upload_indexed8(
            &g_kind_textures[material], pixels, 8u, 8u, palette,
            (uint16_t)(4u + material)));
        g_kind_materials[material] = (sat_scene3d_material_t){0};
        g_kind_materials[material].kind = SAT_SCENE3D_INDEXED_SOLID;
        g_kind_materials[material].texture = &g_kind_textures[material];
        g_kind_materials[material].color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
    }
    palette[1] = SAT_RGB555(8u,16u,27u);
    sat_example_must(sat_tex_upload_indexed8(
        &g_hlod_window_texture, pixels, 8u, 8u, palette, 7u));
    g_hlod_window_material = (sat_scene3d_material_t){0};
    g_hlod_window_material.kind = SAT_SCENE3D_INDEXED_SOLID;
    g_hlod_window_material.texture = &g_hlod_window_texture;
    g_hlod_window_material.color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
    palette[1] = SAT_RGB555(10u,7u,4u);
    sat_example_must(sat_tex_upload_indexed8(
        &g_hlod_door_texture, pixels, 8u, 8u, palette, 2u));
    g_hlod_door_material = (sat_scene3d_material_t){0};
    g_hlod_door_material.kind = SAT_SCENE3D_INDEXED_SOLID;
    g_hlod_door_material.texture = &g_hlod_door_texture;
    g_hlod_door_material.color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
}

static void build_hlod_meshes(void) {
    static demo_hlod_level_t descriptors[HLOD_CELL_COUNT][MESH_LODS];
    const uint8_t* package = g_world_hlod_demo_package;
    const uint32_t package_size = g_world_hlod_demo_package_size;
    const uint32_t directory_end = 44u + HLOD_CELL_COUNT * (38u + MESH_LODS * 20u);
    uint32_t cursor = 44u;

    if (package_size != SAT_WORLD_HLOD_DEMO_PACKAGE_BYTES || package_size < 44u ||
        directory_end > package_size ||
        package[0] != 'W' || package[1] != 'L' || package[2] != 'H' ||
        package[3] != '1' || package_u16(package + 4u) != 1u ||
        package_u16(package + 6u) != HLOD_CELL_COUNT ||
        package_u32(package + 8u) != package_size) {
        sat_example_must(SAT_ERR_INVALID_ARG);
    }

    g_hlod_materials[0] = g_kind_materials[KIND_STONE];
    g_hlod_materials[1] = g_kind_materials[KIND_AVATAR];
    g_hlod_materials[2] = g_kind_materials[KIND_STONE];
    g_hlod_materials[3] = g_hlod_door_material;
    g_hlod_materials[4] = g_hlod_window_material;

    for (uint16_t cell = 0u; cell < HLOD_CELL_COUNT; ++cell) {
        int32_t key[3];
        int32_t bounds[6];
        uint16_t lod_count;
        if (cursor + 38u > package_size) sat_example_must(SAT_ERR_INVALID_ARG);
        key[0] = package_i32(package + cursor);
        key[1] = package_i32(package + cursor + 4u);
        key[2] = package_i32(package + cursor + 8u);
        for (uint8_t axis = 0u; axis < 6u; ++axis)
            bounds[axis] = package_i32(package + cursor + 12u + axis * 4u);
        lod_count = package_u16(package + cursor + 36u);
        cursor += 38u;
        if (lod_count != MESH_LODS || cursor + (uint32_t)lod_count * 20u > package_size)
            sat_example_must(SAT_ERR_INVALID_ARG);

        for (uint8_t axis = 0u; axis < 3u; ++axis) {
            const int64_t world_units = (int64_t)key[axis] * 16;
            const int64_t origin_fx = world_units * 65536LL;
            if (origin_fx < -2147483648LL || origin_fx > 2147483647LL)
                sat_example_must(SAT_ERR_INVALID_ARG);
            g_hlod_origins[cell][axis] = (sat_fx16_t)origin_fx;
        }
        for (uint8_t lod = 0u; lod < MESH_LODS; ++lod) {
            demo_hlod_level_t* desc = &descriptors[cell][lod];
            const uint8_t* record = package + cursor;
            uint32_t expected_bytes;
            desc->vertex_count = package_u16(record);
            desc->face_count = package_u16(record + 2u);
            desc->data_offset = package_u32(record + 12u);
            desc->data_bytes = package_u32(record + 16u);
            expected_bytes = (uint32_t)desc->vertex_count * 20u +
                             (uint32_t)desc->face_count * 8u;
            if (desc->vertex_count == 0u || desc->face_count == 0u ||
                desc->vertex_count > MESH_VERTEX_CAPACITY ||
                desc->face_count > MESH_FACE_CAPACITY ||
                desc->data_bytes != expected_bytes ||
                desc->data_offset < directory_end ||
                desc->data_offset > package_size ||
                desc->data_bytes > package_size - desc->data_offset)
                sat_example_must(SAT_ERR_INVALID_ARG);
            cursor += 20u;

            demo_mesh_t* mesh = &g_hlod_meshes[cell][lod];
            sat_example_must(sat_mesh_init(&mesh->mesh, mesh->vertices,
                                           MESH_VERTEX_CAPACITY, mesh->indices,
                                           MESH_FACE_CAPACITY));
            mesh->bounds = (sat_aabb3_t){
                {(sat_fx16_t)(((int64_t)bounds[0] + bounds[3]) / 2),
                 (sat_fx16_t)(((int64_t)bounds[1] + bounds[4]) / 2),
                 (sat_fx16_t)(((int64_t)bounds[2] + bounds[5]) / 2)},
                {(sat_fx16_t)(((int64_t)bounds[3] - bounds[0]) / 2),
                 (sat_fx16_t)(((int64_t)bounds[4] - bounds[1]) / 2),
                 (sat_fx16_t)(((int64_t)bounds[5] - bounds[2]) / 2)}
            };
            g_hlod_faces[cell][lod] = desc->face_count;
            for (uint16_t vertex = 0u; vertex < desc->vertex_count; ++vertex) {
                const uint8_t* v = package + desc->data_offset + (uint32_t)vertex * 20u;
                uint16_t inserted;
                const sat_fx16_t x = package_i32(v);
                const sat_fx16_t y = package_i32(v + 4u);
                const sat_fx16_t z = package_i32(v + 8u);
                sat_example_must(sat_mesh_add_vertex(&mesh->mesh, x, y, z, &inserted));
                if (inserted != vertex) sat_example_must(SAT_ERR_INVALID_ARG);
            }
            for (uint16_t face = 0u; face < desc->face_count; ++face) {
                const uint8_t* f = package + desc->data_offset +
                    (uint32_t)desc->vertex_count * 20u + (uint32_t)face * 8u;
                const uint16_t a = package_u16(f);
                const uint16_t b = package_u16(f + 2u);
                const uint16_t c = package_u16(f + 4u);
                const uint16_t material = package_u16(f + 6u);
                if (a >= desc->vertex_count || b >= desc->vertex_count ||
                    c >= desc->vertex_count || material >= 5u)
                    sat_example_must(SAT_ERR_INVALID_ARG);
                sat_example_must(sat_mesh_add_face(&mesh->mesh, a, c, b, b));
                g_hlod_face_materials[cell][lod][face] = material;
            }
            for (uint8_t material = 0u; material < 5u; ++material) {
                g_hlod_instance_materials[cell][lod][material] = g_hlod_materials[material];
                g_hlod_instance_materials[cell][lod][material].color_calc_slot =
                    SAT_INDEXED_SOLID_OPAQUE;
            }
        }
    }
}

static void build_objects(void) {
    uint16_t id = 1u;
    g_definitions[0] = (demo_object_t){0, 0, 0, KIND_AVATAR, 0u, {0u,0u}};
    for (uint8_t cell = 0u; cell < HLOD_CELL_COUNT; ++cell) {
        g_definitions[id++] = (demo_object_t){
            g_hlod_origins[cell][0], g_hlod_origins[cell][1], g_hlod_origins[cell][2],
            KIND_HLOD, cell, {0u,0u}};
    }
    for (uint8_t row = 0u; row < 4u; ++row) {
        for (uint8_t col = 0u; col < 4u; ++col) {
            const int32_t x = -36 + (int32_t)col * 24 + ((row & 1u) ? 6 : 0);
            const int32_t z = 12 + (int32_t)row * 26;
            g_definitions[id++] = (demo_object_t){
                SAT_FX16(x), 0, SAT_FX16(z), KIND_TREE, 0u, {0u,0u}};
        }
    }
    if (id != OBJECT_COUNT) sat_example_must(SAT_ERR_INVALID_ARG);

    uint16_t level_index = 0u;
    for (uint16_t i = 0u; i < OBJECT_COUNT; ++i) {
        sat_world_lod3d_object_t* object = &g_objects[i];
        const demo_object_t* def = &g_definitions[i];
        object->stable_id = i;
        object->generation = 1u;
        object->levels_first = level_index;
        object->level_count = i == 0u ? 1u : 3u;
        object->flags = i == 0u ? SAT_WORLD_LOD3D_OBJECT_PINNED : SAT_WORLD_LOD3D_OBJECT_OPTIONAL;
        object->priority = i == 0u ? 255u : 128u;
        for (uint8_t lod = 0u; lod < object->level_count; ++lod) {
            const uint8_t mesh_lod = i == 0u ? 0u : lod;
            sat_world_lod3d_level_t* level = &g_levels[level_index++];
            if (def->kind == KIND_HLOD) {
                level->asset_id = HLOD_ASSET_BASE +
                    (uint32_t)def->cell_index * MESH_LODS + mesh_lod;
            } else {
                level->asset_id = (uint32_t)def->kind * MESH_LODS + mesh_lod;
            }
            level->min_projected_radius_px_16_16 = i == 0u ? 0u :
                (lod == 0u ? 42u << 16u : (lod == 1u ? 18u << 16u : 0u));
            level->faces = def->kind == KIND_HLOD ?
                g_hlod_faces[def->cell_index][mesh_lod] :
                g_meshes[def->kind][mesh_lod].mesh.face_count;
            level->command_estimate = level->faces + (level->faces + 7u) / 8u;
            level->resident = 1u;
            level->slot_generation = 1u;
            level->resident_generation = 1u;
        }
    }
}

static uint32_t distance_squared_to_camera(const demo_object_t* object) {
    const int32_t dx = (object->x - g_camera_focus_x) >> 16;
    const int32_t dz = (object->z - g_camera_focus_z) >> 16;
    return (uint32_t)(dx * dx + dz * dz);
}

static void update_camera_and_view(void) {
    const sat_fx16_t sy = sat_sin_deg(g_yaw);
    const sat_fx16_t cy = sat_cos_deg(g_yaw);
    const sat_fx16_t sp = sat_sin_deg(g_pitch);
    const sat_fx16_t cp = sat_cos_deg(g_pitch);
    const sat_fx16_t fx = sat_fx16_mul(sy, cp);
    /* Positive pitch raises the orbit camera and looks down toward the avatar. */
    const sat_fx16_t fy = -sp;
    const sat_fx16_t fz = sat_fx16_mul(cy, cp);
    const sat_vec3_t target = {
        g_camera_focus_x, SAT_FX16(CAMERA_TARGET_Y), g_camera_focus_z};
    const sat_vec3_t eye = {
        target.x - sat_fx16_mul(fx, g_camera_distance),
        target.y - sat_fx16_mul(fy, g_camera_distance),
        target.z - sat_fx16_mul(fz, g_camera_distance)
    };
    const sat_vec3_t up = {0, SAT_FX16_ONE, 0};
    sat_example_must(sat_camera3d_init(&g_camera, &eye, &target, &up,
        SAT_FX16(52), (sat_fx16_t)(((int64_t)SCREEN_W << 16) / SCREEN_H),
        SAT_FX16(1) / 2, SAT_FX16(180)));

    g_view.api_version = SAT_WORLD_LOD3D_API_VERSION;
    g_view.struct_size = sizeof(g_view);
    g_view.generation = g_view_generation;
    g_view.forward_x = fx;
    g_view.forward_y = fy;
    g_view.forward_z = fz;
    g_view.focal_length_px_16_16 = (uint32_t)(((uint64_t)(SCREEN_H / 2) << 32u) /
        (uint32_t)sat_tan_deg(SAT_FX16(26)));
    g_view.near_depth_16_16 = SAT_FX16(1) / 2;
    g_view.viewport_width = SCREEN_W;
    g_view.viewport_height = SCREEN_H;
    {
        const sat_fx16_t tan_y = sat_tan_deg(SAT_FX16(26));
        const sat_fx16_t tan_x = sat_fx16_mul(tan_y,
            (sat_fx16_t)(((int64_t)SCREEN_W << 16) / SCREEN_H));
        const sat_vec3_t right = {-cy, 0, sy};
        sat_vec3_t camera_up;
        {
            const sat_vec3_t forward = {fx, fy, fz};
            sat_vec3_cross(&camera_up, &right, &forward);
        }
        g_view.plane_count = SAT_WORLD_LOD3D_MAX_PLANES;
        {
            sat_vec3_t n = {fx, fy, fz};
            sat_vec3_normalize(&n, &n);
            g_view.planes[0] = (sat_world_lod3d_plane_t){
                n.x, n.y, n.z, -(SAT_FX16(1) / 2)};
            g_view.planes[1] = (sat_world_lod3d_plane_t){
                -n.x, -n.y, -n.z, SAT_FX16(180)};
        }
        {
            const sat_vec3_t forward = {fx, fy, fz};
            const sat_vec3_t horizontal = {
                sat_fx16_mul(forward.x, tan_x), sat_fx16_mul(forward.y, tan_x),
                sat_fx16_mul(forward.z, tan_x)};
            const sat_vec3_t vertical = {
                sat_fx16_mul(forward.x, tan_y), sat_fx16_mul(forward.y, tan_y),
                sat_fx16_mul(forward.z, tan_y)};
            sat_vec3_t normals[4] = {
                {right.x + horizontal.x, right.y + horizontal.y, right.z + horizontal.z},
                {-right.x + horizontal.x, -right.y + horizontal.y, -right.z + horizontal.z},
                {camera_up.x + vertical.x, camera_up.y + vertical.y, camera_up.z + vertical.z},
                {-camera_up.x + vertical.x, -camera_up.y + vertical.y, -camera_up.z + vertical.z}
            };
            for (uint8_t i = 0u; i < 4u; ++i) {
                sat_vec3_normalize(&normals[i], &normals[i]);
                g_view.planes[i + 2u] = (sat_world_lod3d_plane_t){
                    normals[i].x, normals[i].y, normals[i].z, 0};
            }
        }
    }

    for (uint16_t i = 0u; i < OBJECT_COUNT; ++i) {
        sat_world_lod3d_object_t* object = &g_objects[i];
        const demo_object_t* def = &g_definitions[i];
        const sat_fx16_t half_x = def->kind == KIND_TREE ? SAT_FX16(4) : SAT_FX16(1);
        const sat_fx16_t max_y = def->kind == KIND_TREE ? SAT_FX16(14) : SAT_FX16(4);
        const sat_fx16_t x = def->x;
        const sat_fx16_t z = def->z;
        if (def->kind == KIND_HLOD) {
            const sat_aabb3_t* bounds = &g_hlod_meshes[def->cell_index][0].bounds;
            object->bounds_min[0] = x + bounds->center.x - bounds->half.x - eye.x;
            object->bounds_max[0] = x + bounds->center.x + bounds->half.x - eye.x;
            object->bounds_min[1] = def->y + bounds->center.y - bounds->half.y - eye.y;
            object->bounds_max[1] = def->y + bounds->center.y + bounds->half.y - eye.y;
            object->bounds_min[2] = z + bounds->center.z - bounds->half.z - eye.z;
            object->bounds_max[2] = z + bounds->center.z + bounds->half.z - eye.z;
        } else {
            object->bounds_min[0] = x - half_x - eye.x;
            object->bounds_max[0] = x + half_x - eye.x;
            object->bounds_min[1] = def->y - eye.y;
            object->bounds_max[1] = def->y + max_y - eye.y;
            object->bounds_min[2] = z - half_x - eye.z;
            object->bounds_max[2] = z + half_x - eye.z;
        }
        if (i != 0u) {
            const uint32_t d2 = distance_squared_to_camera(def);
            const uint32_t penalty = d2 / 80u;
            object->priority = (uint8_t)(penalty >= 120u ? 8u : 128u - penalty);
        }
    }
}

static sat_result_t make_plan(void) {
    return sat_world_lod3d_plan(
        &g_view, &g_frame_budget, g_objects, OBJECT_COUNT, g_levels, MAX_LEVELS,
        g_states, g_draws, OBJECT_COUNT, g_requests, OBJECT_COUNT,
        g_work, OBJECT_COUNT, &g_planner_stats);
}

static uint8_t object_kind(uint32_t object_id) {
    return g_definitions[object_id].kind;
}

static void submit_plan(void) {
    uint32_t faded = 0u;
    uint32_t hlod_draws = 0u;
    uint32_t hlod_faces = 0u;
    for (uint16_t i = 0u; i < g_planner_stats.draw_count; ++i) {
        const sat_world_lod3d_draw_t* draw = &g_draws[i];
        const uint8_t kind = object_kind(draw->stable_id);
        const uint8_t lod = (uint8_t)(draw->asset_id % MESH_LODS);
        const demo_object_t* def = &g_definitions[draw->stable_id];
        demo_mesh_t* mesh;
        const sat_scene3d_material_t* materials;
        uint16_t material_count;
        const uint16_t* face_materials;
        sat_scene3d_material_t material = kind == KIND_HLOD ?
            g_hlod_materials[0] : g_kind_materials[kind];
        sat_scene3d_material_t faded_material;
        sat_world_lod3d_fade_result_t fade_result;
        sat_mat4_t world;
        const sat_fx16_t x = def->x;
        const sat_fx16_t y = def->y;
        const sat_fx16_t z = def->z;
        sat_result_t result;

        if (g_fade_enabled == 0u && kind != KIND_AVATAR) {
            material.color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
            faded_material = material;
            fade_result = (sat_world_lod3d_fade_result_t){1u, SAT_INDEXED_SOLID_OPAQUE,
                                                          SAT_WORLD_LOD3D_FADE_DISABLED, 0u};
        } else {
            result = sat_world_lod3d_fade_apply(
                &g_fade, draw, &material, &g_fade_composition, g_view_generation,
                &g_fade_states[draw->stable_id], &faded_material, &fade_result);
            sat_example_must(result);
        }
        if (!fade_result.draw) continue;
        if (kind != KIND_AVATAR && fade_result.color_calc_slot != SAT_INDEXED_SOLID_OPAQUE)
            ++faded;

        if (kind == KIND_HLOD) {
            const uint8_t cell = def->cell_index;
            mesh = &g_hlod_meshes[cell][lod];
            ++hlod_draws;
            hlod_faces += mesh->mesh.face_count;
            for (uint8_t material_index = 0u; material_index < 5u; ++material_index) {
                g_hlod_instance_materials[cell][lod][material_index] =
                    g_hlod_materials[material_index];
                g_hlod_instance_materials[cell][lod][material_index].color_calc_slot =
                    fade_result.color_calc_slot;
            }
            materials = g_hlod_instance_materials[cell][lod];
            material_count = 5u;
            face_materials = g_hlod_face_materials[cell][lod];
        } else {
            mesh = &g_meshes[kind][lod];
            materials = &faded_material;
            material_count = 1u;
            face_materials = g_face_materials;
        }

        sat_example_must(sat_mat4_translate(&world, x, y, z));
        g_instance = (sat_scene3d_instance_t){0};
        g_instance.mesh = &mesh->mesh;
        g_instance.materials = materials;
        g_instance.material_count = material_count;
        g_instance.face_materials = face_materials;
        g_instance.world = &world;
        g_instance.pass = 0u;
        g_instance.cull_backfaces = 1u;
        g_instance.bounds = &mesh->bounds;
        sat_example_must(sat_scene_submit_instance(
            &g_scene, &g_instance, SAT_SCENE3D_SLOT_INHERIT,
            g_projected, g_world_vertices));
    }
    g_demo.faded_objects = faded;
    g_demo.hlod_draws = hlod_draws;
    g_demo.hlod_faces = hlod_faces;
    if (hlod_draws > g_demo.hlod_max_draws) g_demo.hlod_max_draws = hlod_draws;
    if (g_hlod_lod0_missing != 0u) {
        for (uint16_t i = 0u; i < g_planner_stats.draw_count; ++i) {
            const sat_world_lod3d_draw_t* draw = &g_draws[i];
            if (g_definitions[draw->stable_id].kind == KIND_HLOD &&
                draw->asset_id % MESH_LODS != 0u)
                ++g_demo.hlod_fallbacks;
        }
        ++g_demo.hlod_missing_lod_test_frames;
    }
    g_demo.hlod_lod0_missing = g_hlod_lod0_missing;
}

static void update_ground_and_sky(void) {
    sat_vdp2_rbg0_ground_config_t cfg = g_ground_base;
    const sat_fx16_t eye_y = g_camera.eye.y;
    uint8_t coefficients_changed = 0u;
    int32_t horizon = HORIZON -
        (int32_t)(((int64_t)sat_tan_deg(g_pitch) * FOCAL_PX) >> 16u);
    if (horizon < 40) horizon = 40;
    if (horizon > 184) horizon = 184;
    cfg.horizon = (uint32_t)horizon;
    cfg.focal = (uint32_t)(sat_fx16_to_int(eye_y) > 1 ? sat_fx16_to_int(eye_y) : 1);
    if (g_ground_coefficients_valid == 0u || g_last_ground_pitch != g_pitch ||
        g_last_ground_eye_y != eye_y) {
        const sat_fx16_t tan_pitch = sat_tan_deg(g_pitch);
        const sat_fx16_t max_distance = SAT_FX16(180);
        const sat_fx16_t minimum_ray_tangent = sat_fx16_div(eye_y, max_distance);
        for (uint32_t y = 0u; y < SCREEN_H; ++y) {
            const int32_t pixel_delta = (int32_t)y - HORIZON;
            const sat_fx16_t tan_screen = (sat_fx16_t)
                (((int64_t)pixel_delta * 65536) / FOCAL_PX);
            const sat_fx16_t product = sat_fx16_mul(tan_pitch, tan_screen);
            const sat_fx16_t denominator = SAT_FX16_ONE - product;
            sat_fx16_t tan_ray = 0;
            sat_fx16_t distance = 0;
            if (denominator != 0) {
                tan_ray = sat_fx16_div(tan_pitch + tan_screen, denominator);
            }
            if (tan_ray <= minimum_ray_tangent) {
                g_coefficients[y * 2u] = 0x8000u;
                g_coefficients[y * 2u + 1u] = 0u;
                continue;
            }
            distance = sat_fx16_div(eye_y, tan_ray);
            if (distance <= 0 || distance > max_distance) {
                g_coefficients[y * 2u] = 0x8000u;
                g_coefficients[y * 2u + 1u] = 0u;
                continue;
            }
            {
                const sat_fx16_t coefficient = sat_fx16_div(
                    distance, SAT_FX16(FOCAL_PX));
                g_coefficients[y * 2u] = (uint16_t)((coefficient >> 16u) & 0x007Fu);
                g_coefficients[y * 2u + 1u] = (uint16_t)coefficient;
            }
        }
        g_last_ground_pitch = g_pitch;
        g_last_ground_eye_y = eye_y;
        g_ground_coefficients_valid = 1u;
        coefficients_changed = 1u;
    }
    g_ground_environment.ground = cfg;
    sat_example_must(sat_vdp2_ground_environment_build_params(
        &g_ground_environment, sat_fx16_to_int(g_camera.eye.x),
        sat_fx16_to_int(g_camera.eye.z)));
    {
        const sat_fx16_t s = sat_sin_deg(g_yaw), c = sat_cos_deg(g_yaw);
        g_rotation[14] = (uint16_t)((-c) >> 16);
        g_rotation[15] = (uint16_t)-c;
        g_rotation[16] = (uint16_t)(s >> 16);
        g_rotation[17] = (uint16_t)s;
        g_rotation[20] = (uint16_t)(s >> 16);
        g_rotation[21] = (uint16_t)s;
        g_rotation[22] = (uint16_t)(c >> 16);
        g_rotation[23] = (uint16_t)c;
    }
    if (coefficients_changed != 0u) {
        sat_example_must(sat_vdp2_vram_write_words(
            GROUND_COEFFICIENT_WORD, g_coefficients, SCREEN_H * 2u));
    }
    sat_example_must(sat_vdp2_ground_environment_commit_params(&g_ground_environment));
    {
        const sat_vdp2_scroll_t scroll = {
            (uint16_t)(((uint32_t)(sat_fx16_to_int(g_yaw) * 4)) & 511u),
            0u, (uint16_t)((128u - cfg.horizon % 128u) % 128u), 0u
        };
        sat_example_must(sat_vdp2_nbg0_set_scroll(&scroll));
    }
    sat_example_must(sat_vdp2_layers_commit());
}

static void update_controls(const sat_pad_state_t* pad) {
    const sat_fx16_t turn = SAT_FX16(2);
    if (pad->held & SAT_PAD_LEFT) g_yaw -= turn;
    if (pad->held & SAT_PAD_RIGHT) g_yaw += turn;
    if (g_yaw < 0) g_yaw += SAT_FX16(360);
    if (g_yaw >= SAT_FX16(360)) g_yaw -= SAT_FX16(360);
    if (pad->held & SAT_PAD_L) g_pitch += SAT_FX16(1);
    if (pad->held & SAT_PAD_R) g_pitch -= SAT_FX16(1);
    g_pitch = clamp_fx(g_pitch, SAT_FX16(-16), SAT_FX16(24));
    if (pad->held & SAT_PAD_UP) {
        g_camera_focus_x += sat_fx16_mul(sat_sin_deg(g_yaw), SAT_FX16_ONE / 2);
        g_camera_focus_z += sat_fx16_mul(sat_cos_deg(g_yaw), SAT_FX16_ONE / 2);
    }
    if (pad->held & SAT_PAD_DOWN) {
        g_camera_focus_x -= sat_fx16_mul(sat_sin_deg(g_yaw), SAT_FX16_ONE / 2);
        g_camera_focus_z -= sat_fx16_mul(sat_cos_deg(g_yaw), SAT_FX16_ONE / 2);
    }
    g_camera_focus_x = clamp_fx(g_camera_focus_x, -SAT_FX16(384), SAT_FX16(384));
    g_camera_focus_z = clamp_fx(g_camera_focus_z, -SAT_FX16(384), SAT_FX16(384));
    if (pad->held & SAT_PAD_A) g_camera_distance += SAT_FX16_ONE / 4;
    if (pad->held & SAT_PAD_B) g_camera_distance -= SAT_FX16_ONE / 4;
    g_camera_distance = clamp_fx(g_camera_distance,
        SAT_FX16(CAMERA_DISTANCE_MIN), SAT_FX16(CAMERA_DISTANCE_MAX));
    if (pad->pressed & SAT_PAD_Y) {
        g_governor_config.enabled = (uint8_t)!g_governor_config.enabled;
        g_governor_state = (sat_world_lod3d_governor_state_t){0};
        g_frame_budget = g_base_budget;
    }
    if (pad->pressed & SAT_PAD_C) {
        g_fade_enabled = (uint8_t)!g_fade_enabled;
        g_fade_composition.fade_enabled = g_fade_enabled;
        for (uint16_t i = 0u; i < OBJECT_COUNT; ++i) {
            g_fade_states[i] = (sat_world_lod3d_fade_state_t){0};
        }
    }
    if (pad->pressed & SAT_PAD_Z) {
        g_hlod_lod0_missing = (uint8_t)!g_hlod_lod0_missing;
        for (uint16_t i = 0u; i < OBJECT_COUNT; ++i) {
            if (g_definitions[i].kind == KIND_HLOD) {
                const uint16_t first = g_objects[i].levels_first;
                g_levels[first].resident = (uint8_t)!g_hlod_lod0_missing;
            }
        }
    }
    if (pad->pressed & SAT_PAD_X) {
        g_camera_focus_x = 0;
        g_camera_focus_z = 0;
        g_yaw = 0;
        g_pitch = 0;
        g_hlod_lod0_missing = 0u;
        ++g_view_generation;
        if (g_view_generation == 0u) ++g_view_generation;
        for (uint16_t i = 0u; i < OBJECT_COUNT; ++i) {
            g_states[i] = (sat_world_lod3d_state_t){0};
            g_fade_states[i] = (sat_world_lod3d_fade_state_t){0};
            if (g_definitions[i].kind == KIND_HLOD)
                g_levels[g_objects[i].levels_first].resident = 1u;
        }
    }
}

static void draw_hud(void) {
    sat_example_must(sat_hud_text(&g_hud, "OUTDOOR LOD  UP/DN FLY  LEFT/RIGHT YAW", 4, 4));
    sat_example_must(sat_hud_text(&g_hud, "L/R PITCH  A/B ZOOM  Y GOVERNOR  C FADE  X RESET", 4, 14));
    sat_example_must(sat_hud_value(&g_hud, "HLOD CELLS ",
                                   SAT_WORLD_HLOD_DEMO_CELL_COUNT, 4, 24));
    sat_example_must(sat_hud_value(&g_hud, "BYTES ",
                                   SAT_WORLD_HLOD_DEMO_PACKAGE_BYTES, 104, 24));
    sat_example_must(sat_hud_value(&g_hud, "L0 ", g_planner_stats.selected_by_lod[0], 4, 202));
    sat_example_must(sat_hud_value(&g_hud, "L1 ", g_planner_stats.selected_by_lod[1], 56, 202));
    sat_example_must(sat_hud_value(&g_hud, "L2 ", g_planner_stats.selected_by_lod[2], 108, 202));
    sat_example_must(sat_hud_value(&g_hud, "FACE ", g_planner_stats.faces, 160, 202));
    sat_example_must(sat_hud_value(&g_hud, "CMD ", g_governor_stats.world_commands, 236, 202));
}

static void reset_fade_state(void) {
    for (uint16_t i = 0u; i < OBJECT_COUNT; ++i) {
        g_fade_states[i] = (sat_world_lod3d_fade_state_t){0};
    }
}

int main(void) {
    const sat_video_config_t video = {SCREEN_W, SCREEN_H, SAT_VIDEO_AUTO, 0u};
    sat_pad_state_t pad = {0};
    sat_example_must(sat_init(&video));
    sat_example_must(sat_vdp1_set_erase_transparent());
    build_background();
    build_meshes();
    build_textures();
    build_hlod_meshes();
    build_objects();
    sat_example_must(sat_scene_init(&g_scene, g_scene_faces, g_scene_keys,
                                    g_scene_order, SCENE_FACE_CAPACITY));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, FONT_PALETTE));
    sat_example_must(sat_hud_init(&g_hud, &g_font, SAT_COLOR_WHITE, 8u));
    sat_example_must(sat_world_lod3d_governor_config_default(&g_governor_config));
    g_base_budget.max_faces = GOVERNOR_BASE_FACES;
    g_base_budget.max_commands = GOVERNOR_BASE_COMMANDS;
    g_base_budget.max_texture_bytes = 0u;
    g_base_budget.max_budget_lod_loss = 1u;
    g_frame_budget = g_base_budget;
    g_governor_config.minimum_faces = 36u;
    g_governor_config.minimum_commands = 36u;
    g_governor_config.target_fields = 2u;
    g_governor_config.target_cpu_frt_ticks = 0u;
    g_fade_composition.fade_enabled = 1u;
    reset_fade_state();
    g_view_generation = 1u;
    g_demo.magic = WORLD_LOD3D_DEMO_MAGIC;
    for (;;) {
        const uint32_t frame_field_start = sat_field_count();
        uint32_t cpu_ticks;
        sat_example_must(sat_wait_vblank());
        g_frame_start = sat_time_frc();
        sat_example_must(sat_pad_poll(&pad));
        update_controls(&pad);
        update_camera_and_view();
        update_ground_and_sky();
        sat_example_must(sat_begin_frame());
        sat_example_must(sat_scene_begin(
            &g_scene, &g_camera, SAT_FX16(1) / 2, SCREEN_W, SCREEN_H,
            OVERLAY_COMMANDS));
        {
            const sat_result_t plan_result = make_plan();
            g_demo.planner_status = (uint32_t)(-plan_result);
            sat_example_must(plan_result);
        }
        submit_plan();
        sat_example_must(sat_scene_flush(&g_scene));
        draw_hud();
        sat_example_must(sat_end_frame());

        cpu_ticks = (uint16_t)(sat_time_frc() - g_frame_start);
        {
            sat_world_lod3d_governor_telemetry_t telemetry = {0};
            sat_world_lod3d_budget_t next_budget = {0};
            sat_scene_stats_t stats;
            sat_example_must(sat_scene_stats(&g_scene, &stats));
            telemetry.scene = stats;
            telemetry.field_count = sat_field_count();
            telemetry.cpu_frt_ticks = cpu_ticks;
            telemetry.profile_available = 1u;
            telemetry.paused = (pad.pressed & SAT_PAD_X) != 0u;
            sat_example_must(sat_world_lod3d_governor_step(
                &g_governor_config, &telemetry, &g_base_budget,
                &g_governor_state, &next_budget, &g_governor_stats));
            g_frame_budget = next_budget;
            g_demo.scene_result = (uint32_t)(-stats.result);
            g_demo.commands = stats.world_commands;
            g_demo.considered = g_planner_stats.considered;
            g_demo.frustum_rejected = g_planner_stats.frustum_rejected;
            g_demo.no_resident = g_planner_stats.no_resident_lod;
            g_demo.cpu_frt_ticks = cpu_ticks;
        }

        g_demo.frames++;
        g_demo.wanted_lod0 = g_planner_stats.desired_by_lod[0];
        g_demo.wanted_lod1 = g_planner_stats.desired_by_lod[1];
        g_demo.wanted_lod2 = g_planner_stats.desired_by_lod[2];
        g_demo.selected_lod0 = g_planner_stats.selected_by_lod[0];
        g_demo.selected_lod1 = g_planner_stats.selected_by_lod[1];
        g_demo.selected_lod2 = g_planner_stats.selected_by_lod[2];
        g_demo.dropped = g_planner_stats.optional_dropped;
        g_demo.faces = g_planner_stats.faces;
        g_demo.face_budget = g_frame_budget.max_faces;
        g_demo.command_budget = g_frame_budget.max_commands;
        g_demo.governor_enabled = g_governor_config.enabled;
        g_demo.governor_samples = g_governor_stats.sample_count;
        g_demo.governor_transitions = g_governor_stats.degraded_transitions;
        g_demo.fade_enabled = g_fade_enabled;
        g_demo.elapsed_fields = sat_field_count() - frame_field_start;
        if (g_demo.elapsed_fields == 1u) ++g_demo.field_hist_1;
        else if (g_demo.elapsed_fields == 2u) ++g_demo.field_hist_2;
        else if (g_demo.elapsed_fields == 3u) ++g_demo.field_hist_3;
        else if (g_demo.elapsed_fields == 4u) ++g_demo.field_hist_4;
        else ++g_demo.field_hist_gt4;
        g_demo.cpu_frt_sum += cpu_ticks;
        g_demo.camera_x_fx = (uint32_t)g_camera_focus_x;
        g_demo.camera_z_fx = (uint32_t)g_camera_focus_z;
        g_demo.yaw_degrees = (uint32_t)sat_fx16_to_int(g_yaw);
        g_demo.pitch_degrees = (uint32_t)sat_fx16_to_int(g_pitch);
        g_demo.camera_distance_fx = (uint32_t)g_camera_distance;
        g_demo.view_generation = g_view_generation;
        g_world_lod3d_demo = g_demo;
    }
    return 0;
}
