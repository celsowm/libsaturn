/* bullet_hell_stress - a vertical shooter in TATE on the hi-res screen, built to find out how many
 * enemy bullets a Saturn really draws at 60 fps, and to prove the library pieces a DoDonPachi-class
 * port needs:
 *
 *   - 640 x 224 hi-res, the 224 x 448 playfield turned a quarter (saturn/tate.h, bh_view.h) with the
 *     art rotated offline, so every sprite is an ordinary VDP1 sprite and a thousand bullets cost two
 *     adds each; the 96-pixel panels left and right of the 448-pixel field carry the HUD
 *   - sprites through sat_sprite_batch_draw from a SATL atlas (sat_sprite_atlas): INDEX8 frames on one
 *     shared palette, residency groups in VDP1 VRAM (common art plus one enemy group per stage)
 *   - a VDP2 bitmap background scrolled in the VBlank, masked to the field by a window
 *   - stage transitions without a stall: while a stage plays the next one's enemy group and
 *     background are read from the disc in 16 KiB segments (bh_stream.c) next to the streamed music,
 *     which never underruns (docs/SCSP_AUDIO_STREAMING_GUIDE.md)
 *   - a deterministic simulation (bh_sim.c, host-tested) and a benchmark that holds exactly
 *     128 / 256 / 512 / 1024 enemy bullets on screen, all collision-tested every frame
 *
 * Controls: D-pad move, A shot, B bomb, C focus (slow), X run the benchmark, Y jump to the end of the
 * stage (the stage change is then a couple of seconds away), START hand control back to the autopilot.
 * Nothing pressed: the autopilot plays (and dodges), which is what the harness records.
 *
 * g_bh_telemetry holds what harness/tests/test_bullet_hell.py reads. */
#include <stdint.h>
#include "saturn/audio.h"
#include "saturn/cd_block.h"
#include "saturn/cd_mount.h"
#include "saturn/cdfs.h"
#include "saturn/color.h"
#include "saturn/core.h"
#include "saturn/input.h"
#include "saturn/sprite_atlas.h"
#include "saturn/sprite_batch.h"
#include "saturn/tate.h"
#include "saturn/time.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/vdp2_compose.h"
#include "saturn/vdp2_layers.h"
#include "saturn/video.h"

#include "bh_render.h"
#include "bh_sim.h"
#include "bh_stream.h"
#include "bh_view.h"
#include "bullet_hell_stress/bh_music.h"
#include "bullet_hell_stress/shmup_atlas_ids.h"

#define SCREEN_W 640u
#define SCREEN_H 224u
#define SPRITE_BANK 1u                  /* CRAM words 256..511: the atlas palette */
#define BG_PRIORITY 1u
#define SPRITE_PRIORITY 5u
#define BG_BANK_BYTES 0x20000u          /* one 512 x 256 8 bpp bitmap per VDP2 VRAM bank (A0, A1) */
#define BENCH_FRAMES 300u
#define PREFETCH_DELAY 90u              /* frames after a stage change before the next one is read */
#define GROUP_COMMON 0u
#define ATLAS_SLOT_COMMON 0u            /* slots 1 and 2 alternate between the current and next stage */

#ifdef BH_AUTOBENCH
#define AUTOBENCH_START 180u
#endif
/* BH_BENCH_LOOP=k (1..4) repeats the benchmark of kBenchSizes[k-1] forever, to read a steady frame
 * rate off the HUD of an emulator that has no telemetry. */

#ifdef BH_AUTOBENCH
#define BH_HUD_TIMING 1u
#else
#define BH_HUD_TIMING 0u
#endif

#define TELEMETRY_MAGIC SAT_FOURCC('B', 'H', 'S', '1')

typedef struct bh_bench_result {
    uint32_t n;                  /* bullets held */
    uint32_t frames;             /* program frames measured */
    uint32_t vblanks;            /* display fields they took (sat_frame_count delta) */
    uint32_t ticks_total;        /* FRT ticks, whole frames */
    uint32_t ticks_serv, ticks_sim, ticks_build, ticks_draw, ticks_end;   /* summed per stage */
    uint32_t ticks_max;          /* longest frame */
    uint32_t late_swaps;         /* VDP1 lists that missed a VBlank */
    uint32_t commands;           /* VDP1 commands of the last frame */
    uint32_t drawn;              /* instances drawn on the last frame */
    uint32_t bullet_tests;       /* bullet-vs-player tests executed */
    uint32_t player_hits;
    uint32_t done;
} bh_bench_result_t;

typedef struct bh_telemetry {
    uint32_t magic;
    uint32_t setup_ok;           /* 1 once every boot step passed */
    uint32_t setup_error;        /* the first failing result code, 0 none */
    uint32_t frame;              /* program frames */
    uint32_t video_frames;       /* sat_frame_count() */
    uint32_t frt_hz;
    uint32_t sim_hash;
    uint32_t bullets, enemies, shots, explosions;
    uint32_t stage, stage_serial, bench_phase;
    uint32_t cmd_used, cmd_capacity;
    uint32_t instances;
    uint32_t batch_submitted, batch_culled, batch_dropped, batch_invalid, batch_no_preclip;
    uint32_t t_serv, t_sim, t_build, t_draw, t_end, t_wait;   /* last frame, FRT ticks */
    uint32_t late_swaps, draw_waits, draw_timeouts;
    uint32_t fps_x10;            /* over the last second */
    uint32_t scroll_x;           /* VDP2 NBG0 scroll, whole pixels */
    uint32_t underruns, refills, music_buffered, music_playing;
    uint32_t music_min_buffered, music_segments, music_loops, data_deferred;
    uint32_t stream_bytes, stream_segments, stream_jobs, stream_waits, stream_error;
    uint32_t stage_swaps;        /* stage transitions performed */
    uint32_t transition_stalls;  /* transitions that had to wait for the disc (must stay 0) */
    uint32_t group_ready_mask;   /* residency groups whose frames are drawable */
    uint32_t bg_swaps;
    uint32_t pad_held;
    uint32_t cd_progress_errors;
    uint32_t prof_vdp2, prof_pad, prof_disc, prof_audio, prof_poll, prof_music, prof_data;   /* cumulative FRT ticks of the service section */
    bh_bench_result_t bench[4];
    uint32_t ccr_initial, ccr_now;   /* SH-2 cache control register at the start of main and now */
} bh_telemetry_t;

volatile bh_telemetry_t g_bh_telemetry;

#ifdef BH_BENCH_N
/* BH_BENCH_N=n replaces the largest phase's bullet count (a sweep between 512 and 1024). */
static const uint16_t kBenchSizes[4] = {128u, 256u, 512u, BH_BENCH_N};
#else
static const uint16_t kBenchSizes[4] = {128u, 256u, 512u, 1024u};
#endif

static sat_cd_block_t g_cd_block;
static sat_cd_device_t g_cd_device;
static sat_cdfs_volume_t g_volume;
/* The two music rings (64 KiB) are too big for Work RAM High; .wram_l is zeroed at boot like .bss. */
static bh_music_rings_t g_music_rings __attribute__((section(".wram_l")));

static sat_sprite_atlas_t g_atlas;
static uint8_t g_atlas_tables[8192] __attribute__((aligned(4)));
/* Every atlas frame twice: as it is, and clipped to the playfield (bh_render.h). */
static sat_sprite_frame_t g_frames[2u * SHMUP_FRAME_COUNT];
static uint8_t g_ready[SHMUP_FRAME_COUNT];
static sat_sprite_instance_t g_instances[BH_MAX_INSTANCES];

static bh_sim_t g_sim;
static bh_view_t g_view;
static bh_stream_t g_stream;

static uint16_t g_active_serial;        /* the stage serial the screen and the resident groups follow */
static uint16_t g_prefetch_delay;       /* frames until the next stage's data is queued, 0 = nothing pending */
static uint8_t g_bg_pending;            /* a new background is to be shown at the next VBlank */
static uint8_t g_bg_slot;               /* VDP2 VRAM bank / CRAM bank pair currently shown: 0 or 1 */
static uint8_t g_autopilot = 1u;
static uint8_t g_bench_phase;           /* 0 off, 1..4 benchmark of kBenchSizes[phase-1] */
static uint32_t g_bench_count;
static uint32_t g_bench_vblank0, g_bench_late0, g_bench_tests0, g_bench_hits0;
static uint32_t g_fps_vblank0;
static uint32_t g_fps_x10;
static uint32_t g_tick_sum[9], g_tick_mean[9];   /* the CPU-time line of the HUD */

static sat_result_t note_setup(sat_result_t r) {
    if (r != SAT_OK && g_bh_telemetry.setup_error == 0u) g_bh_telemetry.setup_error = (uint32_t)(-(int32_t)r);
    return r;
}
#define SETUP(expr) do { if (note_setup(expr) != SAT_OK) goto setup_failed; } while (0)

/* A group's frames, plain and with the playfield clip (the second half of g_frames). */
static sat_result_t build_group_frames(uint32_t group) {
    SAT_TRY(sat_sprite_atlas_build_frames(&g_atlas, group, 0u, g_frames, SHMUP_FRAME_COUNT));
    return sat_sprite_atlas_build_frames(&g_atlas, group, SAT_SPRITE_FLAG_USER_CLIP,
                                         g_frames + SHMUP_FRAME_COUNT, SHMUP_FRAME_COUNT);
}

static uint8_t slot_of(uint16_t serial) { return (uint8_t)(1u + (serial & 1u)); }
static uint8_t group_of(uint16_t serial) { return bh_stage_group((uint16_t)(serial % BH_STAGES)); }
static uint8_t bg_of(uint16_t serial) { return (uint8_t)(serial % BH_STAGES); }

static sat_result_t service_cd_audio(void* context) {
    (void)context;
    return sat_audio_update();
}

static void pump_audio(void* context) {
    (void)context;
    (void)sat_audio_update();
}

/* ---- VDP2 ------------------------------------------------------------------------------- */

/* The background bitmap of VRAM bank `slot` (0: A0, 1: A1) with CRAM bank 0 or 2, shown on NBG0
 * behind the sprites. Recorded by the layer manager and replayed at every commit. */
static sat_result_t show_background(uint8_t slot) {
    sat_vdp2_layer_config_t c;
    sat_vdp2_layer_config_default(SAT_VDP2_NBG0, &c);
    c.bitmap = 1u;
    c.color_mode = SAT_VDP2_COLOR_MODE_256;
    c.bitmap_size = SAT_VDP2_BITMAP_512X256;
    c.priority = BG_PRIORITY;
    c.transparent = 0u;
    c.palette = (uint8_t)(slot * 2u);
    c.plane_address[0] = slot * BG_BANK_BYTES;
    return sat_vdp2_layer_configure(&c);
}

/* The field is 448 dots wide; the panels beside it show the back colour. */
static sat_result_t mask_panels(void) {
    sat_vdp2_window_rect_t field = {g_view.layout.field.x, 0u,
                                    (uint16_t)(g_view.layout.field.x + g_view.layout.field.width - 1), SCREEN_H - 1u};
    SAT_TRY(sat_vdp2_window_set_rect(0u, &field));
    sat_vdp2_screen_window_t w;
    sat_vdp2_screen_window_default(SAT_VDP2_SCREEN_NBG0, &w);
    w.w0 = SAT_VDP2_WINDOW_AREA_INSIDE;
    return sat_vdp2_screen_window_set(&w);
}

/* ---- stage data ------------------------------------------------------------------------- */

static void mark_group(uint8_t group, uint8_t ready) {
    sat_sprite_atlas_group_info_t gi;
    if (sat_sprite_atlas_group_info(&g_atlas, group, &gi) != SAT_OK) return;
    for (uint32_t i = 0; i < gi.frame_count; ++i) g_ready[gi.first_frame + i] = ready;
    if (ready) g_bh_telemetry.group_ready_mask |= 1u << group;
    else g_bh_telemetry.group_ready_mask &= ~(1u << group);
}

/* Queues the data of stage `serial` into the slot and bank it will use. */
static void queue_stage(uint16_t serial) {
    const uint8_t slot = (uint8_t)(serial & 1u);       /* VDP2 bank / palette pair */
    (void)bh_stream_queue_group(&g_stream, group_of(serial), slot_of(serial));
    (void)bh_stream_queue_background(&g_stream, bg_of(serial), (uint8_t)(slot * 2u), slot * BG_BANK_BYTES);
}

static uint8_t stage_ready(uint16_t serial) {
    return (uint8_t)(bh_stream_group_ready(&g_stream, group_of(serial)) &&
                     bh_stream_background_ready(&g_stream, bg_of(serial)));
}

/* The simulation moved to stage `serial`: show its data. The disc read normally finished long ago;
 * if it did not, finish it now and count the stall. */
static void enter_stage(uint16_t serial) {
    if (!stage_ready(serial)) {
        if (!bh_stream_busy(&g_stream)) queue_stage(serial);
        g_bh_telemetry.transition_stalls++;
        (void)bh_stream_finish(&g_stream);
        /* a queued job for another stage may have run first; make sure this one is in */
        if (!stage_ready(serial)) {
            queue_stage(serial);
            (void)bh_stream_finish(&g_stream);
        }
    }
    const uint8_t old_group = group_of(g_active_serial), new_group = group_of(serial);
    const uint8_t old_slot = slot_of(g_active_serial);
    if (old_group != new_group) {
        mark_group(old_group, 0u);
        (void)sat_sprite_atlas_evict(&g_atlas, old_slot);
        bh_stream_forget_group(&g_stream, old_group);
    }
    bh_stream_forget_background(&g_stream, bg_of(g_active_serial));
    (void)build_group_frames(new_group);
    mark_group(new_group, 1u);
    g_bg_slot = (uint8_t)(serial & 1u);
    g_bg_pending = 1u;
    g_active_serial = serial;
    g_prefetch_delay = PREFETCH_DELAY;
    g_bh_telemetry.stage_swaps++;
}

/* ---- input ------------------------------------------------------------------------------ */

/* The benchmark flies a fixed pattern (god mode is on, nothing needs dodging): the autopilot scans
 * every bullet, which would charge the measurement for work no game does. */
static uint16_t bench_input(uint32_t n) {
    uint16_t in = BH_IN_SHOT;
    const uint32_t t = n % 240u;
    in |= t < 120u ? BH_IN_LEFT : BH_IN_RIGHT;
    in |= (n / 120u) % 2u ? BH_IN_UP : BH_IN_DOWN;
    return in;
}

static uint16_t read_input(const sat_pad_state_t* pad) {
    uint16_t in = 0u;
    if (pad->held & SAT_PAD_UP) in |= BH_IN_UP;
    if (pad->held & SAT_PAD_DOWN) in |= BH_IN_DOWN;
    if (pad->held & SAT_PAD_LEFT) in |= BH_IN_LEFT;
    if (pad->held & SAT_PAD_RIGHT) in |= BH_IN_RIGHT;
    if (pad->held & SAT_PAD_A) in |= BH_IN_SHOT;
    if (pad->held & SAT_PAD_B) in |= BH_IN_BOMB;
    if (pad->held & SAT_PAD_C) in |= BH_IN_FOCUS;
    if (in) g_autopilot = 0u;
    if (pad->pressed & SAT_PAD_START) g_autopilot = 1u;
    if (g_bench_phase) in = bench_input(g_bench_count);
    else if (g_autopilot) in = bh_demo_input(&g_sim);
    return in;
}

/* ---- benchmark -------------------------------------------------------------------------- */

static void bench_begin(uint8_t phase) {
    g_bench_phase = phase;
    g_bench_count = 0u;
    bh_sim_set_god(&g_sim, 1u);
    bh_sim_set_steady_bullets(&g_sim, kBenchSizes[phase - 1u]);
    sat_vdp1_frame_stats_t fs;
    (void)sat_vdp1_frame_stats(&fs);
    g_bench_vblank0 = sat_frame_count();
    g_bench_late0 = fs.late_swaps;
    g_bench_tests0 = g_sim.stats.bullet_tests;
    g_bench_hits0 = g_sim.stats.player_hits;
    bh_bench_result_t* r = (bh_bench_result_t*)&g_bh_telemetry.bench[phase - 1u];
    __builtin_memset(r, 0, sizeof(*r));
    r->n = kBenchSizes[phase - 1u];
}

static void bench_end(void) {
    bh_bench_result_t* r = (bh_bench_result_t*)&g_bh_telemetry.bench[g_bench_phase - 1u];
    sat_vdp1_frame_stats_t fs;
    (void)sat_vdp1_frame_stats(&fs);
    r->vblanks = sat_frame_count() - g_bench_vblank0;
    r->late_swaps = fs.late_swaps - g_bench_late0;
    r->bullet_tests = g_sim.stats.bullet_tests - g_bench_tests0;
    r->player_hits = g_sim.stats.player_hits - g_bench_hits0;
    r->done = 1u;
}

/* ---- main ------------------------------------------------------------------------------- */

static sat_result_t boot(void) {
    sat_video_config_t video = {SCREEN_W, SCREEN_H, SAT_VIDEO_AUTO, 0u};
    SETUP(sat_init(&video));
    g_bh_telemetry.magic = TELEMETRY_MAGIC;
    g_bh_telemetry.frt_hz = sat_time_frequency_hz();
    g_bh_telemetry.ccr_initial = *(volatile uint8_t*)0xFFFFFE92u;
    g_bh_telemetry.ccr_now = *(volatile uint8_t*)0xFFFFFE92u;
#ifdef BH_FRAME_ON_SUBMIT
    SETUP(sat_vdp1_set_frame_mode(SAT_VDP1_FRAME_ON_SUBMIT));
#endif
    SETUP(sat_vdp2_cram_mode_set(0u));
    SETUP(sat_vdp2_back_color_set(SAT_RGB555(2, 2, 4)));
    SETUP(sat_vdp2_sprite_set_priority(SPRITE_PRIORITY));
    SETUP(bh_view_init(&g_view, SCREEN_W, SCREEN_H, BH_FIELD_W, BH_FIELD_H, SAT_TATE_ROT_CW));

    SETUP(sat_audio_init());
    SETUP(sat_cd_mount_default(&g_cd_block, service_cd_audio, 0, &g_cd_device, 0u, &g_volume));

    /* The atlas: tables first (they stay in memory), group data later through the stream. */
    {
        static const char* const bg_paths[3] = {"STAGE1.BG", "STAGE2.BG", "STAGE3.BG"};
        SETUP(bh_stream_init(&g_stream, &g_cd_block, &g_volume, &g_atlas, "SHMUP.ATL", bg_paths, 3u));
        uint8_t header[64] __attribute__((aligned(4)));
        uint32_t got = 0u, table_bytes = 0u, total_bytes = 0u;
        SETUP(sat_cdfs_read_at(&g_volume, &g_stream.atlas_file, 0u, header, sizeof(header), &got));
        SETUP(sat_sprite_atlas_peek(header, got, &table_bytes, &total_bytes));
        if (table_bytes > sizeof(g_atlas_tables)) SETUP(SAT_ERR_CAPACITY);
        SETUP(sat_cdfs_read_at(&g_volume, &g_stream.atlas_file, 0u, g_atlas_tables, table_bytes, &got));
        SETUP(sat_sprite_atlas_open(&g_atlas, g_atlas_tables, got));
    }
    {
        sat_sprite_atlas_group_info_t gi;
        uint32_t stage_bytes = 0u;
        SETUP(sat_sprite_atlas_group_info(&g_atlas, GROUP_COMMON, &gi));
        SETUP(sat_sprite_atlas_reserve_slot(&g_atlas, ATLAS_SLOT_COMMON, gi.data_bytes));
        for (uint32_t g = 1u; g < sat_sprite_atlas_group_count(&g_atlas); ++g) {
            SETUP(sat_sprite_atlas_group_info(&g_atlas, g, &gi));
            if (gi.data_bytes > stage_bytes) stage_bytes = gi.data_bytes;
        }
        SETUP(sat_sprite_atlas_reserve_slot(&g_atlas, 1u, stage_bytes));
        SETUP(sat_sprite_atlas_reserve_slot(&g_atlas, 2u, stage_bytes));
    }
    SETUP(sat_sprite_atlas_upload_palette(&g_atlas, SPRITE_BANK));
    SETUP(sat_vdp2_sprite_palette_bank_set(SPRITE_BANK));
    SETUP(sat_sprite_atlas_upload_luts(&g_atlas));

    /* Common art and the first stage, read now with the screen still dark. */
    bh_stream_set_pump(&g_stream, pump_audio, 0);
    SETUP(bh_stream_queue_group(&g_stream, GROUP_COMMON, ATLAS_SLOT_COMMON));
    queue_stage(0u);
    SETUP(bh_stream_finish(&g_stream));
    SETUP(build_group_frames(GROUP_COMMON));
    mark_group(GROUP_COMMON, 1u);
    SETUP(build_group_frames(group_of(0u)));
    mark_group(group_of(0u), 1u);

    SETUP(show_background(0u));
    SETUP(mask_panels());
    g_bg_slot = 0u;
    g_active_serial = 0u;
    g_prefetch_delay = PREFETCH_DELAY;

    /* The music streams from the same disc, scheduled with the stage data (bh_stream.c). */
    SETUP(bh_stream_music_start(&g_stream, &g_music_rings, &g_volume, "STAGE.PCM", BH_MUSIC_BYTES, BH_MUSIC_RATE, BH_MUSIC_SAMPLE_BYTES));

    {
        bh_params_t params;
        bh_params_default(&params);
        /* Collision radii come from the packed art, not from a table of guesses. */
        for (uint32_t s = 0; s < BH_SHAPES; ++s) {
            sat_sprite_atlas_frame_info_t fi;
            if (sat_sprite_atlas_frame_info(&g_atlas, bh_frame_bullet((uint8_t)s, 4u), &fi) == SAT_OK && fi.radius) {
                params.bullet_radius[s] = (uint8_t)fi.radius;
            }
        }
        bh_sim_init(&g_sim, &params, 7u);
    }
    g_bh_telemetry.setup_ok = 1u;
    return SAT_OK;
setup_failed:
    return SAT_ERR_IO;
}

int main(void) {
    if (boot() != SAT_OK) {
        /* Keep the program alive, showing the back colour, so a harness can read setup_error. */
        for (;;) {
            if (sat_wait_vblank() != SAT_OK) break;
            (void)sat_begin_frame();
            (void)sat_end_frame();
        }
        return 1;
    }

    sat_pad_state_t pad = {0};
    uint16_t t_last = sat_time_frc();
    g_fps_vblank0 = sat_frame_count();

    for (;;) {
        const uint16_t t_start = sat_time_frc();
        if (sat_wait_vblank() != SAT_OK) break;
        const uint16_t t_vblank = sat_time_frc();

        const uint16_t t_serv0 = sat_time_frc();
        /* VDP2 state goes in in the VBlank, in step with the sprite list that was submitted last. */
        if (g_bg_pending) {
            (void)show_background(g_bg_slot);
            g_bg_pending = 0u;
            g_bh_telemetry.bg_swaps++;
        }
        const uint32_t scroll_px = (g_sim.scroll_y >> BH_FP_SHIFT) & 511u;
        (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG0, (int32_t)(scroll_px << 16), 0);
        (void)sat_vdp2_layers_commit();
        g_bh_telemetry.scroll_x = scroll_px;

        const uint16_t t_pad0 = sat_time_frc();
        /* The pads were requested right after the last frame was submitted (below), so the SMPC
         * has been gathering them while the CPU did other work. */
        (void)sat_input_poll_finish();
        (void)sat_pad_tap_state(0u, 0u, &pad);
        const uint16_t t_pad1 = sat_time_frc();
        g_bh_telemetry.pad_held = pad.held;
        const uint16_t input = read_input(&pad);
        if ((pad.pressed & SAT_PAD_X) && !g_bench_phase) bench_begin(1u);
        if ((pad.pressed & SAT_PAD_Y)) bh_sim_set_stage_time(&g_sim, (uint16_t)(g_sim.params.stage_frames - 150u));
#ifdef BH_AUTOBENCH
        if (g_bh_telemetry.frame == AUTOBENCH_START && !g_bench_phase) {
#ifdef BH_BENCH_LOOP
            bench_begin(BH_BENCH_LOOP);
#else
            bench_begin(1u);
#endif
        }
#endif

        /* The disc: music segments first, stage data in the gaps (never blocks). */
        if (g_prefetch_delay && --g_prefetch_delay == 0u) queue_stage((uint16_t)(g_active_serial + 1u));
        const uint16_t t_disc0 = sat_time_frc();
        (void)bh_stream_service(&g_stream);
        const uint16_t t_disc1 = sat_time_frc();
        (void)sat_audio_update();
        const uint16_t t_audio1 = sat_time_frc();
        g_bh_telemetry.prof_vdp2 += (uint16_t)(t_pad0 - t_serv0);
        g_bh_telemetry.prof_pad += (uint16_t)(t_pad1 - t_pad0);
        g_bh_telemetry.prof_disc += (uint16_t)(t_disc1 - t_disc0);
        g_bh_telemetry.prof_audio += (uint16_t)(t_audio1 - t_disc1);

        const uint16_t t_sim0 = sat_time_frc();
        const uint32_t d_serv = (uint16_t)(t_sim0 - t_serv0);
        bh_sim_update(&g_sim, input);
        if (g_sim.stage_serial != g_active_serial) enter_stage(g_sim.stage_serial);
        const uint16_t t_sim1 = sat_time_frc();

        if (sat_begin_frame() != SAT_OK) break;
        bh_hud_t hud = {g_fps_x10 / 10u, g_sim.bullet_count, (uint32_t)g_sim.stage + 1u, BH_HUD_TIMING, {0}};
        for (uint32_t k = 0; k < 9u; ++k) hud.ticks[k] = g_tick_mean[k];
        uint32_t bullets_at = 0u;
        const uint32_t n = bh_render_build(&g_sim, &g_view, g_frames, SHMUP_FRAME_COUNT, g_ready, &hud,
                                           g_instances, BH_MAX_INSTANCES, &bullets_at);
        const uint16_t t_build1 = sat_time_frc();
        sat_sprite_batch_stats_t bs = {0}, bs2 = {0};
        /* The playfield sprites that cross the field's edge are clipped to it (bh_render.h). The
         * enemy bullets go in between the two instance runs, straight into the command list. */
        const sat_rect_t* field = &g_view.layout.field;
        (void)sat_vdp1_set_user_clip((int16_t)field->x, (int16_t)field->y,
                                     (int16_t)(field->x + field->width - 1), (int16_t)(field->y + field->height - 1));
        (void)sat_sprite_batch_draw(g_frames, 2u * SHMUP_FRAME_COUNT, g_instances, bullets_at, 0u, &bs);
        sat_sprite_batch_stats_t bs_bullets = {0};
        bh_render_bullets(&g_sim, &g_view, g_frames, SHMUP_FRAME_COUNT, g_ready, &bs_bullets);
        (void)sat_sprite_batch_draw(g_frames, 2u * SHMUP_FRAME_COUNT, g_instances + bullets_at, n - bullets_at, 0u, &bs2);
        bs.submitted += bs_bullets.submitted + bs2.submitted;
        bs.culled += bs_bullets.culled + bs2.culled;
        bs.dropped += bs_bullets.dropped + bs2.dropped;
        bs.invalid += bs_bullets.invalid + bs2.invalid;
        bs.no_preclip += bs_bullets.no_preclip + bs2.no_preclip;
        const uint16_t t_draw1 = sat_time_frc();
        sat_vdp1_command_stats_t cs = {0};
        (void)sat_vdp1_command_stats(&cs);
        (void)sat_end_frame();
        (void)sat_input_poll_begin();
        const uint16_t t_end = sat_time_frc();

        /* ---- bookkeeping ---- */
        const uint32_t d_sim = (uint16_t)(t_sim1 - t_sim0), d_build = (uint16_t)(t_build1 - t_sim1);
        const uint32_t d_draw = (uint16_t)(t_draw1 - t_build1), d_end = (uint16_t)(t_end - t_draw1);
        const uint32_t d_total = (uint16_t)(t_end - t_start);
        const uint32_t d_wait = (uint16_t)(t_vblank - t_start);
        g_tick_sum[0] += d_serv; g_tick_sum[1] += d_sim; g_tick_sum[2] += d_build;
        g_tick_sum[3] += d_draw; g_tick_sum[4] += d_end;
        g_tick_sum[5] += (uint16_t)(t_pad0 - t_serv0); g_tick_sum[6] += (uint16_t)(t_pad1 - t_pad0);
        g_tick_sum[7] += (uint16_t)(t_disc1 - t_disc0); g_tick_sum[8] += (uint16_t)(t_audio1 - t_disc1);
        (void)t_last;
        t_last = t_end;

        sat_vdp1_frame_stats_t fs;
        (void)sat_vdp1_frame_stats(&fs);
        bh_stream_music_stats(&g_stream, (uint32_t*)&g_bh_telemetry.underruns, (uint32_t*)&g_bh_telemetry.refills,
                              (uint32_t*)&g_bh_telemetry.music_buffered);
        g_bh_telemetry.music_playing = g_stream.music_wanted;
        g_bh_telemetry.music_min_buffered = g_stream.stats.music_min_buffered;
        g_bh_telemetry.music_segments = g_stream.stats.music_segments;
        g_bh_telemetry.music_loops = g_stream.stats.music_loops;
        g_bh_telemetry.data_deferred = g_stream.stats.data_deferred;
        g_bh_telemetry.frame++;
        g_bh_telemetry.video_frames = sat_frame_count();
        g_bh_telemetry.sim_hash = bh_sim_hash(&g_sim);
        g_bh_telemetry.bullets = g_sim.bullet_count;
        g_bh_telemetry.enemies = g_sim.enemy_count;
        g_bh_telemetry.shots = g_sim.shot_count;
        g_bh_telemetry.explosions = g_sim.explosion_count;
        g_bh_telemetry.stage = g_sim.stage;
        g_bh_telemetry.stage_serial = g_sim.stage_serial;
        g_bh_telemetry.bench_phase = g_bench_phase;
        g_bh_telemetry.cmd_used = cs.used;
        g_bh_telemetry.cmd_capacity = cs.capacity;
        g_bh_telemetry.instances = n;
        g_bh_telemetry.batch_submitted = bs.submitted;
        g_bh_telemetry.batch_culled = bs.culled;
        g_bh_telemetry.batch_dropped = bs.dropped;
        g_bh_telemetry.batch_invalid = bs.invalid;
        g_bh_telemetry.batch_no_preclip = bs.no_preclip;
        g_bh_telemetry.t_serv = d_serv;
        g_bh_telemetry.t_sim = d_sim;
        g_bh_telemetry.t_build = d_build;
        g_bh_telemetry.t_draw = d_draw;
        g_bh_telemetry.t_end = d_end;
        g_bh_telemetry.t_wait = d_wait;
        g_bh_telemetry.late_swaps = fs.late_swaps;
        g_bh_telemetry.draw_waits = fs.draw_waits;
        g_bh_telemetry.draw_timeouts = fs.draw_timeouts;
        g_bh_telemetry.prof_poll = g_stream.stats.ticks_poll;
        g_bh_telemetry.prof_music = g_stream.stats.ticks_music;
        g_bh_telemetry.prof_data = g_stream.stats.ticks_data;
        g_bh_telemetry.fps_x10 = g_fps_x10;
        g_bh_telemetry.stream_bytes = g_stream.stats.bytes;
        g_bh_telemetry.stream_segments = g_stream.stats.segments;
        g_bh_telemetry.stream_jobs = g_stream.stats.jobs_done;
        g_bh_telemetry.stream_waits = g_stream.stats.waits;
        g_bh_telemetry.stream_error = (uint32_t)(-g_stream.stats.last_error);
        g_bh_telemetry.cd_progress_errors = g_cd_block.progress_error_count;

        if ((g_bh_telemetry.frame % 60u) == 0u) {
            const uint32_t v = g_bh_telemetry.video_frames - g_fps_vblank0;
            g_fps_x10 = v ? (600u * sat_video_refresh_hz()) / v : 0u;   /* 60 program frames took v fields */
            g_fps_vblank0 = g_bh_telemetry.video_frames;
            for (uint32_t k = 0; k < 9u; ++k) { g_tick_mean[k] = g_tick_sum[k] / 60u; g_tick_sum[k] = 0u; }
        }

        if (g_bench_phase) {
            bh_bench_result_t* r = (bh_bench_result_t*)&g_bh_telemetry.bench[g_bench_phase - 1u];
            r->frames++;
            r->ticks_total += d_total;
            r->ticks_serv += d_serv; r->ticks_sim += d_sim; r->ticks_build += d_build; r->ticks_draw += d_draw; r->ticks_end += d_end;
            if (d_total > r->ticks_max) r->ticks_max = d_total;
            r->commands = cs.used;
            r->drawn = n + bs_bullets.submitted + bs_bullets.culled;   /* instances plus the bullets written direct */
            if (++g_bench_count >= BENCH_FRAMES) {
                bench_end();
#ifdef BH_BENCH_LOOP
                bench_begin(BH_BENCH_LOOP);
#else
                if (g_bench_phase < 4u) {
                    bench_begin((uint8_t)(g_bench_phase + 1u));
                } else {
                    g_bench_phase = 0u;
                    bh_sim_set_steady_bullets(&g_sim, 0u);
                    bh_sim_set_god(&g_sim, 0u);
                }
#endif
            }
        }
    }
    (void)sat_shutdown();
    return 0;
}
