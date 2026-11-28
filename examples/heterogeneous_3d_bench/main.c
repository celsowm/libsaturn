#include "saturn/saturn.h"
#include "saturn/app.h"
#include "saturn/audio.h"
#include "saturn/font.h"
#include "saturn/irq.h"
#include "saturn/skeleton3d.h"
#include "lighting.h"

enum {
    WIDTH = 320, HEIGHT = 224, ACTORS = 4, JOINTS = 4, TERRAIN = 32,
    FACES = ACTORS*JOINTS*6 + TERRAIN, VERTICES = 4*FACES,
    SCENE_MODES = 5, SCENE_FRAMES = 120, TEX_SIZE = 64, DMA_LEVEL = 1
};
#define ONE SAT_FX16_ONE
#define MAGIC SAT_FOURCC('H','3','D','1')

typedef struct {
    uint32_t total, best, worst, master, slave, dsp, dispatch, join, ports, dsp_vertices, slave_vertices;
} bench_row_t;

typedef struct {
    uint32_t mode, frames, swaps, fields, ready, worst, geometry, master, slave, dsp;
    uint32_t dma_wait, submitted, culled, latency, late, digest, active, dma_elapsed;
} scene_row_t;

volatile struct {
    uint32_t magic, status, errors, completed, frc_hz, refresh_hz, scene_count, texture_bytes;
    uint32_t live_frames, audio_refills, audio_underruns, audio_consumed, audio_playing;
    bench_row_t sweep[SIZE_COUNT][MODE_COUNT];
    scene_row_t scene[SCENE_MODES];
} g_heterogeneous_3d_bench;

static sat_scene_t scene;
static sat_scene3d_face_t faces[FACES];
static uint32_t keys[FACES];
static uint16_t order[FACES];
static sat_quad3_t quads[FACES];
static sat_ascii_font_t font;
static sat_camera3d_t camera;
static sat_skeleton_instance_t rig;
static sat_transform3d_node_t nodes[JOINTS];
static uint16_t scratch[JOINTS];
static sat_skeleton_joint_t joints[JOINTS];
static const sat_skeleton_asset_t skeleton = {joints, JOINTS, 0};
static sat_skeleton_pose_joint_t pose[JOINTS];
static sat_mat4_t world[ACTORS][JOINTS];
static sat_vdp1_texture_t tile[2];
static _Alignas(16) uint8_t pixels[TEX_SIZE*TEX_SIZE/2];
static unsigned phase, back_texture;
static uint16_t frame_start, dma_start_frc;
static uint32_t block_start_field, dma_prior;
static volatile uint32_t dma_serial;
static volatile uint16_t dma_end_frc;
static volatile struct {
    uint32_t serial, mode, phase;
    uint16_t started;
} swap_record[2];
volatile uint32_t g_heterogeneous_scene_capture;
static sat_result_t preparation_status;
static sat_audio_stream_t stream;
static int8_t pcm_ring[16384];
static int8_t pcm_period[1024];
static uint8_t stress_audio;

static sat_result_t service_audio(void) {
    const uint32_t room = sat_audio_stream_available(stream);
    if (room >= sizeof(pcm_period))
        SAT_TRY(sat_audio_stream_write(stream, pcm_period, sizeof(pcm_period)));
    return sat_audio_update();
}

static const int8_t corners[8][3] = {
    {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},
    {-1,-1,1}, {1,-1,1}, {1,1,1}, {-1,1,1}
};
static const uint8_t topology[6][4] = {
    {4,5,6,7}, {1,0,3,2}, {5,1,2,6}, {0,4,7,3}, {7,6,2,3}, {0,1,5,4}
};
static const int8_t axis[6][3] = {
    {0,0,1}, {0,0,-1}, {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}
};

static uint32_t elapsed(uint16_t start) {
    return (uint16_t)(sat_time_frc() - start);
}

static void check_at(sat_result_t result, int line) {
    if (result == SAT_OK) return;
    g_heterogeneous_3d_bench.status = (uint32_t)-result;
    sat_panic(result, __FILE__, line);
}

#define check(result) check_at((result), __LINE__)

static void benchmark(void) {
    /* Deterministic signed Q10 values. Poison output to catch unwritten slots
     * even when the previous mode computed the identical input. */
    uint32_t random = 0x71bc9a53u;
    for (unsigned i = 0; i < MAX_VERTICES; ++i)
        for (unsigned j = 0; j < 3; ++j) {
            random = random * 1664525u + 1013904223u;
            normals[i][j] = (int32_t)(random >> 21) - 1024;
        }

    for (unsigned size = 0; size < SIZE_COUNT; ++size) {
        const unsigned count = TILE << size;
        for (unsigned rep = 0; rep <= REPEATS; ++rep) {
            for (unsigned index = 0; index < MODE_COUNT; ++index) {
                const unsigned mode = (index + rep) % MODE_COUNT;
                for (unsigned i = 0; i < count; ++i) intensity[i] = INT32_MIN;
                lighting_ticks_t t;
                check(lighting_run(mode, count, &t, 0));
                g_heterogeneous_3d_bench.errors += lighting_mismatches(count);
                if (!rep) continue;
                volatile bench_row_t* row = &g_heterogeneous_3d_bench.sweep[size][mode];
                row->total += t.total;
                if (rep == 1 || t.total < row->best) row->best = t.total;
                if (t.total > row->worst) row->worst = t.total;
                row->master += t.master;
                row->slave += t.slave;
                row->dsp += t.dsp_elapsed;
                row->dispatch += t.dispatch;
                row->join += t.join;
                row->ports += t.ports;
                row->dsp_vertices = t.dsp_vertices;
                row->slave_vertices = t.slave_vertices;
            }
        }
        ++g_heterogeneous_3d_bench.completed;
    }
}

static unsigned faster(unsigned first, unsigned last) {
    const volatile bench_row_t* rows = g_heterogeneous_3d_bench.sweep[5]; /* 512 */
    unsigned fastest = first;
    for (unsigned mode = first + 1; mode <= last; ++mode)
        if (rows[mode].total < rows[fastest].total) fastest = mode;
    return fastest;
}

static void prepare_terrain(void) {
    for (unsigned i = 0; i < TERRAIN; ++i) {
        const sat_fx16_t x = ((int)(i % 8)-4)*2*ONE;
        const sat_fx16_t z = ((int)(i / 8)-3)*3*ONE;
        quads[i] = (sat_quad3_t){.v = {
            {x,0,z}, {x+2*ONE,0,z}, {x+2*ONE,0,z+3*ONE}, {x,0,z+3*ONE}
        }};
        for (unsigned k = 0; k < 4; ++k) {
            normals[4*i+k][0] = 0;
            normals[4*i+k][1] = 1024;
            normals[4*i+k][2] = 0;
        }
    }
}

static void dma_end(void* user) {
    (void)user;
    dma_end_frc = sat_time_frc();
    ++dma_serial;
}

static void animate(void) {
    for (unsigned a = 0; a < ACTORS; ++a) {
        const int walk = (int)phase*6 + (int)a*45;
        const sat_fx16_t swing = sat_sin_deg_whole(walk);
        pose[0].translation = (sat_vec3_t){
            ((int)a-2)*2*ONE+ONE/2 + sat_cos_deg_whole(walk)/3,
            ONE+sat_fx16_abs(swing)/12, -((int)a & 1)*2*ONE
        };
        for (unsigned j = 2; j < JOINTS; ++j) {
            sat_quat16_t q = {(j == 2 ? swing : -swing)/8, 0, 0, INT16_MAX};
            sat_quat16_normalize(&pose[j].rotation, &q);
        }
        check(sat_skeleton_evaluate(&rig, pose, JOINTS));
        for (unsigned j = 0; j < JOINTS; ++j) {
            check(sat_skeleton_joint_world(&rig, j, &world[a][j]));
            const sat_fx16_t* m = world[a][j].m;
            for (unsigned f = 0; f < 6; ++f) {
                const unsigned base = 4*(TERRAIN + (a*JOINTS+j)*6 + f);
                for (unsigned k = 0; k < 4; ++k)
                    for (unsigned d = 0; d < 3; ++d)
                        normals[base+k][d] = (m[4*d]*axis[f][0] +
                            m[4*d+1]*axis[f][1] + m[4*d+2]*axis[f][2])/64;
            }
        }
    }

    /* Upload this frame's floor pattern into the texture the previous list
     * is not referencing. The texture is consumed by all terrain quads. */
    for (unsigned y = 0; y < TEX_SIZE; ++y)
        for (unsigned x = 0; x < TEX_SIZE/2; ++x) {
            const unsigned shade = ((x/4 + ((y + phase)/8)) & 1) + 1;
            pixels[y*(TEX_SIZE/2)+x] = (uint8_t)(shade*0x11);
        }
}

static void prepare_geometry(void) {
    void* dst = (void*)sat_vdp1_vram_address(tile[back_texture].srca*8u, sizeof(pixels));
    preparation_status = stress_audio ? service_audio() : SAT_OK;
    if (preparation_status == SAT_OK) {
        dma_prior = dma_serial;
        dma_start_frc = sat_time_frc();
        preparation_status = sat_dma_start(DMA_LEVEL, pixels, dst, sizeof(pixels));
    }

    camera.eye.x = sat_sin_deg_whole((int)phase*3)*2;
    camera.eye.z = 7*ONE + sat_cos_deg_whole((int)phase*3);
    if (preparation_status == SAT_OK) preparation_status = sat_camera3d_update(&camera);

    const sat_vec3_t dimensions[JOINTS] = {
        {ONE/3, ONE/3, ONE/4}, {ONE/4, ONE/4, ONE/4},
        {ONE/8, ONE/3, ONE/7}, {ONE/8, ONE/3, ONE/7}
    };
    for (unsigned a = 0; a < ACTORS; ++a)
        for (unsigned j = 0; j < JOINTS; ++j) {
            sat_vec3_t posed[8];
            for (unsigned k = 0; k < 8; ++k) {
                const sat_vec4_t local = {
                    corners[k][0]*dimensions[j].x, corners[k][1]*dimensions[j].y,
                    corners[k][2]*dimensions[j].z, ONE
                };
                sat_vec4_t result;
                (void)sat_mat4_transform_vec4(&world[a][j], &local, &result);
                posed[k] = (sat_vec3_t){result.x, result.y, result.z};
            }
            for (unsigned f = 0; f < 6; ++f)
                for (unsigned k = 0; k < 4; ++k)
                    quads[TERRAIN+(a*JOINTS+j)*6+f].v[k] = posed[topology[f][k]];
        }
}

static unsigned draw_scene(void) {
    check(sat_vdp1_set_erase_transparent());
    check(sat_begin_frame());
    check(sat_scene_begin(&scene, &camera, camera.near_z, WIDTH, HEIGHT, 104));
    const uint16_t pal[ACTORS] = {
        SAT_RGB555(21,9,5), SAT_RGB555(5,18,15), SAT_RGB555(10,13,23), SAT_RGB555(19,6,17)
    };
    unsigned culled = 0;
    for (unsigned f = 0; f < FACES; ++f) {
        const sat_vec3_t p = quads[f].v[0];
        const int32_t* n = normals[4*f];
        const int32_t toward = n[0]*((camera.eye.x-p.x)/256) +
            n[1]*((camera.eye.y-p.y)/256) + n[2]*((camera.eye.z-p.z)/256);
        if (toward <= 0) {
            ++culled;
            continue;
        }
        uint16_t shades[4];
        for (unsigned k = 0; k < 4; ++k) {
            const int32_t dot = intensity[f*4+k];
            shades[k] = sat_gouraud_grey(dot > 0 ? dot/131072 : -3);
        }
        sat_scene3d_material_t m = {
            .vertex_gouraud = shades, .color_calc_slot = SAT_INDEXED_SOLID_OPAQUE
        };
        if (f < TERRAIN) {
            m.kind = SAT_SCENE3D_INDEXED_TEXTURED;
            m.texture = &tile[back_texture];
            m.rgb555 = SAT_RGB555(10,11,12);
        } else {
            m.kind = SAT_SCENE3D_RGB;
            m.rgb555 = pal[(f-TERRAIN)/(JOINTS*6)];
        }
        check(sat_scene_submit_quad(&scene, &quads[f], &m, 0));
    }
    check(sat_scene_flush(&scene));
    return culled;
}

static void swapped(uint32_t serial, void* user) {
    (void)user;
    const unsigned index = serial & 1;
    if (swap_record[index].serial != serial) return;
    const unsigned mode = swap_record[index].mode;
    ++g_heterogeneous_3d_bench.scene[mode].swaps;
    g_heterogeneous_3d_bench.scene[mode].latency += elapsed(swap_record[index].started);
    if (swap_record[index].phase == 59) g_heterogeneous_scene_capture = mode+1;
    swap_record[index].serial = 0;
}

static void line(const char* name, unsigned value, int x, int y) {
    char digits[6] = {0};
    for (unsigned i = 5; i > 0; --i) {
        digits[i-1] = (char)('0' + value%10);
        value /= 10;
    }
    check(sat_ascii_font_draw_text_screen_indexed8(&font, name, x, y, 8, 0, 0));
    check(sat_ascii_font_draw_text_screen_indexed8(&font, digits, x+88, y, 8, 0, 0));
}

static void draw_hud(unsigned mode, const lighting_ticks_t* t, unsigned wait,
                     unsigned geometry, const sat_scene_stats_t* s) {
    const volatile scene_row_t* row = &g_heterogeneous_3d_bench.scene[mode];
    check(sat_vdp1_overlay_begin());
    line("MODE", mode+1, 8, 8);
    line("MASTER", t->master, 8, 20);
    line("SLAVE", t->slave, 8, 32);
    line("DSP", t->dsp_elapsed, 8, 44);
    line("DMA TIME", wait, 8, 56);
    line("GEOMETRY", geometry, 170, 8);
    line("FRAME", elapsed(frame_start), 170, 20);
    const uint32_t fields = row->fields ? row->fields : sat_field_count()-block_start_field;
    line("FPS", fields ? row->swaps*sat_video_refresh_hz()/fields : 0, 170, 32);
    line("VERTICES", VERTICES, 170, 44);
    line("POLYGONS", s->flushed_faces, 170, 56);
    line("CULLED", s->culled_faces, 170, 68);
}

static unsigned render_frame(unsigned mode, unsigned backend, unsigned measured) {
    frame_start = sat_time_frc();
    lighting_ticks_t ticks;
    animate();
    const uint32_t pose_ticks = elapsed(frame_start);
    check(lighting_run(backend, VERTICES, &ticks, prepare_geometry));
    check(preparation_status);
    const uint16_t dma_started = sat_time_frc();
    check(sat_dma_wait(DMA_LEVEL));
    const uint32_t dma_wait = elapsed(dma_started);
    const uint16_t dma_observed = dma_serial != dma_prior ? dma_end_frc : sat_time_frc();
    const uint32_t dma_elapsed = (uint16_t)(dma_observed - dma_start_frc);

    const uint16_t draw_start = sat_time_frc();
    const unsigned backfaces = draw_scene();
    const uint32_t geometry = pose_ticks + ticks.total + elapsed(draw_start);
    sat_scene_stats_t stats;
    check(sat_scene_stats(&scene, &stats));
    stats.culled_faces += backfaces;
    draw_hud(mode, &ticks, dma_elapsed, geometry, &stats);
    const uint32_t active = elapsed(frame_start);
    /* The VDP1 can still be reading the previous texture. sat_end_frame
     * waits for its swap before publishing the newly built command bank. */
    const uint32_t serial = sat_vdp1_list_serial()+1;
    const unsigned index = serial & 1;
    swap_record[index].started = frame_start;
    swap_record[index].mode = mode;
    swap_record[index].phase = phase;
    swap_record[index].serial = measured ? serial : 0;
    check(sat_end_frame());
    check(sat_vdp1_list_serial() == serial ? SAT_OK : SAT_ERR_TIMEOUT);

    if (measured) {
        volatile scene_row_t* row = &g_heterogeneous_3d_bench.scene[mode];
        ++row->frames;
        const uint32_t ready = elapsed(frame_start);
        row->ready += ready;
        row->active += active;
        if (ready > row->worst) row->worst = ready;
        row->geometry += geometry;
        row->master += ticks.master;
        row->slave += ticks.slave;
        row->dsp += ticks.dsp_elapsed;
        row->dma_wait += dma_wait;
        row->dma_elapsed += dma_elapsed;
        row->submitted += stats.flushed_faces;
        row->culled += stats.culled_faces;
        /* Integer results from identical poses must match across all modes. */
        uint32_t hash = row->digest;
        for (unsigned i = 0; i < VERTICES; ++i) hash = hash*33u+(uint32_t)intensity[i];
        row->digest = hash;
    }
    back_texture ^= 1;
    sat_pad_state_t pad;
    check(sat_pad_poll(&pad));
    return pad.pressed;
}

int main(void) {
    g_heterogeneous_3d_bench.magic = MAGIC;
    check(sat_app_init_default());
    g_heterogeneous_3d_bench.frc_hz = sat_time_frequency_hz();
    g_heterogeneous_3d_bench.refresh_hz = sat_video_refresh_hz();
    g_heterogeneous_3d_bench.texture_bytes = sizeof(pixels);
    check(lighting_init());
    benchmark();

    check(sat_vdp1_set_frame_mode(SAT_VDP1_FRAME_ON_SUBMIT));
    check(sat_vdp1_set_swap_handler(swapped, 0));
    (void)sat_irq_set_handler(SAT_IRQ_DMA1_END, dma_end, 0);
    check(sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, 0, 0));
    uint16_t sky[HEIGHT];
    for (unsigned i = 0; i < HEIGHT; ++i)
        sky[i] = SAT_RGB555(5+i/64, 9+i/32, 17+i/32);
    check(sat_vdp2_back_screen_set_lines(0x7f800, sky, HEIGHT));
    const uint16_t lut[16] = {0, SAT_RGB555(10,11,12), SAT_RGB555(15,16,17)};
    uint16_t lut_handle;
    check(sat_vdp1_upload_lut(lut, &lut_handle));
    for (unsigned i = 0; i < 2; ++i)
        check(sat_tex_upload_lut4_pixels(&tile[i], pixels, TEX_SIZE, TEX_SIZE, lut_handle));

    joints[0].parent = SAT_TRANSFORM3D_ROOT;
    joints[1].rest_translation.y = ONE/2;
    joints[2].rest_translation = (sat_vec3_t){ONE/5, -ONE/2, 0};
    joints[3].rest_translation = (sat_vec3_t){-ONE/5, -ONE/2, 0};
    for (unsigned j = 0; j < JOINTS; ++j) sat_quat16_identity(&joints[j].rest_rotation);
    check(sat_skeleton_instance_init(&rig, &skeleton, nodes, JOINTS, scratch, JOINTS));
    check(sat_skeleton_rest_pose(&skeleton, pose, JOINTS));
    const sat_vec3_t eye = {0, 3*ONE, 7*ONE}, target = {0, ONE, 0}, up = {0, ONE, 0};
    check(sat_camera3d_init(&camera, &eye, &target, &up,
                            56*ONE, WIDTH*ONE/HEIGHT, ONE/8, 30*ONE));
    check(sat_scene_init(&scene, faces, keys, order, FACES));
    prepare_terrain();

    const unsigned backends[] = {faster(MASTER_C, MASTER_MAC), faster(DUAL_C, DUAL_MAC),
                                 faster(TRIPLE_EIGHTH, TRIPLE_THREE_EIGHTHS),
                                 DUAL_SLAVE_WEIGHTED, TRIPLE_SLAVE_WEIGHTED};
    (void)render_frame(0, backends[0], 0); /* Prime the manual-swap pipeline. */
    for (unsigned mode = 0; mode < SCENE_MODES; ++mode) {
        sat_vdp1_frame_stats_t before, after;
        check(sat_wait_vblank());
        check(sat_vdp1_frame_stats(&before));
        block_start_field = sat_field_count();
        g_heterogeneous_3d_bench.scene[mode].mode = backends[mode];
        for (phase = 0; phase < SCENE_FRAMES; ++phase)
            (void)render_frame(mode, backends[mode], 1);
        check(sat_wait_vblank());
        check(sat_vdp1_frame_stats(&after));
        g_heterogeneous_3d_bench.scene[mode].fields = sat_field_count()-block_start_field;
        g_heterogeneous_3d_bench.scene[mode].late = after.late_swaps-before.late_swaps;
        ++g_heterogeneous_3d_bench.scene_count;
    }
    const volatile scene_row_t* rows = g_heterogeneous_3d_bench.scene;
    const unsigned dual = rows[3].ready < rows[1].ready ? 3 : 1;
    const unsigned triple = rows[4].ready < rows[2].ready ? 4 : 2;
    check(sat_audio_init());
    const sat_audio_spec_t audio = {22050, sizeof(pcm_ring), 1, SAT_AUDIO_PCM_S8, 0};
    check(sat_audio_stream_open(&stream, &audio, pcm_ring, sizeof(pcm_ring)));
    for (unsigned i = 0; i < sizeof(pcm_period); ++i)
        pcm_period[i] = (int8_t)((int)(i % 128) / 4 - 16);
    for (unsigned i = 0; i < sizeof(pcm_ring); i += sizeof(pcm_period))
        check(sat_audio_stream_write(stream, pcm_period, sizeof(pcm_period)));
    stress_audio = 1;
    unsigned mode = dual;
    unsigned manual = 0;
    for (;;) {
        const unsigned buttons = render_frame(mode, backends[mode], 0);
        ++g_heterogeneous_3d_bench.live_frames;
        sat_audio_stream_stats_t stats;
        check(sat_audio_stream_stats(stream, &stats));
        g_heterogeneous_3d_bench.audio_refills = stats.refill_count;
        g_heterogeneous_3d_bench.audio_underruns = stats.underrun_count;
        g_heterogeneous_3d_bench.audio_consumed = stats.consumed_frames;
        g_heterogeneous_3d_bench.audio_playing = stats.playing;
        if (buttons & (SAT_PAD_A | SAT_PAD_B | SAT_PAD_C)) manual = 1;
        if (buttons & SAT_PAD_A) mode = 0;
        if (buttons & SAT_PAD_B) mode = dual;
        if (buttons & SAT_PAD_C) mode = triple;
        phase = (phase + 1) % SCENE_FRAMES;
        if (!manual && phase == 0) mode = mode == dual ? triple : dual;
    }
}
