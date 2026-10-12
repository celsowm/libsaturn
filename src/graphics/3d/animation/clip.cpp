#include "saturn/skeleton3d.h"

namespace {

constexpr int32_t kOne = 65536;

/* Seconds (16.16) to frames (16.16): time * num / den. */
int32_t to_frames(sat_fx16_t time, const sat_skeleton_clip_t& c) {
    return sat_fx16_mul(time, (sat_fx16_t)((int32_t)c.rate_num << 16)) / (int32_t)c.rate_den;
}

/* Frames (16.16) to seconds (16.16): frames * den / num. */
sat_fx16_t to_seconds(int32_t frames, const sat_skeleton_clip_t& c) {
    return (sat_fx16_t)(((int64_t)frames * c.rate_den) / c.rate_num);
}

bool loops(const sat_skeleton_clip_t& c) { return (c.flags & SAT_CLIP_FLAG_LOOP) != 0; }

const sat_skeleton_pose_joint_t* frame_at(const sat_skeleton_clip_t& c, uint32_t f) {
    return c.frames + (size_t)f * c.joint_count;
}

sat_vec3_t lerp3(const sat_vec3_t& a, const sat_vec3_t& b, int32_t t) {
    return {a.x + sat_fx16_mul(b.x - a.x, t), a.y + sat_fx16_mul(b.y - a.y, t),
            a.z + sat_fx16_mul(b.z - a.z, t)};
}

/* Sampled root translation along frames [0, frame_count-1] only (no
 * wrap segment), used for root travel. fpos is in 16.16 frames. */
sat_vec3_t clamped_root(const sat_skeleton_clip_t& c, int32_t fpos) {
    const int32_t last = ((int32_t)c.frame_count - 1) << 16;
    if (fpos <= 0) return frame_at(c, 0)[0].translation;
    if (fpos >= last) return frame_at(c, c.frame_count - 1)[0].translation;
    const uint32_t i = (uint32_t)fpos >> 16;
    return lerp3(frame_at(c, i)[0].translation, frame_at(c, i + 1)[0].translation,
                 fpos & 0xffff);
}

}  // namespace

extern "C" sat_result_t sat_clip_validate(
    const sat_skeleton_clip_t* clip, const sat_skeleton_asset_t* skeleton) {
    if (!clip || !clip->frames || clip->frame_count == 0 || clip->joint_count == 0 ||
        clip->rate_num == 0 || clip->rate_num > 32767 || clip->rate_den == 0)
        return SAT_ERR_INVALID_ARG;
    if (skeleton && clip->joint_count != skeleton->joint_count) return SAT_ERR_INVALID_ARG;
    if (clip->event_count && !clip->events) return SAT_ERR_INVALID_ARG;
    for (uint16_t i = 0; i < clip->event_count; ++i) {
        if (clip->events[i].frame >= clip->frame_count) return SAT_ERR_INVALID_ARG;
        if (i && clip->events[i].frame < clip->events[i - 1].frame) return SAT_ERR_INVALID_ARG;
    }
    return SAT_OK;
}

extern "C" sat_fx16_t sat_clip_duration(const sat_skeleton_clip_t* clip) {
    if (!clip || !clip->frames || clip->rate_num == 0 || clip->rate_den == 0) return 0;
    const int32_t frames = loops(*clip) ? clip->frame_count : clip->frame_count - 1;
    return to_seconds(frames << 16, *clip);
}

extern "C" sat_result_t sat_clip_sample(
    const sat_skeleton_clip_t* clip, sat_fx16_t time,
    sat_skeleton_pose_joint_t* out, uint16_t capacity) {
    SAT_TRY(sat_clip_validate(clip, nullptr));
    if (!out) return SAT_ERR_INVALID_ARG;
    if (capacity < clip->joint_count) return SAT_ERR_CAPACITY;
    const int32_t period = (int32_t)clip->frame_count << 16;
    int32_t fpos = time <= 0 ? 0 : to_frames(time, *clip);
    uint32_t i0, i1;
    int32_t frac;
    if (loops(*clip)) {
        fpos %= period;
        i0 = (uint32_t)fpos >> 16;
        i1 = (i0 + 1 == clip->frame_count) ? 0 : i0 + 1;
        frac = fpos & 0xffff;
    } else if (fpos >= period - kOne) {
        i0 = i1 = clip->frame_count - 1u;
        frac = 0;
    } else {
        i0 = (uint32_t)fpos >> 16;
        i1 = i0 + 1;
        frac = fpos & 0xffff;
    }
    const sat_skeleton_pose_joint_t* a = frame_at(*clip, i0);
    const sat_skeleton_pose_joint_t* b = frame_at(*clip, i1);
    for (uint16_t j = 0; j < clip->joint_count; ++j) {
        if (frac == 0) {
            out[j] = a[j];
        } else {
            out[j].translation = lerp3(a[j].translation, b[j].translation, frac);
            sat_quat16_nlerp(&out[j].rotation, &a[j].rotation, &b[j].rotation, frac);
        }
    }
    const sat_vec3_t base = frame_at(*clip, 0)[0].translation;
    if (clip->flags & SAT_CLIP_ROOT_X) out[0].translation.x = base.x;
    if (clip->flags & SAT_CLIP_ROOT_Y) out[0].translation.y = base.y;
    if (clip->flags & SAT_CLIP_ROOT_Z) out[0].translation.z = base.z;
    return SAT_OK;
}

extern "C" sat_result_t sat_skel_clip_player_init(
    sat_skel_clip_player_t* player, const sat_skeleton_clip_t* clip) {
    if (!player) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_clip_validate(clip, nullptr));
    player->clip = clip;
    player->time = 0;
    player->speed = kOne;
    player->flags = 0;
    player->reserved = 0;
    return SAT_OK;
}

extern "C" sat_result_t sat_skel_clip_player_set_clip(
    sat_skel_clip_player_t* player, const sat_skeleton_clip_t* clip, sat_fx16_t start_time) {
    if (!player || start_time < 0) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_clip_validate(clip, nullptr));
    player->clip = clip;
    player->time = start_time;
    player->flags &= (uint16_t)~SAT_CLIP_PLAYER_FINISHED;
    return SAT_OK;
}

extern "C" sat_result_t sat_skel_clip_player_set_speed(sat_skel_clip_player_t* player, sat_fx16_t speed) {
    if (!player || speed < 0) return SAT_ERR_INVALID_ARG;
    player->speed = speed;
    return SAT_OK;
}

extern "C" void sat_skel_clip_player_set_paused(sat_skel_clip_player_t* player, int paused) {
    if (!player) return;
    if (paused) player->flags |= SAT_CLIP_PLAYER_PAUSED;
    else player->flags &= (uint16_t)~SAT_CLIP_PLAYER_PAUSED;
}

extern "C" sat_result_t sat_skel_clip_player_advance(
    sat_skel_clip_player_t* player, sat_fx16_t dt, sat_clip_event_t* events_out,
    uint16_t event_capacity, uint16_t* event_count_out, sat_vec3_t* root_delta_out) {
    if (!player || !player->clip || dt < 0) return SAT_ERR_INVALID_ARG;
    const sat_skeleton_clip_t& c = *player->clip;
    SAT_TRY(sat_clip_validate(&c, nullptr));
    if (event_count_out) *event_count_out = 0;
    if (root_delta_out) *root_delta_out = {0, 0, 0};
    if (player->flags & (SAT_CLIP_PLAYER_PAUSED | SAT_CLIP_PLAYER_FINISHED)) return SAT_OK;

    const int32_t period = (int32_t)c.frame_count << 16;
    const int32_t last = period - kOne;
    const int32_t a = to_frames(player->time, c);
    const int32_t step = to_frames(sat_fx16_mul(dt, player->speed), c);
    const bool loop = loops(c);
    int32_t b = a + step; /* unwrapped new position */
    bool finished = false;
    uint32_t cycles = 0;
    if (loop) {
        cycles = (uint32_t)b / (uint32_t)period;
        b -= (int32_t)cycles * period;
    } else if (b >= last) {
        b = last;
        finished = true;
    }
    const int32_t ia = a >> 16;

    /* Events crossed: integer frame K (counting wraps) with
     * floor(a) < K <= floor(b_unwrapped). Counted first so a capacity error
     * leaves the player untouched. */
    uint32_t found = 0;
    if (c.event_count) {
        const int32_t ib_unwrapped = (b >> 16) + (int32_t)cycles * (int32_t)c.frame_count;
        for (int pass = 0; pass < 2; ++pass) {
            uint32_t n = 0;
            for (uint32_t cy = 0; cy <= cycles; ++cy) {
                for (uint16_t e = 0; e < c.event_count; ++e) {
                    const int32_t K = (int32_t)c.events[e].frame + (int32_t)cy * c.frame_count;
                    if (K > ia && K <= ib_unwrapped) {
                        if (pass == 1 && events_out) events_out[n] = c.events[e];
                        ++n;
                    }
                }
            }
            if (pass == 0) {
                found = n;
                if (events_out && n > event_capacity) return SAT_ERR_CAPACITY;
                if (!events_out) break;
            }
        }
    }

    if (root_delta_out) {
        const sat_vec3_t p0 = clamped_root(c, a);
        const sat_vec3_t p1 = clamped_root(c, b);
        sat_vec3_t d = {p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
        if (cycles) {
            const sat_vec3_t f = frame_at(c, 0)[0].translation;
            const sat_vec3_t l = frame_at(c, c.frame_count - 1)[0].translation;
            d.x += (sat_fx16_t)cycles * (l.x - f.x);
            d.y += (sat_fx16_t)cycles * (l.y - f.y);
            d.z += (sat_fx16_t)cycles * (l.z - f.z);
        }
        /* Axes that are not extracted stay in the pose, not in the delta. */
        if (!(c.flags & SAT_CLIP_ROOT_X)) d.x = 0;
        if (!(c.flags & SAT_CLIP_ROOT_Y)) d.y = 0;
        if (!(c.flags & SAT_CLIP_ROOT_Z)) d.z = 0;
        *root_delta_out = d;
    }

    player->time = to_seconds(b, c);
    if (finished) player->flags |= SAT_CLIP_PLAYER_FINISHED;
    if (event_count_out) *event_count_out = (uint16_t)(found > 0xffffu ? 0xffffu : found);
    return SAT_OK;
}

extern "C" sat_result_t sat_skel_clip_player_sample(
    const sat_skel_clip_player_t* player, sat_skeleton_pose_joint_t* out, uint16_t capacity) {
    if (!player || !player->clip) return SAT_ERR_INVALID_ARG;
    return sat_clip_sample(player->clip, player->time, out, capacity);
}

extern "C" sat_result_t sat_pose_fade_init(
    sat_pose_fade_t* fade, sat_skeleton_pose_joint_t* storage, uint16_t joint_count) {
    if (!fade || !storage || joint_count == 0) return SAT_ERR_INVALID_ARG;
    fade->from = storage;
    fade->joint_count = joint_count;
    fade->reserved = 0;
    fade->duration = 0;
    fade->elapsed = 0;
    return SAT_OK;
}

extern "C" sat_result_t sat_pose_fade_begin(
    sat_pose_fade_t* fade, const sat_skeleton_pose_joint_t* current, sat_fx16_t duration) {
    if (!fade || !fade->from || !current || duration < 0) return SAT_ERR_INVALID_ARG;
    for (uint16_t i = 0; i < fade->joint_count; ++i) fade->from[i] = current[i];
    fade->duration = duration;
    fade->elapsed = 0;
    return SAT_OK;
}

extern "C" void sat_pose_fade_advance(sat_pose_fade_t* fade, sat_fx16_t dt) {
    if (!fade || dt < 0) return;
    fade->elapsed += dt;
    if (fade->elapsed > fade->duration) fade->elapsed = fade->duration;
}

extern "C" int sat_pose_fade_active(const sat_pose_fade_t* fade) {
    return fade && fade->elapsed < fade->duration;
}

extern "C" sat_result_t sat_pose_fade_apply(
    const sat_pose_fade_t* fade, sat_skeleton_pose_joint_t* pose) {
    if (!fade || !fade->from || !pose) return SAT_ERR_INVALID_ARG;
    if (!sat_pose_fade_active(fade)) return SAT_OK;
    const sat_fx16_t t = sat_fx16_div(fade->elapsed, fade->duration);
    return sat_skeleton_pose_blend(pose, fade->from, pose, fade->joint_count, t);
}
