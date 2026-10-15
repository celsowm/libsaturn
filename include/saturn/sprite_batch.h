#ifndef SATURN_SPRITE_BATCH_H
#define SATURN_SPRITE_BATCH_H

#include <stdint.h>
#include "saturn/core.h"
#include "saturn/vdp1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bulk submission of many identical-kind VDP1 normal sprites (bullets,
 * particles, tiles of enemies). A frame is prepared once: every command word
 * that does not depend on position is composed up front, so submitting an
 * instance is one capacity-checked 32-byte store with no per-sprite
 * validation, texture lookup or camera work. sat_draw_texture() stays the
 * general path for tinted, scaled, rotated or camera-transformed sprites.
 *
 * The batch path draws plain replace sprites only (optionally mesh, opaque,
 * flipped): no tint, blend, scale, rotation, camera or user clip. Bits in
 * flags outside that set make sat_sprite_frame_init fail. */
typedef struct sat_sprite_frame {
    /* The first twelve bytes are the VDP1 command's own, so the batch copies them as three
     * longwords (hence the 4-byte alignment: a misaligned longword read is an address error on
     * the SH-2): no per-sprite composing. Normal sprite, no Gouraud shading. */
    uint16_t ctrl;     /* CMDCTRL: normal sprite + Dir (flip) bits */
    uint16_t link;     /* CMDLINK: 0 */
    uint16_t pmod;     /* CMDPMOD without the pre-clipping bit */
    uint16_t colr;     /* CMDCOLR: bank << 8 (INDEX8) or LUT address / 8 (LUT4) */
    uint16_t srca;     /* CMDSRCA: character address / 8 */
    uint16_t size;     /* CMDSIZE: (width / 8) << 8 | height */
    int16_t pivot_x;   /* the instance position is this texel of the frame */
    int16_t pivot_y;
} __attribute__((aligned(4))) sat_sprite_frame_t;

/* The frame's size in texels, from CMDSIZE. */
static inline uint16_t sat_sprite_frame_width(const sat_sprite_frame_t* frame) {
    return (uint16_t)((frame->size >> 8) << 3);
}
static inline uint16_t sat_sprite_frame_height(const sat_sprite_frame_t* frame) {
    return (uint16_t)(frame->size & 0xFFu);
}

typedef struct sat_sprite_instance {
    int16_t x;         /* screen pixels, origin top-left, of the frame's pivot */
    int16_t y;
    uint16_t frame;    /* index into the frame array */
    uint16_t reserved; /* zero */
} sat_sprite_instance_t;

/* Options for sat_sprite_batch_draw. */
/* Never set the pre-clipping disable bit (CMDPMOD bit 11): the commands are
 * then byte-identical to the sat_draw_texture path. */
#define SAT_SPRITE_BATCH_KEEP_PRECLIP 0x0001u
/* Do not cull sprites that lie entirely outside the screen. */
#define SAT_SPRITE_BATCH_NO_CULL 0x0002u

typedef struct sat_sprite_batch_stats {
    uint32_t submitted;   /* commands appended to the frame's list */
    uint32_t culled;      /* entirely off screen, no command */
    uint32_t dropped;     /* command list full (SAT_ERR_CAPACITY) */
    uint32_t invalid;     /* frame index out of range, skipped */
    uint32_t no_preclip;  /* submitted with pre-clipping disabled */
} sat_sprite_batch_stats_t;

/* Builds a frame from an uploaded texture. flags: SAT_SPRITE_FLAG_OPAQUE,
 * _MESH, _FLIP_H, _FLIP_V, _USER_CLIP (draw only inside the rectangle of the
 * last sat_vdp1_set_user_clip, which the caller appends before the batch;
 * such a frame is never submitted with pre-clipping disabled). The pivot is the texel the instance position names;
 * it may lie outside the frame (a trimmed sprite whose anchor is off its art). */
sat_result_t sat_sprite_frame_init(sat_sprite_frame_t* frame,
                                   const sat_vdp1_texture_t* texture,
                                   uint16_t flags,
                                   int16_t pivot_x, int16_t pivot_y);

/* Same from raw VDP1 values, for frames of a sprite atlas whose character
 * address and colour word are already known. */
sat_result_t sat_sprite_frame_init_raw(sat_sprite_frame_t* frame,
                                       uint16_t srca, uint16_t width, uint16_t height,
                                       uint16_t format, uint16_t palette, uint16_t flags,
                                       int16_t pivot_x, int16_t pivot_y);

/* Appends one VDP1 command per visible instance to the current frame's list,
 * in order (later instances draw on top). Sprites with any part on screen are
 * submitted and clipped by the VDP1's system clip; sprites entirely inside the
 * screen are submitted with pre-clipping disabled (VDP1 manual 6.3 Pclp: for
 * small items the per-line detection costs up to 5 clocks per line) unless
 * SAT_SPRITE_BATCH_KEEP_PRECLIP, and never when the frame is flipped
 * horizontally, which the bit forbids.
 *
 * Returns SAT_OK, SAT_ERR_CAPACITY when the list filled up (the instances
 * that fit were submitted; stats->dropped counts the rest) or
 * SAT_ERR_INVALID_ARG when an instance named a frame that does not exist (the
 * others were still processed). The protected overlay quota of
 * sat_vdp1_reserve_overlay_commands is never consumed. stats may be NULL. */
sat_result_t sat_sprite_batch_draw(const sat_sprite_frame_t* frames, uint32_t frame_count,
                                   const sat_sprite_instance_t* instances, uint32_t count,
                                   uint32_t options, sat_sprite_batch_stats_t* stats);

/* Direct writes. sat_sprite_batch_draw takes an array of instances; a program whose sprites come
 * straight from its own state (a pool of bullets) can skip that array: it takes a window of command
 * slots in the frame's list, writes each sprite's command with sat_sprite_batch_emit and gives the
 * window back with sat_sprite_batch_end. The commands are the ones sat_sprite_batch_draw writes
 * (same culling, same pre-clipping rule), in the order they are emitted.
 *
 *     sat_sprite_batch_window_t win;
 *     if (sat_sprite_batch_begin(&win, max_sprites, 0u) == SAT_OK) {
 *         for (...) sat_sprite_batch_emit(&win, &frames[f], x, y);
 *         sat_sprite_batch_end(&win, NULL);
 *     }
 *
 * Nothing else may append to the frame's list between begin and end. The window is a plain struct:
 * copy it to a local before a loop so the compiler keeps its fields in registers. */
typedef struct sat_sprite_batch_window {
    uint32_t* base;       /* first reserved command, 8 longwords each */
    uint32_t* next;       /* where the next command goes */
    uint32_t* limit;      /* one past the last reserved command */
    int32_t screen_w, screen_h;
    int32_t origin_x, origin_y;
    uint32_t keep_preclip;   /* nonzero: never set Pclp (SAT_SPRITE_BATCH_KEEP_PRECLIP) */
    uint32_t cull;           /* nonzero: drop sprites wholly off screen */
    uint32_t culled, dropped, no_preclip;   /* counts, as in sat_sprite_batch_stats_t */
} sat_sprite_batch_window_t;

/* Reserves room for up to `max_commands` commands at the end of the current list (fewer when the
 * list has less room: win->limit says how many; sat_sprite_batch_emit counts the rest as dropped).
 * options: SAT_SPRITE_BATCH_KEEP_PRECLIP, SAT_SPRITE_BATCH_NO_CULL. SAT_ERR_NOT_INITIALIZED outside
 * a frame, SAT_ERR_INVALID_ARG for an unknown option. */
sat_result_t sat_sprite_batch_begin(sat_sprite_batch_window_t* win, uint32_t max_commands, uint32_t options);

/* Appends what was emitted to the list and releases the window. stats may be NULL. Returns
 * SAT_ERR_CAPACITY when sprites were dropped for lack of room, SAT_OK otherwise. */
sat_result_t sat_sprite_batch_end(const sat_sprite_batch_window_t* win, sat_sprite_batch_stats_t* stats);

/* VDP1 command words as the SH-2 stores them: two command halfwords per longword, first one high. */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define SAT_SPRITE_BATCH_PAIR(first, second) ((((uint32_t)(second)) << 16) | (((uint32_t)(first)) & 0xFFFFu))
#else
#define SAT_SPRITE_BATCH_PAIR(first, second) ((((uint32_t)(first)) << 16) | (((uint32_t)(second)) & 0xFFFFu))
#endif

/* Writes one sprite: `frame` at screen position (x, y), its pivot. Returns 1 when a command was
 * written, 0 when the sprite was culled or the window is full. The frame must be a valid
 * sat_sprite_frame_t (no index to check here). */
static inline int sat_sprite_batch_emit(sat_sprite_batch_window_t* win, const sat_sprite_frame_t* frame,
                                        int32_t x, int32_t y) {
    const int32_t left = x - frame->pivot_x;
    const int32_t top = y - frame->pivot_y;
    const uint32_t size = frame->size;
    const int32_t width = (int32_t)(size >> 8) << 3;
    const int32_t height = (int32_t)(size & 0xFFu);
    const int inside = (left | top | (win->screen_w - width - left) | (win->screen_h - height - top)) >= 0;
    if (!inside && win->cull &&
        (left + width <= 0 || top + height <= 0 || left >= win->screen_w || top >= win->screen_h)) {
        ++win->culled;
        return 0;
    }
    if (win->next >= win->limit) {
        ++win->dropped;
        return 0;
    }
    const uint32_t* const fw = (const uint32_t*)(const void*)frame;
    uint32_t w1 = fw[1];
    /* Pclp (CMDPMOD bit 11): small sprites wholly on screen skip the VDP1's per-line pre-clip test
     * (manual 6.3), except flipped ones (the bit forbids it) and user-clipped ones (they may cross
     * the user rectangle). */
    if (inside && !win->keep_preclip && (frame->ctrl & 0x0010u) == 0u && (frame->pmod & 0x0400u) == 0u) {
        w1 |= SAT_SPRITE_BATCH_PAIR(0x0800u, 0u);
        ++win->no_preclip;
    }
    uint32_t* const w = win->next;
    w[0] = fw[0];
    w[1] = w1;
    w[2] = fw[2];
    w[3] = SAT_SPRITE_BATCH_PAIR((uint16_t)(left - win->origin_x), (uint16_t)(top - win->origin_y));
    win->next = w + 8;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif
