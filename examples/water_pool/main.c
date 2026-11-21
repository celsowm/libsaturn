#include "example_util.h"
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/input.h"
#include "saturn/mesh3d.h"
#include "saturn/render3d.h"
#include "saturn/scene.h"
#include "saturn/time.h"
#include "saturn/vdp1.h"
#include "saturn/water.h"
#include "saturn/water_sim.h"

/* A tiled pool with a floating ball and a live wave simulation, after Evan
 * Wallace's WebGL Water. Reflection, refraction and caustics are approximated
 * with per-vertex Gouraud offsets from sat_water_sim_shade and
 * sat_water_sim_caustics rather than computed per pixel. */

#define UNIT(n) ((n) * SAT_FX16_ONE)

enum {
    WIDTH = 320, HEIGHT = 224,
    FACE_CAPACITY = 1024, OVERLAY_COMMANDS = 64,
    MAX_NODES = 17, MAX_VERTICES = MAX_NODES * MAX_NODES,
    SCRIPT_TICKS = 70, TILE = 8, SPHERE_SEGMENTS = 12, SPHERE_RINGS = 8,
    SPHERE_VERTICES = (SPHERE_RINGS + 1) * SPHERE_SEGMENTS,
    SPHERE_FACES = SPHERE_RINGS * SPHERE_SEGMENTS
};

static const sat_fx16_t HALF = UNIT(6), RIM = 0, REST = -UNIT(3) / 2, FLOOR = -UNIT(5);
static const sat_fx16_t RADIUS = SAT_FX16_ONE * 3 / 2;

typedef struct ball {
    sat_vec3_t position;
    sat_fx16_t velocity_y;
} ball_t;

/* Big-endian uint32_t words, read through the Ymir harness using the ELF symbol. */
volatile struct {
    uint32_t tick;
    uint32_t nodes;
    uint32_t volume_high, volume_low;
    uint32_t faces;
    uint32_t preparation_ticks;
    uint32_t flush_ticks;
    uint32_t tick_frequency;
    int32_t ball_x, ball_y, ball_z;
    uint32_t gravity;
    uint32_t paused;
    uint32_t script;
} water_pool_telemetry;

static sat_scene_t scene;
static sat_scene3d_face_t faces[FACE_CAPACITY];
static uint32_t keys[FACE_CAPACITY];
static uint16_t order[FACE_CAPACITY];

static sat_fx16_t heights[MAX_VERTICES], velocities[MAX_VERTICES];
static sat_water_sim_t sim;
static sat_vec3_t vertices[4 * MAX_NODES];
static sat_vec3_t grid[MAX_VERTICES];
static sat_projected_vertex_t projected[MAX_VERTICES];
static uint16_t indices[4 * (MAX_NODES - 1) * (MAX_NODES - 1)];
static uint16_t face_materials[(MAX_NODES - 1) * (MAX_NODES - 1)];
static uint16_t surface_light[MAX_VERTICES], floor_light[MAX_VERTICES], caustic_light[MAX_VERTICES];
static uint16_t wall_light[4 * MAX_NODES];
static unsigned grid_nodes;

static sat_vdp1_texture_t tile;
static sat_mesh_t sphere;
static sat_vec3_t sphere_vertices[SPHERE_VERTICES];
static uint16_t sphere_indices[4 * SPHERE_FACES];
static sat_vec3_t sphere_normals[SPHERE_VERTICES];
static uint16_t sphere_light[SPHERE_VERTICES];

static sat_fx16_t spacing(void) { return 2 * HALF / (int)(sim.size - 1); }
static sat_fx16_t node_coord(sat_fx16_t world) {
    return (sat_fx16_t)((int64_t)(world + HALF) * SAT_FX16_ONE / spacing());
}
static sat_fx16_t world_coord(unsigned node) { return -HALF + (sat_fx16_t)node * spacing(); }

/* The same channel encoding as sat_gouraud_rgb: levels around neutral 16. */
static uint16_t tint(uint16_t entry, int dr, int dg, int db) {
    const int r = (entry & 31) - 16 + dr, g = ((entry >> 5) & 31) - 16 + dg,
              b = ((entry >> 10) & 31) - 16 + db;
    return sat_gouraud_rgb(r, g, b);
}

static void make_tile(void) {
    uint8_t pixels[TILE * TILE], packed[TILE * TILE / 2];
    uint16_t colors[16] = {0};
    for (unsigned y = 0; y < TILE; ++y) {
        for (unsigned x = 0; x < TILE; ++x) {
            const unsigned dx = x < 4 ? x : 7 - x, dy = y < 4 ? y : 7 - y;
            pixels[y * TILE + x] = dx + dy == 3 ? 1 : 2 + ((x ^ y) & 1u);
        }
    }
    colors[1] = SAT_RGB555(12, 17, 20);
    colors[2] = SAT_RGB555(26, 29, 30);
    colors[3] = SAT_RGB555(23, 27, 29);
    for (unsigned i = 0; i < sizeof(packed); ++i) {
        packed[i] = (uint8_t)((pixels[2 * i] << 4) | pixels[2 * i + 1]);
    }
    uint16_t lut;
    sat_example_must(sat_vdp1_upload_lut(colors, &lut));
    sat_example_must(sat_tex_upload_lut4_pixels(&tile, packed, TILE, TILE, lut));
}

static void make_ball(void) {
    sat_example_must(sat_mesh_init(&sphere, sphere_vertices, SPHERE_VERTICES,
                                   sphere_indices, SPHERE_FACES));
    const sat_vec3_t origin = {0, 0, 0};
    sat_example_must(sat_mesh_build_sphere(&sphere, &origin, RADIUS, SPHERE_SEGMENTS, SPHERE_RINGS));
    sat_example_must(sat_mesh_vertex_normals(&sphere, sphere_normals, SPHERE_VERTICES));
    const sat_vec3_t light = {26214, 45875, 26214};
    sat_example_must(sat_gouraud_lambert(sphere_normals, sphere.vertex_count, &light,
                                         SAT_FX16_ONE / 3, sphere_light));
}

static void prepare_indices(unsigned n) {
    if (grid_nodes == n) return;
    unsigned face = 0;
    for (unsigned z = 0; z + 1 < n; ++z) {
        for (unsigned x = 0; x + 1 < n; ++x, ++face) {
            const unsigned corner = z * n + x;
            indices[4 * face] = corner;
            indices[4 * face + 1] = corner + 1;
            indices[4 * face + 2] = corner + n + 1;
            indices[4 * face + 3] = corner + n;
            face_materials[face] = 0;
        }
    }
    grid_nodes = n;
}

static void submit_grid(const sat_scene3d_material_t* material, unsigned vertex_count) {
    const unsigned n = sim.size;
    const sat_mesh_t mesh = {
        .vertices = grid, .indices = indices,
        .vertex_cap = MAX_VERTICES, .vertex_count = vertex_count,
        .face_cap = (MAX_NODES - 1) * (MAX_NODES - 1), .face_count = (n - 1) * (n - 1)
    };
    const sat_scene3d_instance_t instance = {
        .mesh = &mesh, .materials = material, .material_count = 1,
        .face_materials = face_materials
    };
    sat_example_must(sat_scene_submit_instance(
        &scene, &instance, SAT_SCENE3D_SLOT_INHERIT, projected, 0));
}

static void draw_floor(void) {
    const unsigned n = sim.size;
    for (unsigned z = 0; z < n; ++z)
        for (unsigned x = 0; x < n; ++x)
            grid[z * n + x] = (sat_vec3_t){world_coord(x), FLOOR, world_coord(z)};
    sat_example_must(sat_water_sim_caustics(&sim, 60 * SAT_FX16_ONE, caustic_light));
    for (unsigned i = 0; i < n * n; ++i) floor_light[i] = tint(caustic_light[i], -9, -4, -1);
    const sat_scene3d_material_t material = {
        .kind = SAT_SCENE3D_INDEXED_TEXTURED, .rgb555 = SAT_RGB555(8, 14, 18),
        .texture = &tile, .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE,
        .vertex_gouraud = floor_light
    };
    submit_grid(&material, n * n);
}

static void draw_surface(void) {
    const unsigned n = sim.size;
    for (unsigned z = 0; z < n; ++z)
        for (unsigned x = 0; x < n; ++x)
            grid[z * n + x] = (sat_vec3_t){world_coord(x), REST + heights[z * n + x], world_coord(z)};
    const sat_water_sim_shading_t shading = {
        .spacing = spacing(), .light_x = SAT_FX16_ONE / 2, .light_z = SAT_FX16_ONE / 2,
        .light_gain = {60 * SAT_FX16_ONE, 64 * SAT_FX16_ONE, 64 * SAT_FX16_ONE},
        .sky_gain = {20 * SAT_FX16_ONE, 24 * SAT_FX16_ONE, 28 * SAT_FX16_ONE}
    };
    sat_example_must(sat_water_sim_shade(&sim, &shading, surface_light));
    const sat_scene3d_material_t material = {
        .kind = SAT_SCENE3D_RGB, .rgb555 = SAT_RGB555(3, 14, 22),
        .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE, .vertex_gouraud = surface_light,
        .sprite_flags = SAT_SPRITE_FLAG_HALF_TRANSPARENT
    };
    submit_grid(&material, n * n);
}

/* Needs draw_floor's caustics for the same frame. One wall along x or z, as a strip of columns over four vertex rows: rim,
 * the waterline above the water, the waterline below it, and the floor. The
 * two waterline rows share the edge node's wave height, so the wall follows
 * the surface and dry and wet faces get separate tint. */
static void draw_wall(unsigned side) {
    const unsigned n = sim.size;
    const sat_fx16_t low = -HALF, high = HALF;
    for (unsigned i = 0; i < n; ++i) {
        const sat_fx16_t along = world_coord(i);
        unsigned nx = i, nz = 0;
        sat_fx16_t x = along, z = low;
        if (side == 1) { nx = n - 1; nz = i; x = high; z = along; }
        if (side == 2) { nx = n - 1 - i; nz = n - 1; x = -along; z = high; }
        if (side == 3) { nx = 0; nz = n - 1 - i; x = low; z = -along; }
        const sat_fx16_t water = REST + heights[nz * n + nx];
        const sat_fx16_t ys[4] = {RIM, water, water, FLOOR};
        uint16_t caustic[3];
        caustic[0] = 0x4210u;
        caustic[1] = tint(caustic_light[nz * n + nx], -6, -2, -1);
        caustic[2] = tint(caustic_light[nz * n + nx], -11, -6, -2);
        for (unsigned row = 0; row < 4; ++row) {
            vertices[row * n + i] = (sat_vec3_t){x, ys[row], z};
        }
        wall_light[0 * n + i] = caustic[0];
        wall_light[1 * n + i] = caustic[0];
        wall_light[2 * n + i] = caustic[1];
        wall_light[3 * n + i] = caustic[2];
    }
    unsigned face = 0;
    for (unsigned band = 0; band < 2; ++band) {
        for (unsigned i = 0; i + 1 < n; ++i, ++face) {
            const unsigned top = (2 * band) * n + i, bottom = (2 * band + 1) * n + i;
            indices[4 * face] = top;
            indices[4 * face + 1] = top + 1;
            indices[4 * face + 2] = bottom + 1;
            indices[4 * face + 3] = bottom;
            face_materials[face] = 0;
        }
    }
    const sat_mesh_t mesh = {
        .vertices = vertices, .indices = indices, .vertex_cap = 4 * MAX_NODES,
        .vertex_count = 4 * n, .face_cap = (MAX_NODES - 1) * (MAX_NODES - 1),
        .face_count = 2 * (n - 1)
    };
    const sat_scene3d_material_t material = {
        .kind = SAT_SCENE3D_INDEXED_TEXTURED, .rgb555 = SAT_RGB555(12, 16, 18),
        .texture = &tile, .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE,
        .vertex_gouraud = wall_light
    };
    const sat_scene3d_instance_t instance = {
        .mesh = &mesh, .materials = &material, .material_count = 1,
        .face_materials = face_materials, .cull_backfaces = 1
    };
    sat_example_must(sat_scene_submit_instance(
        &scene, &instance, SAT_SCENE3D_SLOT_INHERIT, projected, 0));
    grid_nodes = 0;
}

static void draw_ball(const ball_t* ball) {
    sat_fx16_t wave = 0;
    sat_example_must(sat_water_sim_sample(
        &sim, node_coord(ball->position.x), node_coord(ball->position.z), &wave));
    const sat_fx16_t surface = REST + wave;
    const sat_scene3d_material_t material = {
        .kind = SAT_SCENE3D_RGB, .rgb555 = SAT_RGB555(31, 21, 8),
        .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE
    };
    for (uint16_t f = 0; f < sphere.face_count; ++f) {
        sat_quad3_t quad;
        uint16_t colors[4];
        sat_example_must(sat_mesh_face_quad(&sphere, f, &quad));
        for (unsigned c = 0; c < 4; ++c) {
            colors[c] = sphere_light[sphere.indices[4 * f + c]];
            quad.v[c].x += ball->position.x;
            quad.v[c].y += ball->position.y;
            quad.v[c].z += ball->position.z;
        }
        sat_water_shaded_piece_t pieces[SAT_WATER_SPLIT_PIECES_MAX];
        uint8_t count;
        sat_example_must(sat_water_split_quad_shaded(&quad, colors, surface, pieces, &count));
        for (uint8_t i = 0; i < count; ++i) {
            sat_scene3d_material_t shaded = material;
            if (pieces[i].piece.submerged) {
                for (unsigned c = 0; c < 4; ++c)
                    pieces[i].color[c] = tint(pieces[i].color[c], -6, -2, 3);
            }
            shaded.vertex_gouraud = pieces[i].color;
            sat_example_must(sat_scene_submit_quad(&scene, &pieces[i].piece.quad, &shaded, 0));
        }
    }
}

static int32_t magnitude(int32_t value) { return value < 0 ? -value : value; }

/* Horizontal bands from a deep sky blue to a pale horizon stand in for the
 * original's sky: VDP2's backdrop is one color. */
static void draw_sky(void) {
    enum { BANDS = 8 };
    for (unsigned band = 0; band < BANDS; ++band) {
        sat_example_must(sat_draw_rect_screen(
            0, (int16_t)(band * HEIGHT / BANDS), WIDTH, HEIGHT / BANDS + 1,
            SAT_RGB555(7 + band, 16 + band * 2, 27)));
    }
}

static sat_fx16_t clamp(sat_fx16_t value, sat_fx16_t lo, sat_fx16_t hi) {
    return value < lo ? lo : (value > hi ? hi : value);
}

static void reset(ball_t* ball, sat_vec3_t* cursor, unsigned nodes) {
    sat_example_must(sat_water_sim_init(&sim, nodes, 256, heights, velocities));
    *ball = (ball_t){.position = {UNIT(-2), UNIT(2), UNIT(1)}};
    *cursor = (sat_vec3_t){UNIT(2), REST, UNIT(-1)};
    grid_nodes = 0;
}

static void step_ball(ball_t* ball, const sat_pad_state_t* pad, uint8_t gravity,
                      const sat_water_volume_t* volume, const sat_vec3_t* origin) {
    const sat_fx16_t speed = SAT_FX16_ONE / 8;
    const int x = !!(pad->held & SAT_PAD_RIGHT) - !!(pad->held & SAT_PAD_LEFT);
    const int z = !!(pad->held & SAT_PAD_DOWN) - !!(pad->held & SAT_PAD_UP);
    const sat_vec3_t before = ball->position;
    sat_vec3_t velocity = {x * speed, ball->velocity_y, z * speed};
    if (pad->held & SAT_PAD_Z) {
        /* Z + up / down raises or pushes the ball under the surface. */
        velocity.x = velocity.z = 0;
        velocity.y = (!!(pad->held & SAT_PAD_UP) - !!(pad->held & SAT_PAD_DOWN)) * speed;
        ball->velocity_y = velocity.y;
    }
    sat_water_sample_t sample;
    const sat_vec3_t base = {ball->position.x, ball->position.y - RADIUS, ball->position.z};
    sat_example_must(sat_water_sim_sample_volume(
        &sim, volume, origin, spacing(), &base, 2 * RADIUS, &sample));
    if (gravity) {
        velocity.y -= SAT_FX16_ONE / 36;
        sat_example_must(sat_water_volume_apply(volume, &sample, &velocity));
    } else if (!(pad->held & SAT_PAD_Z)) {
        velocity.y = 0;
    }
    ball->velocity_y = velocity.y;
    ball->position.x = clamp(ball->position.x + velocity.x, -HALF + RADIUS, HALF - RADIUS);
    ball->position.z = clamp(ball->position.z + velocity.z, -HALF + RADIUS, HALF - RADIUS);
    ball->position.y = clamp(ball->position.y + velocity.y, FLOOR + RADIUS, UNIT(8));
    if (ball->position.y == FLOOR + RADIUS && ball->velocity_y < 0) ball->velocity_y = 0;
    // A body pushing through the surface displaces water; a fully dry or fully
    // submerged one does not.
    if (sample.fraction > 0 && sample.fraction < SAT_FX16_ONE) {
        const int64_t moved = (int64_t)magnitude(ball->position.x - before.x) +
            magnitude(ball->position.y - before.y) + magnitude(ball->position.z - before.z);
        const sat_fx16_t amount = (sat_fx16_t)(moved > UNIT(1) / 2 ? UNIT(3) / 2 : 3 * moved);
        const sat_fx16_t radius = (sim.size > 9 ? 3 : 2) * SAT_FX16_ONE;
        sat_example_must(sat_water_sim_displace(
            &sim, node_coord(before.x), node_coord(before.z),
            node_coord(ball->position.x), node_coord(ball->position.z), radius, amount));
    }
}

int main(void) {
    const sat_video_config_t video = {WIDTH, HEIGHT, SAT_VIDEO_AUTO, 0};
    sat_camera3d_t camera;
    sat_water_volume_t volume = {
        .min_x = -HALF, .max_x = HALF, .min_z = -HALF, .max_z = HALF,
        .bottom = FLOOR, .surface = REST,
        .buoyancy = SAT_FX16_ONE / 26, .drag = SAT_FX16_ONE / 5
    };
    const sat_vec3_t origin = {-HALF, 0, -HALF};
    ball_t ball;
    sat_vec3_t cursor;
    uint32_t tick = 0;
    int32_t orbit = 20;
    unsigned nodes = 9;
    uint8_t gravity = 1, paused = 0;
    uint32_t script = 0;

    sat_example_must(sat_init(&video));
    sat_example_must(sat_vdp1_set_frame_mode(SAT_VDP1_FRAME_ON_SUBMIT));
    sat_example_must(sat_vdp2_back_color_set(SAT_RGB555(14, 22, 28)));
    sat_example_must(sat_set_clear_color(SAT_RGB555(14, 22, 28)));
    sat_example_must(sat_scene_init(&scene, faces, keys, order, FACE_CAPACITY));
    make_tile();
    make_ball();
    reset(&ball, &cursor, nodes);
    water_pool_telemetry.tick_frequency = sat_time_frequency_hz();

    for (;;) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_begin_frame());
        sat_example_must(sat_pad_poll(&pad));
        if (pad.pressed & SAT_PAD_START) {
            nodes = 9;
            gravity = 1;
            paused = 0;
            orbit = 20;
            reset(&ball, &cursor, nodes);
            tick = 0;
        } else {
            if (pad.pressed & SAT_PAD_B) gravity ^= 1u;
            if ((pad.pressed & SAT_PAD_Y) && (pad.held & SAT_PAD_Z)) {
                /* Z + Y replays one ripple from a fresh pool and pauses after
                 * a fixed number of simulation ticks, however fast the frames run. */
                reset(&ball, &cursor, nodes);
                tick = 0;
                paused = 0;
                script = 1;
                pad.held = 0;
            } else if (pad.pressed & SAT_PAD_Y) {
                paused ^= 1u;
            }
            if (pad.pressed & SAT_PAD_C) {
                nodes = nodes == 9 ? 13 : (nodes == 13 ? 17 : 9);
                reset(&ball, &cursor, nodes);
            }
            if (pad.held & SAT_PAD_L) orbit = (orbit + 358) % 360;
            if (pad.held & SAT_PAD_R) orbit = (orbit + 2) % 360;
            // A replay ignores the pad, so held buttons cannot make it depend on frame pacing.
            if (script) pad.held = pad.pressed = 0;
            if (!paused) {
                if (pad.held & SAT_PAD_X) {
                    // X held: the D-pad moves the ripple cursor instead of the ball.
                    cursor.x = clamp(cursor.x + (!!(pad.held & SAT_PAD_RIGHT) -
                                                 !!(pad.held & SAT_PAD_LEFT)) * (SAT_FX16_ONE / 8),
                                     -HALF + SAT_FX16_ONE, HALF - SAT_FX16_ONE);
                    cursor.z = clamp(cursor.z + (!!(pad.held & SAT_PAD_DOWN) -
                                                 !!(pad.held & SAT_PAD_UP)) * (SAT_FX16_ONE / 8),
                                     -HALF + SAT_FX16_ONE, HALF - SAT_FX16_ONE);
                    pad.held &= (uint16_t)~(SAT_PAD_UP | SAT_PAD_DOWN | SAT_PAD_LEFT | SAT_PAD_RIGHT);
                }
                if ((pad.pressed & SAT_PAD_A) || ((pad.held & SAT_PAD_A) && (tick & 3u) == 0)) {
                    sat_example_must(sat_water_sim_bump(
                        &sim, node_coord(cursor.x), node_coord(cursor.z),
                        (sim.size > 9 ? 3 : 2) * SAT_FX16_ONE, 4 * SAT_FX16_ONE));
                }
                if (script == 6) {
                    sat_example_must(sat_water_sim_bump(
                        &sim, node_coord(cursor.x), node_coord(cursor.z),
                        (sim.size > 9 ? 3 : 2) * SAT_FX16_ONE, 4 * SAT_FX16_ONE));
                }
                step_ball(&ball, &pad, gravity, &volume, &origin);
                sat_water_sim_step(&sim);
                if (script && ++script > SCRIPT_TICKS) {
                    script = 0;
                    paused = 1;
                }
            }
        }

        const sat_vec3_t target = {0, UNIT(-2), 0};
        const sat_fx16_t yaw = UNIT(orbit);
        const sat_vec3_t eye = {
            target.x + sat_fx16_mul(UNIT(12), sat_sin_deg(yaw)), target.y + UNIT(7),
            target.z + sat_fx16_mul(UNIT(12), sat_cos_deg(yaw))
        };
        const sat_vec3_t up = {0, SAT_FX16_ONE, 0};
        sat_example_must(sat_camera3d_init(&camera, &eye, &target, &up, UNIT(51),
            sat_fx16_div(UNIT(WIDTH), UNIT(HEIGHT)), SAT_FX16_ONE / 2, UNIT(60)));

        draw_sky();
        const uint64_t prepare = sat_time_ticks();
        sat_example_must(sat_scene_begin(&scene, &camera, SAT_FX16_ONE / 2, WIDTH, HEIGHT,
                                        OVERLAY_COMMANDS));
        prepare_indices(sim.size);
        draw_floor();
        for (unsigned side = 0; side < 4; ++side) {
            draw_wall(side);
            prepare_indices(sim.size);
        }
        draw_ball(&ball);
        draw_surface();
        const uint64_t flush = sat_time_ticks();
        sat_example_must(sat_scene_flush(&scene));
        sat_example_must(sat_app_frame_end());

        const int64_t volume_sum = sat_water_sim_volume(&sim);
        water_pool_telemetry.tick = tick;
        water_pool_telemetry.nodes = sim.size;
        water_pool_telemetry.volume_high = (uint32_t)((uint64_t)volume_sum >> 32);
        water_pool_telemetry.volume_low = (uint32_t)volume_sum;
        water_pool_telemetry.preparation_ticks = (uint32_t)(flush - prepare);
        water_pool_telemetry.flush_ticks = (uint32_t)(sat_time_ticks() - flush);
        water_pool_telemetry.ball_x = ball.position.x;
        water_pool_telemetry.ball_y = ball.position.y;
        water_pool_telemetry.ball_z = ball.position.z;
        water_pool_telemetry.gravity = gravity;
        water_pool_telemetry.paused = paused;
        water_pool_telemetry.script = script;
        if (!paused) ++tick;
    }
}
