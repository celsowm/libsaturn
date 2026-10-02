#ifndef IKEMEN_ANIM_H
#define IKEMEN_ANIM_H

#include <stdint.h>

typedef struct ik_clsn_box {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
} ik_clsn_box_t;

typedef struct ik_sprite_source {
    uint32_t data_ofs;
    uint32_t data_size;
    uint16_t source_w;
    uint16_t source_h;
    uint16_t padded_w;
    uint8_t left_pad;
    uint8_t format;
    uint16_t palette_index;
    int16_t group;
    int16_t number;
    int16_t xoff;
    int16_t yoff;
} ik_sprite_source_t;

typedef struct ik_frame {
    uint16_t action;
    uint16_t index;
    uint16_t w;
    uint16_t h;
    int16_t ax;
    int16_t ay;
    uint16_t ticks;
    uint16_t flags;
    uint16_t sprite_index;
    uint16_t clsn1_ofs;
    uint16_t clsn1_count;
    uint16_t clsn2_ofs;
    uint16_t clsn2_count;
    int16_t air_x;
    int16_t air_y;
} ik_frame_t;

typedef struct ik_frame_table {
    const ik_frame_t* frames;
    uint32_t count;
    const ik_clsn_box_t* clsn_boxes;
    uint32_t clsn_box_count;
} ik_frame_table_t;

#define IK_FRAME_FLAG_FLIP_H 1u
#define IK_FRAME_FLAG_FLIP_V 2u
#define IK_FRAME_FLAG_BLEND_ADD 4u
#define IK_FRAME_FLAG_BLEND_SUBTRACT 8u
#define IK_FRAME_FLAG_LOOP_START 16u
#define IK_CLSN_ATTACK 1u
#define IK_CLSN_HURT 2u
#define IK_SPRITE_FORMAT_RAW 0u
#define IK_SPRITE_FORMAT_LZ5 4u

#ifdef __cplusplus
extern "C" {
#endif

int ik_sprite_decode(const ik_sprite_source_t* sprite,
                     const uint8_t* blob,
                     uint32_t blob_size,
                     uint8_t* output,
                     uint32_t output_capacity);

int ik_frames_bounds(const ik_frame_table_t* table, int action,
                     uint32_t* out_first, uint32_t* out_count);
uint16_t ik_frame_ticks(const ik_frame_t* frame);
const ik_frame_t* ik_frame_at_time(const ik_frame_table_t* table,
                                   int action, uint32_t ticks);
/* The frame that plays after `frame`, following ik_frame_at_time's rules:
 * the next frame of the action, the loop start after the last frame of a
 * looping action, and NULL where the animation stays put (a frame that holds
 * forever, the last frame of a non-looping action, or a pointer outside the
 * table). Cheaper than ik_frame_at_time, which scans the whole table. */
const ik_frame_t* ik_frame_after(const ik_frame_table_t* table,
                                 const ik_frame_t* frame);
/* Animation element (1-based, 0 when the action is unknown) and curtime the
 * way upstream Ikemen's Animation reports them, for a fighter whose raw
 * time-in-animation is `time`. Used by the oracle trace so a looping action
 * shows the wrapped curtime instead of an ever-growing counter. */
void ik_anim_trace_state(const ik_frame_table_t* table, int action,
                         uint32_t time, int* out_elem, uint32_t* out_time);
uint32_t ik_action_duration_ticks(const ik_frame_table_t* table, int action);
int ik_action_loops(const ik_frame_table_t* table, int action);
int ik_visual_frame_resolve(
    const ik_frame_t* animation_frame,
    const ik_sprite_source_t* animation_sprites,
    uint16_t animation_sprite_count,
    const ik_sprite_source_t* visual_sprites,
    uint16_t visual_sprite_count,
    ik_frame_t* out_frame);


void ik_frame_screen_anchor(const ik_frame_t* frame,
                            int x, int y, int facing,
                            int16_t* out_dx, int16_t* out_dy);

uint16_t ik_frame_clsn_count(const ik_frame_t* frame, uint8_t kind);
int ik_frame_clsn_world(const ik_frame_table_t* table,
                        const ik_frame_t* frame,
                        uint8_t kind,
                        uint16_t index,
                        int x,
                        int y,
                        int facing,
                        int* out_left,
                        int* out_top,
                        int* out_right,
                        int* out_bottom);

/* Same box in Q8.8 world units, for a Q8.8 position (upstream compares float
 * boxes, so a pixel-rounded position shifts a hit by a tick). */
int ik_frame_clsn_world_q8(const ik_frame_table_t* table,
                           const ik_frame_t* frame,
                           uint8_t kind,
                           uint16_t index,
                           int32_t x_q8,
                           int32_t y_q8,
                           int facing,
                           int32_t* out_left,
                           int32_t* out_top,
                           int32_t* out_right,
                           int32_t* out_bottom);

#ifdef __cplusplus
}
#endif

#endif
