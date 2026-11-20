#include "lab.h"
#include "example_util.h"
#include "saturn/color.h"

enum {
    GRID_VERTICES = (LAB_MAX_SUBDIVISIONS + 1) * (LAB_MAX_SUBDIVISIONS + 1),
    GRID_FACES = LAB_MAX_SUBDIVISIONS * LAB_MAX_SUBDIVISIONS,
    RIPPLE_CENTERS = 5, RIPPLE_ARCS = 3, RIPPLE_SEGMENTS = 3,
    RIPPLE_VERTICES = RIPPLE_CENTERS * RIPPLE_ARCS * 2 * (RIPPLE_SEGMENTS + 1),
    RIPPLE_FACES = RIPPLE_CENTERS * RIPPLE_ARCS * RIPPLE_SEGMENTS
};

static sat_fx16_t sine[360];
static sat_vec3_t vertices[GRID_VERTICES];
static sat_projected_vertex_t projected[GRID_VERTICES];
static uint16_t indices[4 * GRID_FACES];
static uint16_t face_materials[GRID_FACES];
static uint16_t lighting[GRID_VERTICES];
static unsigned grid_size;

void lab_surface_init(void) {
    for (int angle = 0; angle < 360; ++angle) {
        sine[angle] = sat_sin_deg(LAB_UNIT(angle));
    }
}

sat_fx16_t lab_sin_degrees(int degrees) {
    const int angle = degrees % 360;
    return sine[angle < 0 ? angle + 360 : angle];
}

static sat_fx16_t wave_height(const lab_water_t* water, sat_fx16_t wave, sat_fx16_t cross) {
    return water->height + (water->waves ? wave / 8 + cross / 18 : 0);
}

static void submit_mesh(sat_scene_t* scene, unsigned vertex_count, unsigned face_count,
                        const sat_scene3d_material_t* materials, unsigned material_count) {
    const sat_mesh_t mesh = {
        .vertices = vertices, .indices = indices,
        .vertex_cap = GRID_VERTICES, .vertex_count = vertex_count,
        .face_cap = GRID_FACES, .face_count = face_count
    };
    const sat_scene3d_instance_t instance = {
        .mesh = &mesh, .materials = materials, .material_count = material_count,
        .face_materials = face_materials
    };
    sat_example_must(sat_scene_submit_instance(
        scene, &instance, SAT_SCENE3D_SLOT_INHERIT, projected, 0));
}

static void ripple_pair(const lab_water_t* water, sat_fx16_t x, sat_fx16_t z,
                        sat_fx16_t radius, int angle, int phase, sat_vec3_t* vertices_out) {
    const sat_fx16_t dx = lab_sin_degrees(angle + 90);
    const sat_fx16_t dz = lab_sin_degrees(angle);
    for (unsigned side = 0; side < 2; ++side) {
        const sat_fx16_t px = x + sat_fx16_mul(radius, dx);
        const sat_fx16_t pz = z + sat_fx16_mul(radius, dz);
        const sat_fx16_t wave = sat_sin_deg(px * 17 + pz * 31 - LAB_UNIT(phase * 5));
        const sat_fx16_t cross = sat_sin_deg(px * 43 - pz * 13 + LAB_UNIT(phase * 3));
        *vertices_out++ = (sat_vec3_t){px, wave_height(water, wave, cross) + SAT_FX16_ONE / 6, pz};
        radius += SAT_FX16_ONE / 6;
    }
}

static void pool_ripples(sat_scene_t* scene, const lab_water_t* water, uint32_t tick) {
    static const int8_t centers[RIPPLE_CENTERS][2] = {{-5, 3}, {5, 4}, {-4, -4}, {4, -3}, {0, 5}};
    sat_scene3d_material_t materials[RIPPLE_CENTERS];
    const int phase = tick % 360u;
    unsigned vertex = 0, face = 0;
    for (unsigned i = 0; i < RIPPLE_CENTERS; ++i) {
        const unsigned age = (tick + i * 23u) % 88u;
        const sat_fx16_t radius = SAT_FX16_ONE / 3 + (sat_fx16_t)age * (SAT_FX16_ONE / 56);
        const unsigned shine = age < 44 ? age / 3 : (88 - age) / 3;
        materials[i] = (sat_scene3d_material_t){
            .kind = SAT_SCENE3D_RGB, .rgb555 = SAT_RGB555(15 + shine, 23 + shine / 2, 25 + shine / 3),
            .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE,
            .sprite_flags = SAT_SPRITE_FLAG_HALF_TRANSPARENT
        };
        const sat_fx16_t x = LAB_UNIT(centers[i][0]), z = LAB_UNIT(centers[i][1]);
        for (unsigned arc = 0; arc < RIPPLE_ARCS; ++arc) {
            const int start = i * 37u + arc * 120u + age / 2;
            for (unsigned segment = 0; segment <= RIPPLE_SEGMENTS; ++segment) {
                ripple_pair(water, x, z, radius, start + (int)segment * 18, phase, &vertices[vertex]);
                if (segment > 0) {
                    indices[4 * face] = vertex - 2;
                    indices[4 * face + 1] = vertex - 1;
                    indices[4 * face + 2] = vertex + 1;
                    indices[4 * face + 3] = vertex;
                    face_materials[face++] = i;
                }
                vertex += 2;
            }
        }
    }
    submit_mesh(scene, RIPPLE_VERTICES, RIPPLE_FACES, materials, RIPPLE_CENTERS);
    /* The next frame's surface rebuilds the shared index buffer. */
    grid_size = 0;
}

static void prepare_grid(unsigned n) {
    if (grid_size == n) return;
    unsigned face = 0;
    for (unsigned z = 0; z < n; ++z) {
        for (unsigned x = 0; x < n; ++x) {
            const unsigned corner = z * (n + 1) + x;
            indices[4 * face] = corner;
            indices[4 * face + 1] = corner + 1;
            indices[4 * face + 2] = corner + n + 2;
            indices[4 * face + 3] = corner + n + 1;
            ++face;
        }
    }
    grid_size = n;
}

static void pool_surface(sat_scene_t* scene, const lab_water_t* water, uint32_t tick) {
    const unsigned n = water->subdivisions;
    const int step = 16 / (int)n;
    const int phase = water->waves ? (int)(tick % 360u) : 0;
    unsigned vertex = 0;
    for (unsigned row = 0; row <= n; ++row) {
        const int z = -8 + (int)row * step;
        for (unsigned column = 0; column <= n; ++column) {
            const int x = -8 + (int)column * step;
            const sat_fx16_t wave = lab_sin_degrees(x * 17 + z * 31 - phase * 5);
            const sat_fx16_t cross = lab_sin_degrees(x * 43 - z * 13 + phase * 3);
            const int light = (wave + cross / 2) / (SAT_FX16_ONE / 5);
            vertices[vertex] = (sat_vec3_t){LAB_UNIT(x), wave_height(water, wave, cross), LAB_UNIT(z)};
            lighting[vertex++] = SAT_RGB555(16 + light / 2, 16 + light, 16 + light / 2);
        }
    }
    const sat_scene3d_material_t material = {
        .kind = SAT_SCENE3D_RGB, .rgb555 = SAT_RGB555(2, 16, 24),
        .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE, .vertex_gouraud = lighting,
        .sprite_flags = SAT_SPRITE_FLAG_HALF_TRANSPARENT
    };
    for (unsigned face = 0; face < n * n; ++face) face_materials[face] = 0;
    submit_mesh(scene, vertex, n * n, &material, 1);
    if (water->waves) pool_ripples(scene, water, tick);
}

void lab_surface_draw(sat_scene_t* scene, const lab_water_t* water, uint32_t tick) {
    if (water->mode == LAB_DRY) return;
    const unsigned n = water->subdivisions;
    if (n != 4 && n != 8 && n != LAB_MAX_SUBDIVISIONS) return;
    prepare_grid(n);
    if (water->mode == LAB_POOL) {
        pool_surface(scene, water, tick);
        return;
    }

    const sat_fx16_t step = LAB_UNIT(16) / (int)n;
    sat_fx16_t heights[LAB_MAX_SUBDIVISIONS];
    for (unsigned x = 0; x < n; ++x) {
        heights[x] = water->height;
        if (water->waves && water->mode != LAB_UNDERWATER) {
            heights[x] += lab_sin_degrees((tick * 3u + x * 360u / n) % 360u) / 9;
        }
    }
    const unsigned mask = n - 1;
    unsigned vertex = 0;
    for (unsigned z = 0; z <= n; ++z) {
        for (unsigned x = 0; x <= n; ++x) {
            vertices[vertex++] = (sat_vec3_t){
                LAB_UNIT(-8) + (int)x * step, heights[(x + z) & mask], LAB_UNIT(-8) + (int)z * step
            };
        }
    }
    unsigned face = 0;
    for (unsigned z = 0; z < n; ++z) {
        for (unsigned x = 0; x < n; ++x) {
            face_materials[face++] = heights[(x + z) & mask] < heights[(x + z + 2) & mask];
        }
    }
    uint32_t flags = 0;
    if (water->mode == LAB_MESH) flags = SAT_SPRITE_FLAG_MESH;
    if (water->mode >= LAB_HALF) flags = SAT_SPRITE_FLAG_HALF_TRANSPARENT;
    const sat_scene3d_material_t materials[] = {
        {.kind = SAT_SCENE3D_RGB, .rgb555 = SAT_RGB555(5, 20, 23),
         .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE, .sprite_flags = flags},
        {.kind = SAT_SCENE3D_RGB, .rgb555 = SAT_RGB555(6, 23, 25),
         .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE, .sprite_flags = flags}
    };
    submit_mesh(scene, vertex, face, materials, 2);
}
