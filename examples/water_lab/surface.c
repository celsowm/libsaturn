#include "lab.h"
#include "example_util.h"
#include "saturn/color.h"

typedef struct water_vertex {
    sat_vec3_t point;
    uint16_t light;
} water_vertex_t;

static water_vertex_t pool_vertex(const lab_water_t* water, sat_fx16_t x,
                                  sat_fx16_t z, uint32_t tick) {
    const sat_fx16_t wave = sat_sin_deg(x * 17 + z * 31 - LAB_UNIT(tick % 360u * 5u));
    const sat_fx16_t cross = sat_sin_deg(x * 43 - z * 13 + LAB_UNIT(tick % 360u * 3u));
    const sat_fx16_t crest = wave + cross / 2;
    const int light = crest / (SAT_FX16_ONE / 5);
    return (water_vertex_t){
        {x, water->height + (water->waves ? wave / 8 + cross / 18 : 0), z},
        SAT_RGB555(16 + light / 2, 16 + light, 16 + light / 2)
    };
}

static sat_vec3_t ripple_point(const lab_water_t* water, sat_fx16_t x, sat_fx16_t z,
                               sat_fx16_t radius, sat_fx16_t angle, uint32_t tick) {
    const sat_fx16_t px = x + sat_fx16_mul(radius, sat_cos_deg(angle));
    const sat_fx16_t pz = z + sat_fx16_mul(radius, sat_sin_deg(angle));
    sat_vec3_t point = pool_vertex(water, px, pz, tick).point;
    point.y += SAT_FX16_ONE / 6;
    return point;
}

static void pool_ripples(sat_scene_t* scene, const lab_water_t* water, uint32_t tick) {
    static const int8_t centers[][2] = {{-5, 3}, {5, 4}, {-4, -4}, {4, -3}, {0, 5}};
    sat_scene3d_material_t material = {
        .kind = SAT_SCENE3D_RGB, .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE,
        .sprite_flags = SAT_SPRITE_FLAG_HALF_TRANSPARENT
    };
    for (unsigned i = 0; i < sizeof(centers) / sizeof(centers[0]); ++i) {
        const unsigned age = (tick + i * 23u) % 88u;
        const sat_fx16_t radius = SAT_FX16_ONE / 3 + (sat_fx16_t)age * (SAT_FX16_ONE / 56);
        const sat_fx16_t outer = radius + SAT_FX16_ONE / 6;
        const unsigned shine = age < 44 ? age / 3 : (88 - age) / 3;
        material.rgb555 = SAT_RGB555(15 + shine, 23 + shine / 2, 25 + shine / 3);
        const sat_fx16_t x = LAB_UNIT(centers[i][0]), z = LAB_UNIT(centers[i][1]);
        for (unsigned arc = 0; arc < 3; ++arc) {
            const sat_fx16_t start = LAB_UNIT((i * 37u + arc * 120u + age / 2) % 360u);
            sat_vec3_t inner_start = ripple_point(water, x, z, radius, start, tick);
            sat_vec3_t outer_start = ripple_point(water, x, z, outer, start, tick);
            for (unsigned segment = 0; segment < 3; ++segment) {
                const sat_fx16_t end = start + LAB_UNIT((segment + 1u) * 18u);
                const sat_vec3_t inner_end = ripple_point(water, x, z, radius, end, tick);
                const sat_vec3_t outer_end = ripple_point(water, x, z, outer, end, tick);
                const sat_quad3_t ribbon = {{inner_start, outer_start, outer_end, inner_end}};
                sat_example_must(sat_scene_submit_quad(scene, &ribbon, &material, 0));
                inner_start = inner_end;
                outer_start = outer_end;
            }
        }
    }
}

static void pool_surface(sat_scene_t* scene, const lab_water_t* water, uint32_t tick) {
    const unsigned n = water->subdivisions;
    const sat_fx16_t step = LAB_UNIT(16) / (int)n;
    water_vertex_t rows[2][LAB_MAX_SUBDIVISIONS + 1];
    for (unsigned z = 0; z <= n; ++z) {
        water_vertex_t* next = rows[z % 2];
        const water_vertex_t* prev = rows[(z + 1) % 2];
        for (unsigned x = 0; x <= n; ++x) {
            next[x] = pool_vertex(water, LAB_UNIT(-8) + (int)x * step,
                                 LAB_UNIT(-8) + (int)z * step, water->waves ? tick : 0);
        }
        for (unsigned x = 0; z > 0 && x < n; ++x) {
            const sat_quad3_t quad = {{prev[x].point, prev[x + 1].point,
                                      next[x + 1].point, next[x].point}};
            const uint16_t light[] = {prev[x].light, prev[x + 1].light,
                                      next[x + 1].light, next[x].light};
            const sat_scene3d_material_t material = {
                .kind = SAT_SCENE3D_RGB, .rgb555 = SAT_RGB555(2, 16, 24),
                .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE, .vertex_gouraud = light,
                .sprite_flags = SAT_SPRITE_FLAG_HALF_TRANSPARENT
            };
            sat_example_must(sat_scene_submit_quad(scene, &quad, &material, 0));
        }
    }
    if (water->waves) pool_ripples(scene, water, tick);
}

void lab_surface_draw(sat_scene_t* scene, const lab_water_t* water, uint32_t tick) {
    if (water->mode == LAB_DRY) return;
    if (water->mode == LAB_POOL) {
        pool_surface(scene, water, tick);
        return;
    }
    const unsigned n = water->subdivisions;
    const sat_fx16_t step = LAB_UNIT(16) / (int)n;
    sat_fx16_t offsets[LAB_MAX_SUBDIVISIONS];
    for (unsigned x = 0; x < n; ++x) {
        offsets[x] = water->waves && water->mode != LAB_UNDERWATER
            ? sat_sin_deg(LAB_UNIT((tick * 3u + x * 360u / n) % 360u)) / 9
            : 0;
    }
    sat_scene3d_material_t material = {
        .kind = SAT_SCENE3D_RGB,
        .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE
    };
    if (water->mode == LAB_MESH) material.sprite_flags = SAT_SPRITE_FLAG_MESH;
    if (water->mode >= LAB_HALF) material.sprite_flags = SAT_SPRITE_FLAG_HALF_TRANSPARENT;

    for (unsigned z = 0; z < n; ++z) {
        for (unsigned x = 0; x < n; ++x) {
            const sat_fx16_t x0 = LAB_UNIT(-8) + (int)x * step;
            const sat_fx16_t z0 = LAB_UNIT(-8) + (int)z * step;
            const sat_fx16_t y00 = water->height + offsets[(x + z) % n];
            const sat_fx16_t y01 = water->height + offsets[(x + z + 1u) % n];
            const sat_fx16_t y11 = water->height + offsets[(x + z + 2u) % n];
            const sat_quad3_t quad = {{{x0,y00,z0}, {x0+step,y01,z0},
                {x0+step,y11,z0+step}, {x0,y01,z0+step}}};
            material.rgb555 = y00 < y11
                ? SAT_RGB555(6, 23, 25) : SAT_RGB555(5, 20, 23);
            sat_example_must(sat_scene_submit_quad(scene, &quad, &material, 0));
        }
    }
}
