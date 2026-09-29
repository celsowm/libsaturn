#ifndef IKEMEN_ANIM_H
#define IKEMEN_ANIM_H

#include <stdint.h>

/* Frame table contract shared by the generated Ikemen assets
 * (tools/ikemen_sff emit) and the runtime. The player owns no clock:
 * callers sample frames by elapsed state ticks, so animation stays in
 * lock-step with the fight simulation and stays trivially testable.
 *
 * Table layout: frames sorted by (action, index); every action is a
 * contiguous run. pixel_ofs indexes the generated pixel blob and is
 * never dereferenced here (the platform layer owns the blob). */

typedef struct ik_frame {
    uint16_t action;
    uint16_t index;
    uint16_t w;
    uint16_t h;
    int16_t ax;   /* sprite axis inside the sprite, AIR/SFF semantics */
    int16_t ay;
    uint16_t ticks;    /* AIR display time; 0 or -1 (65535) mean "hold" */
    uint16_t flags;    /* bit0 flip_h, bit1 flip_v (baked from AIR) */
    uint32_t pixel_ofs;
} ik_frame_t;

typedef struct ik_frame_table {
    const ik_frame_t* frames;
    uint32_t count;
} ik_frame_table_t;

#define IK_FRAME_FLAG_FLIP_H 1u
#define IK_FRAME_FLAG_FLIP_V 2u

#ifdef __cplusplus
extern "C" {
#endif

/* Locates one action's contiguous frame run. Returns 0 when absent. */
int ik_frames_bounds(const ik_frame_table_t* table, int action,
                     uint32_t* out_first, uint32_t* out_count);

/* Effective display time of a frame (>= 1 tick; 0 and 65535 hold forever
 * by staying on the frame, which wrapping handles by never passing it). */
uint16_t ik_frame_ticks(const ik_frame_t* frame);

/* Samples the frame a fighter would display after `ticks` inside
 * `action`, honouring per-frame display times and wrapping at the end
 * (hold frames stop advancing). Returns NULL for an absent action. */
const ik_frame_t* ik_frame_at_time(const ik_frame_table_t* table,
                                    int action, uint32_t ticks);

/* Screen-space top-left for drawing `frame` anchored at world (x, y)
 * with `facing` (+1 right, -1 left): the sprite axis lands on (x, y),
 * mirroring horizontally when facing left or when the frame baked an
 * AIR H flip. dy also honors a baked V flip (axis mirrors vertically). */
void ik_frame_screen_anchor(const ik_frame_t* frame,
                            int x, int y, int facing,
                            int16_t* out_dx, int16_t* out_dy);

#ifdef __cplusplus
}
#endif

#endif /* IKEMEN_ANIM_H */
