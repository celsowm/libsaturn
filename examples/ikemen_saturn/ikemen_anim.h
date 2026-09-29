#ifndef IKEMEN_ANIM_H
#define IKEMEN_ANIM_H

#include <stdint.h>

typedef struct ik_clsn_box {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
} ik_clsn_box_t;

typedef struct ik_frame {
    uint16_t action;
    uint16_t index;
    uint16_t w;
    uint16_t h;
    int16_t ax;
    int16_t ay;
    uint16_t ticks;
    uint16_t flags;
    uint32_t pixel_ofs;
    uint16_t clsn1_ofs;
    uint16_t clsn1_count;
    uint16_t clsn2_ofs;
    uint16_t clsn2_count;
} ik_frame_t;

typedef struct ik_frame_table {
    const ik_frame_t* frames;
    uint32_t count;
    const ik_clsn_box_t* clsn_boxes;
    uint32_t clsn_box_count;
} ik_frame_table_t;

#define IK_FRAME_FLAG_FLIP_H 1u
#define IK_FRAME_FLAG_FLIP_V 2u
#define IK_CLSN_ATTACK 1u
#define IK_CLSN_HURT 2u

#ifdef __cplusplus
extern "C" {
#endif

int ik_frames_bounds(const ik_frame_table_t* table, int action,
                     uint32_t* out_first, uint32_t* out_count);
uint16_t ik_frame_ticks(const ik_frame_t* frame);
const ik_frame_t* ik_frame_at_time(const ik_frame_table_t* table,
                                   int action, uint32_t ticks);
uint32_t ik_action_duration_ticks(const ik_frame_table_t* table, int action);

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

#ifdef __cplusplus
}
#endif

#endif
