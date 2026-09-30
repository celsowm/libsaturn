#include "ikemen_anim.h"

static int decode_lz5(const uint8_t* source,
                      uint32_t source_size,
                      uint8_t* output,
                      uint32_t output_size) {
    if (!source || !output || source_size < 5u) return 0;

    uint32_t pos = 4u;
    uint32_t written = 0u;
    uint8_t control = source[pos++];
    uint8_t control_bit = 0u;
    uint8_t rebuilt_bits = 0u;
    uint16_t rebuilt_distance = 0u;

    while (written < output_size) {
        if (pos >= source_size) return 0;
        uint16_t code = source[pos++];

        if ((control & (uint8_t)(1u << control_bit)) != 0u) {
            uint32_t distance;
            uint32_t count;

            if ((code & 0x3Fu) == 0u) {
                if (pos + 1u >= source_size) return 0;
                distance = ((uint32_t)code << 2) | source[pos++];
                distance += 1u;
                count = (uint32_t)source[pos++] + 3u;
            } else {
                rebuilt_distance |=
                    (uint16_t)((code & 0xC0u) >> rebuilt_bits);
                rebuilt_bits = (uint8_t)(rebuilt_bits + 2u);
                count = (uint32_t)(code & 0x3Fu) + 1u;
                if (rebuilt_bits < 8u) {
                    if (pos >= source_size) return 0;
                    distance = (uint32_t)source[pos++] + 1u;
                } else {
                    distance = (uint32_t)rebuilt_distance + 1u;
                    rebuilt_distance = 0u;
                    rebuilt_bits = 0u;
                }
            }

            if (distance == 0u || distance > written) return 0;
            while (count != 0u && written < output_size) {
                output[written] = output[written - distance];
                ++written;
                --count;
            }
        } else {
            uint32_t count;
            uint8_t value;
            if ((code & 0xE0u) == 0u) {
                if (pos >= source_size) return 0;
                count = (uint32_t)source[pos++] + 8u;
                value = (uint8_t)code;
            } else {
                count = (uint32_t)(code >> 5);
                value = (uint8_t)(code & 0x1Fu);
            }
            while (count != 0u && written < output_size) {
                output[written++] = value;
                --count;
            }
        }

        ++control_bit;
        if (control_bit >= 8u && written < output_size) {
            if (pos >= source_size) return 0;
            control = source[pos++];
            control_bit = 0u;
        }
    }
    return 1;
}

static void pad_sprite_in_place(uint8_t* pixels,
                                uint16_t width,
                                uint16_t height,
                                uint16_t padded_width,
                                uint8_t left_pad) {
    if (width == padded_width && left_pad == 0u) return;

    for (uint32_t row = height; row != 0u; --row) {
        const uint32_t source_ofs = (row - 1u) * width;
        const uint32_t dest_ofs = (row - 1u) * padded_width;

        for (uint32_t x = width; x != 0u; --x) {
            pixels[dest_ofs + left_pad + x - 1u] =
                pixels[source_ofs + x - 1u];
        }
        for (uint32_t x = 0u; x < left_pad; ++x) {
            pixels[dest_ofs + x] = 0u;
        }
        for (uint32_t x = (uint32_t)left_pad + width;
             x < padded_width; ++x) {
            pixels[dest_ofs + x] = 0u;
        }
    }
}

int ik_sprite_decode(const ik_sprite_source_t* sprite,
                     const uint8_t* blob,
                     uint32_t blob_size,
                     uint8_t* output,
                     uint32_t output_capacity) {
    if (!sprite || !blob || !output ||
        sprite->source_w == 0u || sprite->source_h == 0u ||
        sprite->padded_w < sprite->source_w ||
        (sprite->padded_w & 7u) != 0u ||
        (uint32_t)sprite->left_pad + sprite->source_w > sprite->padded_w) {
        return 0;
    }
    if (sprite->data_ofs > blob_size ||
        sprite->data_size > blob_size - sprite->data_ofs) {
        return 0;
    }

    const uint32_t raw_bytes =
        (uint32_t)sprite->source_w * sprite->source_h;
    const uint32_t padded_bytes =
        (uint32_t)sprite->padded_w * sprite->source_h;
    if (padded_bytes > output_capacity) return 0;

    const uint8_t* source = blob + sprite->data_ofs;
    if (sprite->format == IK_SPRITE_FORMAT_RAW) {
        if (sprite->data_size < raw_bytes) return 0;
        for (uint32_t i = 0u; i < raw_bytes; ++i) output[i] = source[i];
    } else if (sprite->format == IK_SPRITE_FORMAT_LZ5) {
        if (!decode_lz5(source, sprite->data_size, output, raw_bytes)) return 0;
    } else {
        return 0;
    }

    pad_sprite_in_place(
        output, sprite->source_w, sprite->source_h,
        sprite->padded_w, sprite->left_pad);
    return 1;
}

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

int ik_action_loops(const ik_frame_table_t* table, int action) {
    uint32_t first = 0u;
    uint32_t count = 0u;
    if (!ik_frames_bounds(table, action, &first, &count)) return 0;
    for (uint32_t i = 0u; i < count; ++i) {
        if ((table->frames[first + i].flags &
             IK_FRAME_FLAG_LOOP_START) != 0u) {
            return 1;
        }
    }
    return 0;
}

const ik_frame_t* ik_frame_at_time(const ik_frame_table_t* table,
                                   int action, uint32_t ticks) {
    uint32_t first = 0u;
    uint32_t count = 0u;
    if (!ik_frames_bounds(table, action, &first, &count) ||
        count == 0u) {
        return 0;
    }

    uint32_t loop = count;
    for (uint32_t i = 0u; i < count; ++i) {
        if ((table->frames[first + i].flags &
             IK_FRAME_FLAG_LOOP_START) != 0u) {
            loop = i;
            break;
        }
    }

    uint32_t remaining = ticks;
    for (uint32_t i = 0u; i < count; ++i) {
        const ik_frame_t* frame = &table->frames[first + i];
        const uint16_t duration = ik_frame_ticks(frame);
        if (duration == 0u || remaining < duration) return frame;
        remaining -= duration;
    }

    if (loop >= count) {
        return &table->frames[first + count - 1u];
    }

    uint32_t loop_ticks = 0u;
    for (uint32_t i = loop; i < count; ++i) {
        const uint16_t duration =
            ik_frame_ticks(&table->frames[first + i]);
        if (duration == 0u) return &table->frames[first + i];
        loop_ticks += duration;
    }
    if (loop_ticks == 0u) {
        return &table->frames[first + count - 1u];
    }

    remaining %= loop_ticks;
    for (uint32_t i = loop; i < count; ++i) {
        const ik_frame_t* frame = &table->frames[first + i];
        const uint16_t duration = ik_frame_ticks(frame);
        if (remaining < duration) return frame;
        remaining -= duration;
    }
    return &table->frames[first + count - 1u];
}

uint32_t ik_action_duration_ticks(const ik_frame_table_t* table, int action) {
    uint32_t first = 0u;
    uint32_t count = 0u;
    if (!ik_frames_bounds(table, action, &first, &count)) return 0u;
    uint32_t total = 0u;
    for (uint32_t i = 0u; i < count; ++i) {
        const ik_frame_t* frame = &table->frames[first + i];
        if ((frame->flags & IK_FRAME_FLAG_LOOP_START) != 0u) return 0u;
        const uint16_t ticks = ik_frame_ticks(frame);
        if (ticks == 0u) return 0u;
        if (0xFFFFFFFFu - total < ticks) return 0xFFFFFFFFu;
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
