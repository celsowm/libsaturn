#include "ikemen_anim.h"

int ik_frames_bounds(const ik_frame_table_t* table, int action,
                     uint32_t* out_first, uint32_t* out_count) {
    if (table == 0 || table->frames == 0 || table->count == 0u) return 0;
    uint32_t first = 0u;
    int found = 0;
    for (uint32_t i = 0u; i < table->count; ++i) {
        if ((int)table->frames[i].action == action) {
            if (!found) {
                first = i;
                found = 1;
            }
        } else if (found) {
            break;
        }
    }
    if (!found) return 0;
    if (out_first != 0) *out_first = first;
    if (out_count != 0) {
        uint32_t last = first;
        while (last + 1u < table->count &&
               (int)table->frames[last + 1u].action == action) {
            ++last;
        }
        *out_count = last - first + 1u;
    }
    return 1;
}

uint16_t ik_frame_ticks(const ik_frame_t* frame) {
    if (frame == 0) return 1u;
    if (frame->ticks == 0u || frame->ticks == 65535u) return 0u;
    return frame->ticks;
}

const ik_frame_t* ik_frame_at_time(const ik_frame_table_t* table,
                                   int action, uint32_t ticks) {
    uint32_t first = 0u;
    uint32_t count = 0u;
    if (!ik_frames_bounds(table, action, &first, &count)) return 0;
    uint32_t i = 0u;
    uint32_t remaining = ticks;
    for (;;) {
        const ik_frame_t* frame = &table->frames[first + i];
        const uint16_t duration = ik_frame_ticks(frame);
        if (duration == 0u) return frame;
        if (remaining < duration) return frame;
        remaining -= duration;
        i += 1u;
        if (i >= count) i = 0u;
    }
}

uint32_t ik_action_duration_ticks(const ik_frame_table_t* table, int action) {
    uint32_t first = 0u;
    uint32_t count = 0u;
    if (!ik_frames_bounds(table, action, &first, &count)) return 0u;
    uint32_t total = 0u;
    for (uint32_t i = 0u; i < count; ++i) {
        const uint16_t ticks = ik_frame_ticks(&table->frames[first + i]);
        if (ticks == 0u) return 0u;
        total += ticks;
    }
    return total;
}

void ik_frame_screen_anchor(const ik_frame_t* frame,
                            int x, int y, int facing,
                            int16_t* out_dx, int16_t* out_dy) {
    if (frame == 0 || out_dx == 0 || out_dy == 0) return;
    const int flip_h = (facing < 0) !=
                       ((frame->flags & IK_FRAME_FLAG_FLIP_H) != 0u);
    const int flip_v = (frame->flags & IK_FRAME_FLAG_FLIP_V) != 0u;
    const int axis_x = flip_h ? (int)frame->w - (int)frame->ax : frame->ax;
    const int axis_y = flip_v ? (int)frame->h - (int)frame->ay : frame->ay;
    *out_dx = (int16_t)(x - axis_x);
    *out_dy = (int16_t)(y - axis_y);
}

uint16_t ik_frame_clsn_count(const ik_frame_t* frame, uint8_t kind) {
    if (frame == 0) return 0u;
    if (kind == IK_CLSN_ATTACK) return frame->clsn1_count;
    if (kind == IK_CLSN_HURT) return frame->clsn2_count;
    return 0u;
}

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
                        int* out_bottom) {
    if (table == 0 || frame == 0 || table->clsn_boxes == 0) return 0;

    uint32_t ofs = 0u;
    uint16_t count = 0u;
    if (kind == IK_CLSN_ATTACK) {
        ofs = frame->clsn1_ofs;
        count = frame->clsn1_count;
    } else if (kind == IK_CLSN_HURT) {
        ofs = frame->clsn2_ofs;
        count = frame->clsn2_count;
    } else {
        return 0;
    }
    if (index >= count || ofs + index >= table->clsn_box_count) return 0;

    const ik_clsn_box_t* box = &table->clsn_boxes[ofs + index];
    int l = box->left;
    int t = box->top;
    int r = box->right;
    int b = box->bottom;
    if (l > r) { const int tmp = l; l = r; r = tmp; }
    if (t > b) { const int tmp = t; t = b; b = tmp; }

    const int flip_h = (facing < 0) !=
                       ((frame->flags & IK_FRAME_FLAG_FLIP_H) != 0u);
    const int flip_v = (frame->flags & IK_FRAME_FLAG_FLIP_V) != 0u;

    int wl, wr, wt, wb;
    if (flip_h) {
        wl = x - r;
        wr = x - l;
    } else {
        wl = x + l;
        wr = x + r;
    }
    if (flip_v) {
        wt = y - b;
        wb = y - t;
    } else {
        wt = y + t;
        wb = y + b;
    }

    if (out_left) *out_left = wl;
    if (out_top) *out_top = wt;
    if (out_right) *out_right = wr;
    if (out_bottom) *out_bottom = wb;
    return 1;
}
