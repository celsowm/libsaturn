#include "saturn/sprite_clip.h"

#include <limits.h>

/* Sprite clip player: the logic behind include/saturn/sprite_clip.h. Hardware-free. */

namespace {

constexpr int64_t kOne = SAT_FX16_ONE;

inline int16_t clamp16(int32_t v) { return v > INT16_MAX ? INT16_MAX : (v < INT16_MIN ? INT16_MIN : static_cast<int16_t>(v)); }

inline const sat_clip_t* clip_of(const sat_clip_player_t& p) { return &p.set->clips[p.clip]; }
inline const sat_clip_frame_t& frame_of(const sat_clip_player_t& p) { return clip_of(p)->frames[p.frame]; }
inline int64_t frame_span(const sat_clip_frame_t& f) { return static_cast<int64_t>(f.duration) * kOne; }

void announce(const sat_clip_player_t& p, sat_clip_event_fn fn, void* user, sat_clip_step_result_t* r) {
    const uint16_t ev = frame_of(p).event;
    if (ev == 0) return;
    if (fn) fn(user, ev, p.clip, p.frame);
    if (r) ++r->events;
}

/* Moves to the frame after the current one. Returns false when a one-shot ran off its end. */
bool next_frame(sat_clip_player_t& p, uint8_t& flags) {
    const sat_clip_t& c = *clip_of(p);
    switch (c.mode) {
        case SAT_CLIP_ONCE:
            if (p.frame + 1u < c.frame_count) { ++p.frame; return true; }
            return false;
        case SAT_CLIP_LOOP:
            if (p.frame + 1u < c.frame_count) { ++p.frame; return true; }
            p.frame = c.loop_start;
            ++p.loops;
            flags |= SAT_CLIP_STEP_LOOPED;
            return true;
        default: /* ping-pong */
            if (c.frame_count == 1) { ++p.loops; flags |= SAT_CLIP_STEP_LOOPED; return true; }
            if (!p.reverse) {
                if (p.frame + 1u < c.frame_count) { ++p.frame; return true; }
                p.reverse = 1;
                --p.frame;
                return true;
            }
            if (p.frame > 0) { --p.frame; return true; }
            p.reverse = 0;
            p.frame = 1;
            ++p.loops;
            flags |= SAT_CLIP_STEP_LOOPED;
            return true;
    }
}

void begin(sat_clip_player_t& p, uint16_t clip, sat_clip_event_fn fn, void* user) {
    p.clip = clip;
    p.frame = 0;
    p.elapsed = 0;
    p.loops = 0;
    p.playing = 1;
    p.finished = 0;
    p.reverse = 0;
    announce(p, fn, user, nullptr);
}

}  // namespace

extern "C" sat_result_t sat_clip_set_validate(const sat_clip_set_t* set) {
    if (!set || (set->clip_count && !set->clips) || (set->shape_count && !set->shapes)) return SAT_ERR_INVALID_ARG;
    for (uint32_t i = 0; i < set->clip_count; ++i) {
        const sat_clip_t& c = set->clips[i];
        if (!c.frames || c.frame_count == 0 || c.mode > SAT_CLIP_PING_PONG || c.loop_start >= c.frame_count) return SAT_ERR_INVALID_ARG;
        for (uint32_t f = 0; f < c.frame_count; ++f) {
            const sat_clip_frame_t& fr = c.frames[f];
            if (fr.duration == 0 || sat_rect_empty(&fr.source)) return SAT_ERR_INVALID_ARG;
            if (static_cast<uint32_t>(fr.shape_first) + fr.shape_count > set->shape_count) return SAT_ERR_INVALID_ARG;
        }
    }
    return SAT_OK;
}

extern "C" uint32_t sat_clip_set_region_count(const sat_clip_set_t* set) {
    if (!set) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < set->clip_count; ++i) n += set->clips[i].frame_count;
    return n;
}

extern "C" sat_result_t sat_clip_player_init(sat_clip_player_t* player, const sat_clip_set_t* set) {
    if (!player || !set) return SAT_ERR_INVALID_ARG;
    *player = {};
    player->set = set;
    player->rate = SAT_FX16_ONE;
    return SAT_OK;
}

extern "C" sat_result_t sat_clip_player_play(sat_clip_player_t* player, uint16_t clip, sat_clip_switch_t how,
    sat_clip_event_fn event, void* user) {
    if (!player || !player->set || clip >= player->set->clip_count) return SAT_ERR_INVALID_ARG;
    if (how == SAT_CLIP_SWITCH_KEEP_IF_SAME && player->playing && player->clip == clip) return SAT_OK;
    if (how == SAT_CLIP_SWITCH_KEEP_PHASE && player->playing && player->clip != clip) {
        const sat_clip_t& from = *clip_of(*player);
        const sat_clip_t& to = player->set->clips[clip];
        const sat_clip_frame_t& old = frame_of(*player);
        const int64_t fraction = player->elapsed * kOne / frame_span(old); /* 16.16 of the frame done */
        uint32_t frame = static_cast<uint32_t>(static_cast<uint64_t>(player->frame) * to.frame_count / from.frame_count);
        if (frame >= to.frame_count) frame = to.frame_count - 1u;
        player->clip = clip;
        player->frame = static_cast<uint16_t>(frame);
        player->elapsed = static_cast<sat_fx16_t>(fraction * to.frames[frame].duration);
        player->loops = 0;
        player->finished = 0;
        player->reverse = 0;
        return SAT_OK;
    }
    begin(*player, clip, event, user);
    return SAT_OK;
}

extern "C" sat_result_t sat_clip_player_restart(sat_clip_player_t* player, sat_clip_event_fn event, void* user) {
    if (!player || !player->playing) return SAT_ERR_INVALID_ARG;
    begin(*player, player->clip, event, user);
    return SAT_OK;
}

extern "C" sat_result_t sat_clip_player_seek(sat_clip_player_t* player, uint16_t frame, sat_clip_event_fn event, void* user) {
    if (!player || !player->playing || frame >= clip_of(*player)->frame_count) return SAT_ERR_INVALID_ARG;
    player->frame = frame;
    player->elapsed = 0;
    player->finished = 0;
    announce(*player, event, user, nullptr);
    return SAT_OK;
}

extern "C" sat_result_t sat_clip_player_set_rate(sat_clip_player_t* player, sat_fx16_t rate) {
    if (!player || rate < 0) return SAT_ERR_INVALID_ARG;
    player->rate = rate;
    return SAT_OK;
}

extern "C" sat_result_t sat_clip_player_pause(sat_clip_player_t* player) {
    if (!player) return SAT_ERR_INVALID_ARG;
    player->paused = 1;
    return SAT_OK;
}

extern "C" sat_result_t sat_clip_player_resume(sat_clip_player_t* player) {
    if (!player) return SAT_ERR_INVALID_ARG;
    player->paused = 0;
    return SAT_OK;
}

extern "C" sat_result_t sat_clip_player_advance(sat_clip_player_t* player, sat_fx16_t ticks, sat_clip_event_fn event, void* user,
    sat_clip_step_result_t* out) {
    if (!player || ticks < 0) return SAT_ERR_INVALID_ARG;
    sat_clip_step_result_t r = {};
    if (player->playing && !player->paused && !player->finished) {
        int64_t t = static_cast<int64_t>(player->elapsed) + ((static_cast<int64_t>(ticks) * player->rate) >> 16);
        for (;;) {
            const int64_t span = frame_span(frame_of(*player));
            if (t < span) break;
            t -= span;
            if (!next_frame(*player, r.flags)) {
                player->finished = 1;
                r.flags |= SAT_CLIP_STEP_FINISHED;
                t = 0;
                break;
            }
            ++r.frames_advanced;
            r.flags |= SAT_CLIP_STEP_FRAME_CHANGED;
            announce(*player, event, user, &r);
        }
        player->elapsed = static_cast<sat_fx16_t>(t);
    }
    if (out) *out = r;
    return SAT_OK;
}

extern "C" sat_result_t sat_clip_player_step(sat_clip_player_t* player, sat_clip_event_fn event, void* user,
    sat_clip_step_result_t* out) {
    return sat_clip_player_advance(player, SAT_FX16_ONE, event, user, out);
}

extern "C" int sat_clip_player_is_playing(const sat_clip_player_t* player) { return player && player->playing ? 1 : 0; }
extern "C" int sat_clip_player_is_paused(const sat_clip_player_t* player) { return player && player->paused ? 1 : 0; }
extern "C" int sat_clip_player_is_finished(const sat_clip_player_t* player) { return player && player->finished ? 1 : 0; }
extern "C" uint16_t sat_clip_player_clip(const sat_clip_player_t* player) { return player ? player->clip : 0; }
extern "C" uint16_t sat_clip_player_frame_index(const sat_clip_player_t* player) { return player ? player->frame : 0; }

extern "C" const sat_clip_frame_t* sat_clip_player_frame(const sat_clip_player_t* player) {
    return player && player->playing ? &frame_of(*player) : nullptr;
}

extern "C" const sat_clip_shape_t* sat_clip_frame_shapes(const sat_clip_set_t* set, const sat_clip_frame_t* frame, uint32_t* out_count) {
    if (out_count) *out_count = 0;
    if (!set || !frame || frame->shape_count == 0 || static_cast<uint32_t>(frame->shape_first) + frame->shape_count > set->shape_count) return nullptr;
    if (out_count) *out_count = frame->shape_count;
    return set->shapes + frame->shape_first;
}

extern "C" int sat_clip_frame_find_shape(const sat_clip_set_t* set, const sat_clip_frame_t* frame, uint8_t kind, uint8_t index,
    sat_clip_shape_t* out) {
    uint32_t n = 0;
    const sat_clip_shape_t* s = sat_clip_frame_shapes(set, frame, &n);
    for (uint32_t i = 0; i < n; ++i) {
        if (s[i].kind != kind || (index != 0xFFu && s[i].index != index)) continue;
        if (out) *out = s[i];
        return 1;
    }
    return 0;
}

extern "C" sat_result_t sat_clip_frame_dest(const sat_clip_frame_t* frame, int16_t x, int16_t y, uint8_t flip, sat_rect_t* out) {
    if (!frame || !out) return SAT_ERR_INVALID_ARG;
    const int32_t w = frame->source.width, h = frame->source.height;
    const int32_t left = (flip & SAT_CLIP_FLIP_X) ? x - (w - frame->pivot_x) : x - frame->pivot_x;
    const int32_t top = (flip & SAT_CLIP_FLIP_Y) ? y - (h - frame->pivot_y) : y - frame->pivot_y;
    out->x = clamp16(left);
    out->y = clamp16(top);
    out->width = frame->source.width;
    out->height = frame->source.height;
    return SAT_OK;
}

extern "C" sat_result_t sat_clip_shape_rect(const sat_clip_shape_t* shape, int16_t x, int16_t y, uint8_t flip, sat_rect_t* out) {
    if (!shape || !out) return SAT_ERR_INVALID_ARG;
    const int32_t left = (flip & SAT_CLIP_FLIP_X) ? x - (shape->x + static_cast<int32_t>(shape->w)) : x + shape->x;
    const int32_t top = (flip & SAT_CLIP_FLIP_Y) ? y - (shape->y + static_cast<int32_t>(shape->h)) : y + shape->y;
    out->x = clamp16(left);
    out->y = clamp16(top);
    out->width = shape->w;
    out->height = shape->h;
    return SAT_OK;
}

extern "C" sat_point_t sat_clip_shape_point(const sat_clip_shape_t* shape, int16_t x, int16_t y, uint8_t flip) {
    if (!shape) return {x, y};
    return {clamp16((flip & SAT_CLIP_FLIP_X) ? x - shape->x : x + shape->x),
            clamp16((flip & SAT_CLIP_FLIP_Y) ? y - shape->y : y + shape->y)};
}
