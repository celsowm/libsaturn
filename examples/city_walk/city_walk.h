#ifndef CITY_WALK_H
#define CITY_WALK_H

/* Shared contract for city_walk. main.c owns start-up order and the frame
 * loop; the rest lives by concern (loader.c, residency.c, materials.c, ...).
 * city_grid.h and city_format.h are pure and tested natively. */

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/font.h"
#include "saturn/ram_cart.h"
#include "saturn/scene.h"
#include "saturn/scene3d_faces.h"
#include "saturn/scene3d_material_pool.h"
#include "saturn/math3d.h"
#include "saturn/time.h"

#include "city_format.h"
#include "city_grid.h"

#define CITY_W 320u
#define CITY_H 224u
#define CITY_HUD_COMMANDS 160u
#define CITY_WRAM_L __attribute__((section(".wram_l")))

/* VDP2 VRAM word offsets. The 512x512 8bpp ground bitmap (one dot per unit,
 * fine enough for lane dashes and crosswalks) fills banks A0 and A1; the
 * rotation and coefficient tables sit in B0; the sky (NBG0) lives in B1. */
#define CITY_GROUND_BITMAP_WORD 0x00000u
#define CITY_ROTATION_WORD 0x20000u
#define CITY_COEFFICIENT_WORD 0x22000u

/* CRAM banks (256 colours each). */
#define CITY_PALETTE_GROUND 0u
#define CITY_PALETTE_SKY 1u
#define CITY_PALETTE_FONT 2u
#define CITY_PALETTE_SOLID 4u
#define CITY_PALETTE_TEXTURE 5u /* facade textures, from the archive */

/* States a harness can read out of city_telemetry_t. */
enum {
    CITY_STATE_BOOT = 0,
    CITY_STATE_LOADING = 1,
    CITY_STATE_RUNNING = 2,
    CITY_STATE_NO_CART = 3,
    CITY_STATE_CART_TOO_SMALL = 4,
    CITY_STATE_LOAD_FAILED = 5
};

/* One flat struct of uint32_t, so the harness reads it with the map file and a
 * field list parsed from this header. Add fields at the END of a section only
 * if a test depends on offsets; the test parses names, not positions. */
typedef struct city_telemetry {
    uint32_t magic;
    uint32_t state;
    uint32_t status;
    uint32_t cart_type;
    uint32_t cart_capacity;
    uint32_t archive_bytes;
    uint32_t toc_crc_ok;
    uint32_t blob_base;
    uint32_t material_count;
    uint32_t ground_loaded;
    uint32_t ticks_per_vblank;
    uint32_t load_total_vblanks;
    uint32_t load_cd_ticks;
    uint32_t load_copy_ticks;
    uint32_t load_spans;
    uint32_t chunks_loaded;
    uint32_t evictions;
    uint32_t prime_loads;
    uint32_t pop_in_events;
    uint32_t stale_generation_submits;
    uint32_t decode_failures;
    uint32_t decode_first_error; /* (site << 16) | (uint16_t)status of the first failure */
    uint32_t frames;
    uint32_t max_world_faces;
    uint32_t max_vdp1_commands;
    uint32_t scene_first_error;
    uint32_t resident_slots; /* slots holding a loaded (or empty) chunk right now */
    uint32_t chunk_x;
    uint32_t chunk_z;
    uint32_t local_x_fx;
    uint32_t local_z_fx;
    uint32_t collisions; /* frames in which the walker was pushed out of a building */
    uint32_t yaw_degrees;
    uint32_t hist0, hist1, hist2, hist3, hist4, hist5, hist6, hist7;
    /* Sums of FRT ticks over the measured frames; divide by `measured_frames`. */
    uint32_t measured_frames;
    uint32_t t_frame;      /* top of loop to after sat_end_frame */
    uint32_t t_vdp2;       /* ground + sky + layer commit */
    uint32_t t_input;      /* pad poll, player, page-in */
    uint32_t t_plan;       /* city_plan_frame incl. frustum tests */
    uint32_t t_decode;     /* city_blob_decode */
    uint32_t t_submit;     /* sat_scene_submit_instance */
    uint32_t t_flush;      /* sat_scene_flush: sort + VDP1 emission */
    uint32_t t_texture;    /* cart -> VDP1 VRAM texture block copies */
    uint32_t t_hud;
    uint32_t t_wait;           /* Master waiting for the Slave batch */
    uint32_t parallel_backend; /* 1 = Slave SH-2 running, 0 = everything on the Master */
    uint32_t slave_batches;
    uint32_t slave_items;
    uint32_t slave_fallbacks;
    uint32_t last_submitted, last_flushed, last_culled, last_skipped, last_rejected, last_budget_blocked;
    uint32_t faded_cells; /* cells drawn through a colour-calc slot (distance fade) */
    uint32_t hud_status; /* first non-OK result from a HUD draw call, as -code */
    uint32_t textures_uploaded;       /* texture blocks copied cart -> VDP1 VRAM */
    uint32_t texture_bytes_uploaded;
    uint32_t texture_failures;        /* a block refused or not written: faces drawn solid */
    uint32_t max_textured_faces;      /* textured faces submitted in one frame */
    uint32_t texture_vram_base;       /* the reserved VDP1 VRAM arena, bytes */
    uint32_t cart_used_bytes;          /* RAM-cart allocator consumption after CITY.BIN load */
    uint32_t cart_free_bytes;          /* RAM-cart allocator free bytes after CITY.BIN load */
    uint32_t done;
} city_telemetry_t;

#define CITY_TELEMETRY_MAGIC 0x43495459u /* "CITY" */
extern volatile city_telemetry_t g_city;

/* Free-running timer: sat_frame_count() floors per call, so summing short
 * operations with it gives 0 (see the R3 probe). */
static inline uint16_t city_frt_now(void) {
    return sat_time_frc();
}

/* telemetry.c ------------------------------------------------------------- */
void telemetry_init(void);

/* loader.c ---------------------------------------------------------------- */
typedef struct city_archive {
    city_header_t header;
    uint8_t* bank[2];            /* cart pointers, one per 2 MiB half of the blobs */
    uint8_t table[CITY_MATERIAL_ENTRY_BYTES * CITY_MATERIAL_MAX + 32u +
                  CITY_TOC_ENTRIES * CITY_TOC_ENTRY_BYTES + 64u];
    const uint8_t* toc;          /* into table */
    const uint8_t* materials;    /* into table */
    uint16_t ground_palette[256];
    uint16_t texture_palette[256];
} city_archive_t;

extern city_archive_t g_archive;

/* Reports progress between units of work: stage text and 0..100. */
typedef void (*city_progress_fn)(const char* stage, uint8_t percent);

/* Checks the cart, then streams CITY.BIN: blobs into the cart, the ground
 * bitmap into VDP2 VRAM, the palette into CRAM, the TOC into g_archive.
 * SAT_ERR_NOT_CONNECTED / SAT_ERR_UNSUPPORTED mean no cart / a 1 MiB cart;
 * the caller shows the refusal. `staging` is 64 KiB of caller memory. */
sat_result_t city_loader_run(uint8_t* staging, uint32_t staging_bytes,
                             city_progress_fn progress);

/* residency.c ------------------------------------------------------------- */
/* 9 x 5664 + 25 x 1536 + 49 x 1024 = 136.3 KiB: the only geometry ever in
 * work RAM, as raw big-endian blobs (their decoded form is 1.5-2x larger). The
 * loader borrows its first 64 KiB as CD staging while the pool is still empty. */
#define CITY_POOL_BYTES (9u * CITY_SLOT_BYTES_LOD0 + 25u * CITY_SLOT_BYTES_LOD1 +                          49u * CITY_SLOT_BYTES_LOD2)
extern uint8_t g_ring_pool[CITY_POOL_BYTES];
extern city_residency_t g_res;
void residency_init(void);
void residency_recenter(const city_pos_t* pos);
/* Services one pending slot: cart -> WRAM-L. Returns 1 when a slot was served. */
int residency_service_one(const city_pos_t* pos);
/* Blocking fill of every ring slot; only before the first frame. */
void residency_prime(const city_pos_t* pos);
const uint8_t* residency_slot_blob(uint16_t slot, uint16_t* out_bytes);

/* A slot's facade textures: its blob's texture table, compacted. The texels
 * sit in the slot's VDP1 VRAM at vram_base + 8 * offset8. */
typedef struct city_slot_texture {
    uint8_t width8;  /* width / 8 */
    uint8_t height;
    uint16_t offset8;
    uint16_t flags;
} city_slot_texture_t;
uint16_t residency_slot_textures(uint16_t slot, const city_slot_texture_t** out_table,
                                 uint32_t* out_vram_base);
/* Copies the texture block of the slot served this frame into VDP1 VRAM and
 * only then marks the slot drawable. Call right before sat_end_frame, which
 * waits for the VDP1 anyway: the write must not race the list it is drawing. */
void residency_upload_textures(void);

/* materials.c ------------------------------------------------------------- */
extern uint16_t g_material_map[CITY_MATERIAL_MAX];
void materials_init(void);
const sat_scene3d_material_t* materials_table(uint16_t* out_count);
/* The VDP1 RGB code of a pool handle (for a textured face's fallback). */
uint16_t materials_rgb(uint16_t handle);

/* render.c ---------------------------------------------------------------- */
typedef struct city_view {
    city_pos_t pos;      /* chunk + local position of the eye's ground point */
    sat_fx16_t eye_y;    /* world height of the eye, 16.16 */
    sat_fx16_t yaw;      /* degrees, 16.16; 0 = looking along +Z, growing turns left */
} city_view_t;
void render_init(void);
/* Work RAM the renderer reserves (scene faces, scratch, Slave arenas), for the plan. */
uint32_t render_wram_bytes(void);
/* Builds and flushes one frame of city geometry. HUD is drawn after, by the
 * caller, inside the reserved overlay commands. */
void render_frame(const city_view_t* view);

/* ground.c / background.c: the VDP2 layers ------------------------------- */
void ground_init(void);
void ground_update(const city_view_t* view);
/* scratch: 512*128 pixel bytes plus a 64x64 uint16 map (73728 bytes). */
void background_init(uint8_t* scratch, uint32_t scratch_bytes);
void background_update(sat_fx16_t yaw);

/* player.c ---------------------------------------------------------------- */
#include "saturn/input.h"
typedef struct city_player {
    city_view_t view;
} city_player_t;
void player_init(city_player_t* p);
/* Applies one frame of input; `vblanks` is the time since the last call. Returns
 * 1 when the player crossed into another chunk. */
int player_update(city_player_t* p, const sat_pad_state_t* pad, uint32_t vblanks);
/* Jumps to the next of a few street-level viewpoints (the X button). The caller
 * re-primes the residency rings afterwards. */
void player_next_viewpoint(city_player_t* p);

/* hud.c ------------------------------------------------------------------- */
void hud_init(const sat_ascii_font_t* font);
void hud_draw(const city_view_t* view, uint32_t vblanks_per_frame, uint32_t frame);

#endif /* CITY_WALK_H */
