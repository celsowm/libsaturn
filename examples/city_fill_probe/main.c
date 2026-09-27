/* city_fill_probe: measures what a city_walk frame actually costs on VDP1.
 *
 * The city_walk plan sizes every LOD ring off one number -- how many solid
 * faces fit in a 20 fps frame -- and dino_demo's 15-18 fps at ~1670 commands
 * does NOT transfer: those were small textured triangles, while a city draws
 * few large flat polygons and the painter overdraws with no Z-buffer. So this
 * sweeps face count at fixed screen coverage (isolating per-command cost) and
 * coverage at fixed face count (isolating per-pixel fill), for BOTH material
 * kinds, because an INDEXED_SOLID 8x8 sprite stretched over a wall is a
 * different hardware path from an RGB polygon.
 *
 * Frames are measured with sat_frame_count() deltas, never a wall clock: it
 * keeps counting while the program is busy, so a frame that takes three
 * VBlanks advances it by three. g_fill holds what the harness reads.
 */
#include <stdint.h>
#include "saturn/color.h"
#include "saturn/core.h"
#include "saturn/font.h"
#include "saturn/math3d.h"
#include "saturn/scene.h"
#include "saturn/scene3d.h"
#include "saturn/scene3d_material_pool.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/video.h"

#define FILL_MAGIC 0x46494C32u /* "FIL2" */
#define PROBE_PHASES 20u
/* Quads per shared-vertex strip. A strip of R quads uses 2*(R+1) vertices,
 * so 16 gives ~2.1 vertices per face -- the ratio the chunker's LOD0 budget
 * assumes (512 vertices, 256 faces), and what welded facade geometry looks
 * like in practice. */
#define STRIP_QUADS 16u
#define HIST_BUCKETS 8u
#define PROBE_FRAMES 48u /* measured frames per phase */
#define PROBE_WARMUP 4u  /* discarded: cold caches and first VRAM uploads */
#define FACE_CAP 1240u
#define SCREEN_W 320u
#define SCREEN_H 224u
/* (SCREEN_H/2) / tan(fov_y/2) with fov_y = 55 degrees, rounded. Fixed here so
 * the probe needs no trig and the host can reproduce the geometry exactly. */
#define FOCAL_PX 215
#define PROBE_DEPTH 24       /* world units from the eye to the quad plane */
#define PROBE_DEPTH_JITTER 4 /* +/- spread so painter keys actually differ */

/* 0 = RGB quads one at a time, 1 = INDEXED_SOLID quads one at a time,
 * 2 = INDEXED_SOLID through sat_scene_submit_instance over a shared-vertex
 * mesh. Kind 2 is the only path city_walk actually uses: it projects one
 * vertex per ~2 faces instead of four per face, so its cost per face is the
 * number the LOD budget must be built on. */
#define KIND_RGB_QUAD 0u
#define KIND_INDEXED_QUAD 1u
#define KIND_INDEXED_INSTANCE 2u

typedef struct fill_phase_desc {
    uint16_t kind;
    uint16_t faces;
    uint16_t overdraw_x10; /* target total coverage, in tenths of a screen */
} fill_phase_desc_t;

/* Set A holds coverage at 2.5 screens and sweeps face count: any slope there
 * is per-command overhead. Set B pins 900 faces and sweeps coverage: that
 * slope is raw fill rate. The plan's own proposal (900 faces, 2.5 screens) is
 * repeated last for both kinds, so a run that dies early still leaves the
 * headline numbers in the dump. */
static const fill_phase_desc_t PHASES[PROBE_PHASES] = {
    {KIND_RGB_QUAD, 300u, 25u},
    {KIND_RGB_QUAD, 600u, 25u},
    {KIND_RGB_QUAD, 900u, 25u},
    {KIND_RGB_QUAD, 1200u, 25u},
    {KIND_RGB_QUAD, 900u, 10u},
    {KIND_RGB_QUAD, 900u, 40u},
    {KIND_INDEXED_QUAD, 300u, 25u},
    {KIND_INDEXED_QUAD, 600u, 25u},
    {KIND_INDEXED_QUAD, 900u, 25u},
    {KIND_INDEXED_QUAD, 1200u, 25u},
    {KIND_INDEXED_QUAD, 900u, 10u},
    {KIND_INDEXED_QUAD, 900u, 40u},
    /* The path the demo uses, swept the same way. */
    {KIND_INDEXED_INSTANCE, 300u, 25u},
    {KIND_INDEXED_INSTANCE, 600u, 25u},
    {KIND_INDEXED_INSTANCE, 900u, 25u},
    {KIND_INDEXED_INSTANCE, 1200u, 25u},
    {KIND_INDEXED_INSTANCE, 480u, 25u},
    {KIND_INDEXED_INSTANCE, 720u, 25u},
    {KIND_INDEXED_INSTANCE, 900u, 10u},
    {KIND_INDEXED_INSTANCE, 900u, 40u},
};

typedef struct fill_results {
    uint32_t magic;
    uint32_t phase_count;
    uint32_t width;
    uint32_t height;
    uint32_t ntsc_timing;
    /* Per phase, in PHASES order. */
    uint32_t kind[PROBE_PHASES];
    uint32_t faces[PROBE_PHASES];
    uint32_t overdraw_x10[PROBE_PHASES];
    uint32_t quad_px[PROBE_PHASES];        /* edge of one quad, in pixels */
    uint32_t frames_elapsed[PROBE_PHASES]; /* sat_frame_count delta */
    uint32_t frames_rendered[PROBE_PHASES];
    uint32_t submitted[PROBE_PHASES];
    uint32_t flushed[PROBE_PHASES];
    uint32_t world_commands[PROBE_PHASES];
    uint32_t first_error[PROBE_PHASES];
    uint32_t vertices[PROBE_PHASES];
    /* Per-frame VBlank cost, bucketed 1..7 and 8-or-more. A frame costs a
     * whole number of VBlanks, so an average hides exactly the stalls that
     * make a demo feel bad; the histogram does not. */
    uint32_t hist[PROBE_PHASES][HIST_BUCKETS];
    uint32_t done;
} fill_results_t;

volatile fill_results_t g_fill;

/* Face records are ~100 bytes each: 1240 of them is 124 KB, far past .bss
 * comfort, so the scene storage lives in Work RAM-L like every other big
 * example array. */
#define PROBE_WRAM_L __attribute__((section(".wram_l")))
static PROBE_WRAM_L sat_scene3d_face_t g_faces[FACE_CAP];
static PROBE_WRAM_L uint32_t g_keys[FACE_CAP];
static PROBE_WRAM_L uint16_t g_order[FACE_CAP];
/* Prebuilt geometry, so the measured window submits and never computes. */
static PROBE_WRAM_L sat_quad3_t g_quads[FACE_CAP];
static uint32_t g_quad_count;
static sat_scene3d_material_t g_material;

/* Shared-vertex mesh for the instance path, plus the scratch the projector
 * needs. 2*(STRIP_QUADS+1) vertices per strip of STRIP_QUADS faces. */
#define MESH_VERTEX_CAP ((FACE_CAP / STRIP_QUADS + 1u) * 2u * (STRIP_QUADS + 1u))
static PROBE_WRAM_L sat_vec3_t g_mesh_vertices[MESH_VERTEX_CAP];
static PROBE_WRAM_L uint16_t g_mesh_indices[FACE_CAP * 4u];
static PROBE_WRAM_L sat_projected_vertex_t g_screen_scratch[MESH_VERTEX_CAP];
static PROBE_WRAM_L uint16_t g_face_materials[FACE_CAP];
static sat_mesh_t g_mesh;
static sat_scene3d_instance_t g_instance;
static uint8_t g_use_instance;

static sat_scene_t g_scene;
static sat_camera3d_t g_camera;
static sat_ascii_font_t g_font;
static sat_scene3d_solid_pool_t g_pool;
static sat_scene3d_material_t g_pool_materials[4];
static sat_vdp1_texture_t g_pool_textures[4];
static uint16_t g_pool_colors[4];
static uint8_t g_pool_pixels[64];

static void halt(void) {
    for (;;) {
    }
}

static uint32_t isqrt32(uint32_t value) {
    uint32_t remainder = 0u;
    uint32_t root = 0u;
    int i;
    for (i = 0; i < 16; ++i) {
        root <<= 1;
        remainder = (remainder << 2) | (value >> 30);
        value <<= 2;
        if (root < remainder) {
            remainder -= root | 1u;
            root += 2u;
        }
    }
    return root >> 1;
}

/* Edge of one quad in pixels so that `faces` of them cover `overdraw_x10`
 * tenths of the screen. Coverage is what VDP1 fill rate is paid per, so the
 * sweep holds it fixed rather than holding the quad size fixed. */
static uint32_t quad_edge_px(uint16_t faces, uint16_t overdraw_x10) {
    const uint32_t total =
        ((uint32_t)SCREEN_W * (uint32_t)SCREEN_H * (uint32_t)overdraw_x10) / 10u;
    const uint32_t area = total / (faces != 0u ? (uint32_t)faces : 1u);
    const uint32_t px = isqrt32(area);
    return px < 2u ? 2u : px;
}

/* The quad grid is built ONCE per phase, outside the measured window. The
 * placement needs two 64-bit divisions per quad, and on SH-2 that costs more
 * than the renderer does -- a first version of this probe did it inside the
 * frame loop and "measured" a cost perfectly linear in face count and totally
 * independent of screen coverage, which was this arithmetic, not VDP1. */
static void build_phase(uint16_t phase) {
    const fill_phase_desc_t* desc = &PHASES[phase];
    const uint32_t px = quad_edge_px(desc->faces, desc->overdraw_x10);
    /* half world size = (pixels/2) * depth / focal, in 16.16 */
    const sat_fx16_t half =
        (sat_fx16_t)(((int64_t)px * PROBE_DEPTH << 16) / (2 * FOCAL_PX));
    /* Visible half-extent of the plane at PROBE_DEPTH, so the grid fills the
     * screen exactly and the overlap producing the overdraw is real. */
    const int32_t half_h =
        (int32_t)(((int64_t)PROBE_DEPTH * (int64_t)(SCREEN_H / 2u) << 16) / FOCAL_PX);
    const int32_t half_w = (int32_t)(((int64_t)half_h * SCREEN_W) / SCREEN_H);
    uint32_t grid = isqrt32(desc->faces);
    uint32_t i;

    if (grid * grid < (uint32_t)desc->faces) {
        ++grid;
    }
    if (grid == 0u) {
        grid = 1u;
    }

    if (desc->kind == 0u) {
        g_material.kind = SAT_SCENE3D_RGB;
        g_material.rgb555 = SAT_RGB555(20, 24, 16);
        g_material.texture = 0;
        g_material.tiled = 0;
        g_material.color_calc_slot = 255u;
        g_material.vertex_gouraud = 0;
    } else {
        g_material = g_pool.materials[0];
    }

    g_quad_count = desc->faces;
    for (i = 0u; i < (uint32_t)desc->faces; ++i) {
        const uint32_t gx = i % grid;
        const uint32_t gy = i / grid;
        /* Centre of cell (gx,gy) of a grid x grid tiling of the visible plane. */
        const int32_t cx =
            -half_w + (int32_t)(((int64_t)(2 * (int64_t)gx + 1) * half_w) / (int32_t)grid);
        const int32_t cy =
            -half_h + (int32_t)(((int64_t)(2 * (int64_t)gy + 1) * half_h) / (int32_t)grid);
        /* Deterministic depth spread: equal depths would make the painter's
         * tie-breaking, not its sort, the thing being measured. */
        const sat_fx16_t dz = sat_fx16_from_int(
            PROBE_DEPTH - PROBE_DEPTH_JITTER +
            (int32_t)((i * 7u) % (2u * PROBE_DEPTH_JITTER + 1u)));
        sat_quad3_t* quad = &g_quads[i];
        quad->v[0].x = cx - half; quad->v[0].y = cy + half; quad->v[0].z = dz;
        quad->v[1].x = cx + half; quad->v[1].y = cy + half; quad->v[1].z = dz;
        quad->v[2].x = cx + half; quad->v[2].y = cy - half; quad->v[2].z = dz;
        quad->v[3].x = cx - half; quad->v[3].y = cy - half; quad->v[3].z = dz;
    }
    g_fill.quad_px[phase] = px;

    /* The instance path draws the SAME quads, re-expressed as shared-vertex
     * strips: identical pixels on screen, so the two kinds are comparable. */
    g_use_instance = (desc->kind == KIND_INDEXED_INSTANCE) ? 1u : 0u;
    if (g_use_instance == 0u) {
        g_fill.vertices[phase] = 4u * (uint32_t)desc->faces;
        return;
    }
    (void)sat_mesh_init(&g_mesh, g_mesh_vertices, (uint16_t)MESH_VERTEX_CAP,
                        g_mesh_indices, FACE_CAP);
    for (i = 0u; i < (uint32_t)desc->faces;) {
        /* One strip: quads side by side sharing their vertical edges, so the
         * whole run costs 2 new vertices per face instead of 4. */
        const uint32_t run =
            ((uint32_t)desc->faces - i) < STRIP_QUADS ? ((uint32_t)desc->faces - i) : STRIP_QUADS;
        uint16_t top_prev = 0u;
        uint16_t bottom_prev = 0u;
        uint32_t k;
        (void)sat_mesh_add_vertex(&g_mesh, g_quads[i].v[0].x, g_quads[i].v[0].y,
                                  g_quads[i].v[0].z, &top_prev);
        (void)sat_mesh_add_vertex(&g_mesh, g_quads[i].v[3].x, g_quads[i].v[3].y,
                                  g_quads[i].v[3].z, &bottom_prev);
        for (k = 0u; k < run; ++k) {
            const sat_quad3_t* q = &g_quads[i + k];
            uint16_t top_next = 0u;
            uint16_t bottom_next = 0u;
            if (sat_mesh_add_vertex(&g_mesh, q->v[1].x, q->v[1].y, q->v[1].z,
                                    &top_next) != SAT_OK) {
                break;
            }
            if (sat_mesh_add_vertex(&g_mesh, q->v[2].x, q->v[2].y, q->v[2].z,
                                    &bottom_next) != SAT_OK) {
                break;
            }
            if (sat_mesh_add_face(&g_mesh, top_prev, top_next, bottom_next,
                                  bottom_prev) != SAT_OK) {
                break;
            }
            top_prev = top_next;
            bottom_prev = bottom_next;
        }
        i += run;
    }
    for (i = 0u; i < (uint32_t)g_mesh.face_count; ++i) {
        g_face_materials[i] = 0u; /* the one registered pool colour */
    }
    g_instance.mesh = &g_mesh;
    g_instance.materials = g_pool.materials;
    g_instance.material_count = g_pool.count;
    g_instance.face_materials = g_face_materials;
    g_instance.world = 0; /* vertices are already world space */
    g_instance.pass = 0u;
    /* Backface culling OFF: the sweep must pay for every face it names, and a
     * strip that happened to face away would quietly cost nothing. */
    g_instance.cull_backfaces = 0u;
    g_fill.vertices[phase] = g_mesh.vertex_count;
}

/* The measured work: nothing here but the renderer. */
static void submit_phase(void) {
    uint32_t i;
    if (g_use_instance != 0u) {
        (void)sat_scene_submit_instance(&g_scene, &g_instance, 255u,
                                        g_screen_scratch, 0);
        return;
    }
    for (i = 0u; i < g_quad_count; ++i) {
        if (sat_scene_submit_quad(&g_scene, &g_quads[i], &g_material, 0u) != SAT_OK) {
            break;
        }
    }
}

int main(void) {
    sat_video_config_t video;
    sat_vec3_t eye;
    sat_vec3_t target;
    sat_vec3_t up;
    uint16_t index = 0u;
    uint16_t phase;

    video.width = SCREEN_W;
    video.height = SCREEN_H;
    video.ntsc = SAT_VIDEO_AUTO;
    video.reserved = 0u;
    if (sat_init(&video) != SAT_OK) {
        halt();
    }

    g_fill.magic = FILL_MAGIC;
    g_fill.phase_count = PROBE_PHASES;
    g_fill.width = SCREEN_W;
    g_fill.height = SCREEN_H;
    g_fill.ntsc_timing = sat_video_is_ntsc_timing();

    /* Palette index 7 stays clear of the solid pool, which fills upward from
     * index 1 (index 0 is the INDEX8 transparent code). */
    (void)sat_ascii_font_init_8x8_indexed8(&g_font, SAT_COLOR_WHITE,
                                           SAT_COLOR_BLACK, 7u);
    (void)sat_vdp1_set_erase_transparent();
    (void)sat_vdp2_back_color_set(SAT_RGB555(3, 6, 11));

    /* One solid colour is enough: the indexed-sprite path is what costs here,
     * not how many distinct colours the pool holds. */
    if (sat_scene3d_solid_pool_init(&g_pool, g_pool_materials, g_pool_textures,
                                    g_pool_colors, g_pool_pixels, 4u, 0u) != SAT_OK) {
        halt();
    }
    if (sat_scene3d_solid_pool_register(&g_pool, SAT_RGB555(20, 24, 16), &index) != SAT_OK) {
        halt();
    }
    if (sat_scene3d_solid_pool_upload_palette(&g_pool) != SAT_OK) {
        halt();
    }

    eye.x = 0; eye.y = 0; eye.z = 0;
    target.x = 0; target.y = 0; target.z = sat_fx16_from_int(1);
    up.x = 0; up.y = SAT_FX16_ONE; up.z = 0;
    if (sat_camera3d_init(&g_camera, &eye, &target, &up, sat_fx16_from_int(55),
                          (sat_fx16_t)(((int64_t)SCREEN_W << 16) / SCREEN_H),
                          SAT_FX16_ONE / 2, sat_fx16_from_int(200)) != SAT_OK) {
        halt();
    }
    if (sat_camera3d_update(&g_camera) != SAT_OK) {
        halt();
    }
    if (sat_scene_init(&g_scene, g_faces, g_keys, g_order, FACE_CAP) != SAT_OK) {
        halt();
    }

    for (phase = 0u; phase < PROBE_PHASES; ++phase) {
        uint32_t start = 0u;
        uint32_t previous;
        uint32_t frame;
        g_fill.kind[phase] = PHASES[phase].kind;
        g_fill.faces[phase] = PHASES[phase].faces;
        g_fill.overdraw_x10[phase] = PHASES[phase].overdraw_x10;
        build_phase(phase);
        /* Cost is sampled at the SAME point in consecutive iterations -- the
         * top, before the VBlank wait. Sampling between begin_frame and
         * end_frame instead measures only the render window, which always
         * fits inside one VBlank and reports a flat 1 for every workload. */
        previous = sat_frame_count();
        for (frame = 0u; frame < PROBE_WARMUP + PROBE_FRAMES; ++frame) {
            sat_scene_stats_t stats;
            const uint32_t top = sat_frame_count();
            uint32_t cost = top - previous;
            previous = top;
            if (frame == PROBE_WARMUP) {
                start = top;
            }
            if (frame > PROBE_WARMUP) {
                /* frame == PROBE_WARMUP measures the warm-up boundary, not a
                 * phase frame, so it is not bucketed. */
                if (cost < 1u) {
                    cost = 1u;
                }
                if (cost > HIST_BUCKETS) {
                    cost = HIST_BUCKETS;
                }
                ++g_fill.hist[phase][cost - 1u];
            }
            (void)sat_wait_vblank();
            (void)sat_begin_frame();
            (void)sat_scene_begin(&g_scene, &g_camera, SAT_FX16_ONE / 2, SCREEN_W,
                                  SCREEN_H, 0u);
            submit_phase();
            (void)sat_scene_flush(&g_scene);
            (void)sat_end_frame();
            if (sat_scene_stats(&g_scene, &stats) == SAT_OK) {
                g_fill.submitted[phase] = stats.submitted_faces;
                g_fill.flushed[phase] = stats.flushed_faces;
                g_fill.world_commands[phase] = stats.world_commands;
                g_fill.first_error[phase] = (uint32_t)(-stats.result);
            }
            if (frame >= PROBE_WARMUP) {
                g_fill.frames_rendered[phase] = frame - PROBE_WARMUP + 1u;
                g_fill.frames_elapsed[phase] = sat_frame_count() - start;
            }
        }
    }

    g_fill.done = 1u;
    /* Hold the last phase on screen so a human run says something without
     * reading the dump. */
    for (;;) {
        (void)sat_wait_vblank();
        (void)sat_begin_frame();
        (void)sat_ascii_font_draw_text_screen_indexed8(
            &g_font, "CITY FILL PROBE DONE", 8, 8, 8, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(
            &g_font, "FACES ", g_fill.faces[PROBE_PHASES - 1u], 8, 32, 8, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(
            &g_font, "VBLANK/48F ", g_fill.frames_elapsed[PROBE_PHASES - 1u], 8, 48, 8, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(
            &g_font, "CMDS ", g_fill.world_commands[PROBE_PHASES - 1u], 8, 64, 8, 0u, 0u);
        (void)sat_end_frame();
    }
    return 0;
}
