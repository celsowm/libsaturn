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
    uint16_t ctrl;     /* CMDCTRL: normal sprite + Dir (flip) bits */
    uint16_t pmod;     /* CMDPMOD without the pre-clipping bit */
    uint16_t colr;     /* CMDCOLR: bank << 8 (INDEX8) or LUT address / 8 (LUT4) */
    uint16_t srca;     /* CMDSRCA: character address / 8 */
    uint16_t size;     /* CMDSIZE: (width / 8) << 8 | height */
    uint16_t width;
    uint16_t height;
    int16_t pivot_x;   /* the instance position is this texel of the frame */
    int16_t pivot_y;
} sat_sprite_frame_t;

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
 * _MESH, _FLIP_H, _FLIP_V. The pivot must lie inside the texture. */
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

#ifdef __cplusplus
}
#endif
#endif
