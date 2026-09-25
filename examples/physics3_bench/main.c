/* physics3_bench - SH-2 cost of sat_physics3_world_step.
 *
 * Builds one stage (a 16x16-quad floor mesh, 16 static boxes and a grid of
 * dynamic spheres) and times STEPS world steps per configuration with
 * sat_time_ms: linear collider scan vs the opt-in BVH, 8 vs 32 spheres, the
 * floor registered plain or with its mesh grid, then with mesh-face CCD and
 * sphere/sphere contacts, all measured with the spheres resting on the
 * floor. Results land in
 * g_bench_results (read by the harness from Work RAM) and the screen turns
 * green once every configuration stepped without error.
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/physics3_world.h"
#include "saturn/time.h"

#define FX(n) ((sat_fx16_t)((n) * 65536))
#define GRID 16u
#define VERTS ((GRID + 1u) * (GRID + 1u))
#define FACES (GRID * GRID)
#define BOXES 16u
#define MAX_SPHERES 32u
#define CAPACITY (1u + BOXES + MAX_SPHERES)
/* Spheres spawn in the air; the costly part is resting on the floor, so
 * WARMUP steps land them before STEPS are timed. */
#define WARMUP 16u
#define STEPS 4u

typedef struct bench_result {
    uint32_t spheres;
    uint32_t flags;        /* bit0 BVH, bit1 mesh CCD, bit2 sphere pairs, bit3 floor grid */
    uint32_t total_ms;     /* for STEPS world steps */
    uint32_t max_step_ms;  /* sat_time_ms loses 16-bit FRT wraps (~292 ms)
                            * between calls, so each step is timed alone */
    uint32_t candidates;   /* BVH candidates checked on the last step */
    uint32_t pair_contacts;
    int32_t status;
} bench_result_t;

#define CONFIGS 7u
volatile bench_result_t g_bench_results[CONFIGS];
volatile uint32_t g_bench_done;

static sat_vec3_t g_vertices[VERTS];
static uint16_t g_indices[FACES * 4u];
static sat_mesh_t g_floor = {g_vertices, g_indices, VERTS, VERTS, FACES, FACES};
static sat_physics3_actor_t g_actors[CAPACITY];
static sat_contact3_t g_contacts[FACES];
static uint16_t g_collider_indices[CAPACITY];
static sat_physics3_spatial_node_t g_nodes[2u * CAPACITY - 1u];
static uint16_t g_candidates[CAPACITY];
static uint16_t g_pair_order[CAPACITY];
/* 8-unit cells match the floor quads; a face touching cell borders is
 * listed in each, hence the entry headroom. */
static uint16_t g_grid_heads[256];
static sat_mesh3_grid_entry_t g_grid_entries[FACES * 9u];
static uint16_t g_grid_stamps[FACES];
static sat_mesh3_grid_t g_grid;

static void build_floor(void) {
    /* 128x128 units centred on the origin, upward-facing quads. */
    for (uint16_t z = 0u; z <= GRID; ++z) {
        for (uint16_t x = 0u; x <= GRID; ++x) {
            sat_vec3_t* v = &g_vertices[z * (GRID + 1u) + x];
            v->x = FX((int32_t)x * 8 - 64);
            v->y = 0;
            v->z = FX((int32_t)z * 8 - 64);
        }
    }
    for (uint16_t z = 0u; z < GRID; ++z) {
        for (uint16_t x = 0u; x < GRID; ++x) {
            uint16_t* f = &g_indices[(z * GRID + x) * 4u];
            const uint16_t a = (uint16_t)(z * (GRID + 1u) + x);
            f[0] = a;
            f[1] = (uint16_t)(a + GRID + 1u);
            f[2] = (uint16_t)(a + GRID + 2u);
            f[3] = (uint16_t)(a + 1u);
        }
    }
}

static sat_result_t run_config(uint32_t spheres, uint32_t flags, bench_result_t* out) {
    static const sat_physics3_material_t material = {SAT_FX16_ONE / 2, SAT_FX16_ONE / 4};
    const sat_vec3_t gravity = {0, -FX(1) / 32, 0};
    sat_physics3_world_t world;
    uint16_t id = 0u;
    SAT_TRY(sat_physics3_world_init(&world, g_actors, CAPACITY, gravity, 8u, 2u));
    SAT_TRY(sat_physics3_set_mesh_contacts(&world, g_contacts, FACES));
    if ((flags & 8u) != 0u) {
        SAT_TRY(sat_mesh3_grid_init(&g_grid, &g_floor, 3u, g_grid_heads, 256u,
                                    g_grid_entries, FACES * 9u, g_grid_stamps, FACES));
        SAT_TRY(sat_physics3_add_mesh_grid(&world, &g_grid, &material, &id));
    } else {
        SAT_TRY(sat_physics3_add_mesh(&world, &g_floor, &material, &id));
    }
    for (uint16_t i = 0u; i < BOXES; ++i) {
        /* Crates on a 4x4 grid across the floor. */
        const sat_aabb3_t box = {
            {FX((int32_t)(i % 4u) * 32 - 48), FX(2), FX((int32_t)(i / 4u) * 32 - 48)},
            {FX(2), FX(2), FX(2)}};
        SAT_TRY(sat_physics3_add_box(&world, SAT_PHYSICS3_STATIC_BOX, &box, &material, &id));
    }
    for (uint16_t i = 0u; i < spheres; ++i) {
        /* Rolling across the stage from a loose grid, some towards each other. */
        const sat_sphere_t sphere = {
            {FX((int32_t)(i % 8u) * 12 - 42), FX(3) + FX((int32_t)(i / 8u)), FX((int32_t)(i / 8u) * 12 - 18)},
            FX(1)};
        const sat_vec3_t velocity = {(i & 1u) ? FX(1) / 4 : -FX(1) / 4, 0, (i & 2u) ? FX(1) / 8 : -FX(1) / 8};
        SAT_TRY(sat_physics3_add_sphere(&world, &sphere, &velocity, &material, &id));
    }
    SAT_TRY(sat_physics3_set_collider_index_scratch(&world, g_collider_indices, CAPACITY));
    if ((flags & 1u) != 0u) {
        SAT_TRY(sat_physics3_set_spatial_broadphase(
            &world, g_nodes, 2u * CAPACITY - 1u, g_candidates, CAPACITY));
    }
    if ((flags & 2u) != 0u) SAT_TRY(sat_physics3_set_mesh_face_ccd(&world, 1));
    if ((flags & 4u) != 0u) SAT_TRY(sat_physics3_set_sphere_pairs(&world, g_pair_order, CAPACITY));

    for (uint32_t step = 0u; step < WARMUP; ++step) SAT_TRY(sat_physics3_world_step(&world));
    uint32_t pair_contacts = 0u;
    uint32_t total = 0u, longest = 0u;
    for (uint32_t step = 0u; step < STEPS; ++step) {
        const uint32_t start = sat_time_ms();
        SAT_TRY(sat_physics3_world_step(&world));
        const uint32_t took = sat_time_ms() - start;
        total += took;
        if (took > longest) longest = took;
        pair_contacts += world.sphere_pair_contacts;
    }
    out->spheres = spheres;
    out->flags = flags;
    out->total_ms = total;
    out->max_step_ms = longest;
    out->candidates = world.spatial_candidates_checked;
    out->pair_contacts = pair_contacts;
    return SAT_OK;
}

int main(void) {
    static const uint32_t kSpheres[CONFIGS] = {8u, 8u, 32u, 32u, 32u, 32u, 32u};
    static const uint32_t kFlags[CONFIGS] = {0u, 1u, 0u, 1u, 9u, 11u, 15u};
    if (sat_app_init_default() != SAT_OK) return 1;
    build_floor();
    uint8_t ok = 1u;
    for (uint32_t i = 0u; i < CONFIGS; ++i) {
        bench_result_t result = {0};
        result.status = run_config(kSpheres[i], kFlags[i], &result);
        if (result.status != SAT_OK) ok = 0u;
        g_bench_results[i] = result;
    }
    g_bench_done = 1u;
    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_app_frame_begin(SAT_COLOR_BLACK, ok ? SAT_COLOR_GREEN : SAT_COLOR_RED, &pad) != SAT_OK) break;
        (void)sat_app_frame_end();
    }
    return 0;
}
