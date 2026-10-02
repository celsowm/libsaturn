#ifndef SATURN_SPRITE_CLIP_H
#define SATURN_SPRITE_CLIP_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/geometry2d.h"
#include "saturn/texture.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Animation clips and a player for them. This sits next to sprite_anim.h, which only selects a
 * frame by number, and adds what a real character needs: frames of different lengths, loop /
 * one-shot / ping-pong playback, a playback rate, pause and resume, a finished state, switching
 * between clips, a per-frame pivot, per-frame event ids, and generic rectangles and points on
 * each frame. The library gives none of that a meaning: a rectangle is a rectangle with a `kind`
 * and an `index` the game defines, so the same data can hold hurtboxes, attack boxes, interaction
 * areas and attachment points.
 *
 * Clips are immutable data, written offline (the animation packer of Phase 10) or by hand. A frame
 * names a logical region of a texture (a sat_rect_t, never a VRAM address); the texture and its
 * region cache are the existing texture API's business, and sat_clip_set_prepare_regions asks it
 * to prepare every region of a set at load time.
 *
 * Time is counted in ticks. One call to sat_clip_player_step is one tick at rate SAT_FX16_ONE
 * (what a fixed 60 Hz or 50 Hz game step gives); the rate is 16.16, so 0.5 plays at half speed and
 * 2.0 at double. Durations are whole ticks, at least 1. The player keeps the fractional part of
 * the time inside a frame, so a rate that is not a whole number never drifts, and a long step may
 * cross several frames: every event on the way is delivered, in order.
 *
 * A frame's pivot is a position in pixels from the top-left corner of its source region (a
 * continuous coordinate: the middle of a 32 px wide frame is 16). The pivot is what sits at the
 * sprite's position, so a flipped sprite mirrors around it. Shape coordinates are relative to the
 * same pivot, and the helpers mirror them with the sprite. */

#define SAT_CLIP_FLIP_X 1u /* the same bit values as SAT_FLIP_X / SAT_FLIP_Y of render2d */
#define SAT_CLIP_FLIP_Y 2u

typedef enum sat_clip_mode {
    SAT_CLIP_ONCE = 0,      /* plays to the last frame, holds it, and the player reports finished */
    SAT_CLIP_LOOP = 1,      /* after the last frame goes back to `loop_start` */
    SAT_CLIP_PING_PONG = 2  /* 0 1 2 1 0 1 2 ...: the end frames are shown once per pass */
} sat_clip_mode_t;

typedef struct sat_clip_shape {
    int16_t x, y;      /* top-left, relative to the frame's pivot */
    uint16_t w, h;     /* 0 x 0 is a point (an attachment, a muzzle) */
    uint8_t kind;      /* game-defined */
    uint8_t index;     /* game-defined */
    uint16_t flags;    /* game-defined */
} sat_clip_shape_t;

typedef struct sat_clip_frame {
    sat_rect_t source;       /* the region of the texture, non-empty */
    int16_t pivot_x, pivot_y;
    uint16_t duration;       /* ticks, >= 1 */
    uint16_t event;          /* delivered when the frame is entered; 0 = none */
    uint16_t shape_first;    /* index into the set's shape array */
    uint8_t shape_count;
    uint8_t flags;           /* game-defined */
} sat_clip_frame_t;

typedef struct sat_clip {
    const sat_clip_frame_t* frames;
    uint16_t frame_count;    /* >= 1 */
    uint16_t loop_start;     /* LOOP: the frame the loop returns to, < frame_count */
    uint8_t mode;            /* sat_clip_mode_t */
    uint8_t reserved[3];
} sat_clip_t;

typedef struct sat_clip_set {
    const sat_clip_t* clips;
    uint16_t clip_count;
    uint16_t shape_count;
    const sat_clip_shape_t* shapes;
} sat_clip_set_t;

/* The whole set against its own rules: every clip has frames, durations >= 1, non-empty sources, a valid
 * mode and loop start, and every frame's shapes lie inside the shape array. SAT_ERR_INVALID_ARG otherwise. */
sat_result_t sat_clip_set_validate(const sat_clip_set_t* set);

/* How many texture regions the set uses (one per frame, counting repeats), for a content budget
 * against sat_texture_region_capacity(). */
uint32_t sat_clip_set_region_count(const sat_clip_set_t* set);

/* Asks the texture API to prepare every frame's source region (idempotent per region). Stops at the
 * first error and returns it. */
sat_result_t sat_clip_set_prepare_regions(const sat_clip_set_t* set, sat_texture_t texture);

/* ----- playback ----- */

typedef enum sat_clip_switch {
    SAT_CLIP_SWITCH_RESTART = 0,      /* always start the clip from its first frame */
    SAT_CLIP_SWITCH_KEEP_IF_SAME = 1, /* the clip that is already playing is left alone (even a finished one); otherwise restart */
    SAT_CLIP_SWITCH_KEEP_PHASE = 2    /* carry the position over, scaled to the new clip's length (walk -> run) */
} sat_clip_switch_t;

#define SAT_CLIP_STEP_FRAME_CHANGED 0x01u
#define SAT_CLIP_STEP_LOOPED 0x02u    /* a loop or a ping-pong cycle completed */
#define SAT_CLIP_STEP_FINISHED 0x04u  /* a one-shot reached its end during this step */

typedef struct sat_clip_step_result {
    uint32_t frames_advanced;
    uint32_t events;   /* delivered by this step */
    uint8_t flags;     /* SAT_CLIP_STEP_* */
    uint8_t reserved[3];
} sat_clip_step_result_t;

/* Called for each frame event, in playback order. `frame` is the frame just entered. */
typedef void (*sat_clip_event_fn)(void* user, uint16_t event, uint16_t clip, uint16_t frame);

typedef struct sat_clip_player {
    const sat_clip_set_t* set;
    uint16_t clip;
    uint16_t frame;
    sat_fx16_t elapsed;  /* ticks into the current frame, 16.16 */
    sat_fx16_t rate;     /* ticks per step, 16.16, >= 0 */
    uint32_t loops;      /* completed loops / ping-pong cycles since the clip started */
    uint8_t playing;     /* a clip has been chosen */
    uint8_t paused;
    uint8_t finished;
    uint8_t reverse;     /* ping-pong is on its way back */
} sat_clip_player_t;

/* The set must outlive the player. Nothing is playing yet. */
sat_result_t sat_clip_player_init(sat_clip_player_t* player, const sat_clip_set_t* set);

/* Starts (or switches to) a clip. The first frame's event is delivered through `event` when the clip
 * restarts. Rate and pause are unaffected. SAT_ERR_INVALID_ARG for an unknown clip. */
sat_result_t sat_clip_player_play(sat_clip_player_t* player, uint16_t clip, sat_clip_switch_t how,
    sat_clip_event_fn event, void* user);
/* The current clip from its first frame again (a finished one-shot plays again). */
sat_result_t sat_clip_player_restart(sat_clip_player_t* player, sat_clip_event_fn event, void* user);
/* Jumps to the start of `frame` of the current clip. */
sat_result_t sat_clip_player_seek(sat_clip_player_t* player, uint16_t frame, sat_clip_event_fn event, void* user);

sat_result_t sat_clip_player_set_rate(sat_clip_player_t* player, sat_fx16_t rate);
sat_result_t sat_clip_player_pause(sat_clip_player_t* player);
sat_result_t sat_clip_player_resume(sat_clip_player_t* player);

/* One tick (times the rate). `event` may be NULL; `out` may be NULL. A paused, finished or idle player
 * does nothing. */
sat_result_t sat_clip_player_step(sat_clip_player_t* player, sat_clip_event_fn event, void* user,
    sat_clip_step_result_t* out);
/* `ticks` ticks (16.16) times the rate, for variable steps. */
sat_result_t sat_clip_player_advance(sat_clip_player_t* player, sat_fx16_t ticks, sat_clip_event_fn event, void* user,
    sat_clip_step_result_t* out);

int sat_clip_player_is_playing(const sat_clip_player_t* player);
int sat_clip_player_is_paused(const sat_clip_player_t* player);
int sat_clip_player_is_finished(const sat_clip_player_t* player);
uint16_t sat_clip_player_clip(const sat_clip_player_t* player);
uint16_t sat_clip_player_frame_index(const sat_clip_player_t* player);
/* NULL when nothing is playing. */
const sat_clip_frame_t* sat_clip_player_frame(const sat_clip_player_t* player);

/* ----- frame data ----- */

/* The shapes of a frame; NULL and 0 when it has none. */
const sat_clip_shape_t* sat_clip_frame_shapes(const sat_clip_set_t* set, const sat_clip_frame_t* frame, uint32_t* out_count);
/* The first shape of `kind` with `index` (pass 0xFF as index to take any); 0 when there is none. */
int sat_clip_frame_find_shape(const sat_clip_set_t* set, const sat_clip_frame_t* frame, uint8_t kind, uint8_t index,
    sat_clip_shape_t* out);

/* Where the frame's source rectangle lands when the pivot is at (x, y). With flips the sprite is
 * mirrored around the pivot. This is the destination rectangle for sat_draw_texture. */
sat_result_t sat_clip_frame_dest(const sat_clip_frame_t* frame, int16_t x, int16_t y, uint8_t flip, sat_rect_t* out);
/* A shape in the same space (mirrored with the sprite). */
sat_result_t sat_clip_shape_rect(const sat_clip_shape_t* shape, int16_t x, int16_t y, uint8_t flip, sat_rect_t* out);
sat_point_t sat_clip_shape_point(const sat_clip_shape_t* shape, int16_t x, int16_t y, uint8_t flip);

/* Draws the player's current frame with sat_draw_texture: its source region at the destination
 * given by the pivot, mirrored by `flip`. `params` may be NULL; its flip field is replaced by `flip`.
 * SAT_ERR_INVALID_ARG when nothing is playing. */
struct sat_draw_params;
sat_result_t sat_clip_player_draw(const sat_clip_player_t* player, sat_texture_t texture, int16_t x, int16_t y,
    uint8_t flip, const struct sat_draw_params* params);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SPRITE_CLIP_H */
