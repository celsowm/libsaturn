#ifndef SATURN_SKELETON3D_H
#define SATURN_SKELETON3D_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/math3d.h"
#include "saturn/transform3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Skeletal pose evaluation (fixed point, deterministic, no heap)      */
/* ------------------------------------------------------------------ */
/* A skeleton is an immutable asset: a joint list in PARENT-FIRST order
 * (a joint's parent index is always smaller than its own, or ROOT). That
 * ordering makes cycles unrepresentable, so validation is a single pass and
 * evaluation needs no recursion. Each character INSTANCE owns only a node
 * array; the hierarchy maths is the existing sat_transform3d evaluator, not a
 * second one.
 *
 * Rotations are unit quaternions in Q15 (32768 = 1.0). Joint transforms are
 * rigid (rotation + translation); translations are 16.16 like the rest of
 * the library. Matrices are the library's row-major sat_mat4_t, translation
 * in m[3], m[7], m[11].
 *
 * Quaternion inputs must be (approximately) unit length; use
 * sat_quat16_normalize for anything computed. Interpolation is normalised
 * lerp (shortest arc), integer-only: no floating point on the SH-2. */

#define SAT_QUAT16_ONE 32768

typedef struct sat_quat16 {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t w;
} sat_quat16_t;

void sat_quat16_identity(sat_quat16_t* out);
/* Scales to unit length. A zero quaternion becomes identity. */
void sat_quat16_normalize(sat_quat16_t* out, const sat_quat16_t* q);
/* Shortest-arc normalised lerp. t in 16.16, clamped to [0, 1]. */
void sat_quat16_nlerp(
    sat_quat16_t* out, const sat_quat16_t* a, const sat_quat16_t* b,
    sat_fx16_t t);
/* Rigid matrix from rotation + translation. */
void sat_quat16_to_mat4(
    sat_mat4_t* out, const sat_quat16_t* q, const sat_vec3_t* translation);

typedef struct sat_skeleton_joint {
    uint16_t parent; /* SAT_TRANSFORM3D_ROOT or an index < this joint's */
    uint16_t reserved;
    sat_vec3_t rest_translation; /* relative to the parent */
    sat_quat16_t rest_rotation;
} sat_skeleton_joint_t;

typedef struct sat_skeleton_asset {
    const sat_skeleton_joint_t* joints;
    uint16_t joint_count;
    uint16_t reserved;
} sat_skeleton_asset_t;

/* One joint of an animation pose, parent-relative. */
typedef struct sat_skeleton_pose_joint {
    sat_vec3_t translation;
    sat_quat16_t rotation;
} sat_skeleton_pose_joint_t;

/* Checks null pointers, count > 0, parent ordering and unit-ish rotations. */
sat_result_t sat_skeleton_validate(const sat_skeleton_asset_t* skeleton);

/* Writes the rest pose (joint_count entries). */
sat_result_t sat_skeleton_rest_pose(
    const sat_skeleton_asset_t* skeleton,
    sat_skeleton_pose_joint_t* out, uint16_t capacity);

/* Blends two poses joint by joint: translation lerp, rotation nlerp.
 * `out` may alias `a` or `b`. t in 16.16, clamped to [0, 1]. */
sat_result_t sat_skeleton_pose_blend(
    sat_skeleton_pose_joint_t* out, const sat_skeleton_pose_joint_t* a,
    const sat_skeleton_pose_joint_t* b, uint16_t joint_count, sat_fx16_t t);

/* Per-character evaluation state. nodes and scratch are caller-owned and
 * need `joint_count` entries each; many instances may share one asset. */
typedef struct sat_skeleton_instance {
    const sat_skeleton_asset_t* skeleton;
    sat_transform3d_world_t world;
    uint16_t* scratch;
    uint16_t scratch_capacity;
    uint8_t evaluated;
    uint8_t reserved;
} sat_skeleton_instance_t;

sat_result_t sat_skeleton_instance_init(
    sat_skeleton_instance_t* instance, const sat_skeleton_asset_t* skeleton,
    sat_transform3d_node_t* nodes, uint16_t node_capacity,
    uint16_t* scratch, uint16_t scratch_capacity);

/* Evaluates `pose` (joint_count entries, parent-relative) into model-space
 * joint matrices. Nothing is modified when the arguments are rejected. */
sat_result_t sat_skeleton_evaluate(
    sat_skeleton_instance_t* instance, const sat_skeleton_pose_joint_t* pose,
    uint16_t pose_count);

/* Model-space matrix of a joint after evaluate(); SAT_ERR_BUSY before. */
sat_result_t sat_skeleton_joint_world(
    const sat_skeleton_instance_t* instance, uint16_t joint, sat_mat4_t* out);

/* Rigid inverse of every rest-pose joint matrix (the bind inverse), for
 * building skin matrices. out has joint_count entries. */
sat_result_t sat_skeleton_inverse_bind(
    const sat_skeleton_asset_t* skeleton, sat_mat4_t* out, uint16_t capacity);

/* out[i] = joint_world[i] * inverse_bind[i], the matrix that carries a
 * bind-space vertex to its posed position. */
sat_result_t sat_skeleton_skin_matrices(
    const sat_skeleton_instance_t* instance, const sat_mat4_t* inverse_bind,
    sat_mat4_t* out, uint16_t capacity);

/* ------------------------------------------------------------------ */
/* Clips, playback, events, root motion, crossfade                     */
/* ------------------------------------------------------------------ */
/* A clip is immutable, frame-major pose data (frame f, joint j at
 * frames[f * joint_count + j]) sampled at rate_num / rate_den frames per
 * second. Random access to any frame is O(joints), so a move can start at
 * any time without decoding anything earlier. Compressed/quantised clip
 * storage belongs to the asset pipeline and decodes into this layout.
 *
 * Playback state is a small caller struct; many characters may play the
 * same clip. Time is 16.16 seconds. */

#define SAT_CLIP_FLAG_LOOP 0x0001u
/* Root-motion extraction: when set, the root joint's translation on the
 * selected axes is held at its frame-0 value in the sampled pose and the
 * travel is reported as a delta by sat_skel_clip_player_advance instead. */
#define SAT_CLIP_ROOT_X 0x0002u
#define SAT_CLIP_ROOT_Y 0x0004u
#define SAT_CLIP_ROOT_Z 0x0008u

typedef struct sat_clip_event {
    uint16_t frame; /* fires when playback crosses this integer frame */
    uint16_t id;
} sat_clip_event_t;

typedef struct sat_skeleton_clip {
    const sat_skeleton_pose_joint_t* frames;
    const sat_clip_event_t* events; /* sorted by frame; may be null */
    uint16_t frame_count;
    uint16_t joint_count;
    uint16_t event_count;
    uint16_t rate_num;
    uint16_t rate_den;
    uint16_t flags;
} sat_skeleton_clip_t;

sat_result_t sat_clip_validate(
    const sat_skeleton_clip_t* clip, const sat_skeleton_asset_t* skeleton);

/* Clip duration in 16.16 seconds: frame_count frames for a looping clip,
 * frame_count - 1 for a one-shot (which holds its last frame). */
sat_fx16_t sat_clip_duration(const sat_skeleton_clip_t* clip);

/* Pose at an explicit time (16.16 seconds). Looping clips wrap (the last
 * frame blends into frame 0); one-shots clamp. out has joint_count entries. */
sat_result_t sat_clip_sample(
    const sat_skeleton_clip_t* clip, sat_fx16_t time,
    sat_skeleton_pose_joint_t* out, uint16_t capacity);

#define SAT_CLIP_PLAYER_PAUSED 0x0001u
#define SAT_CLIP_PLAYER_FINISHED 0x0002u

typedef struct sat_skel_clip_player {
    const sat_skeleton_clip_t* clip;
    sat_fx16_t time;
    sat_fx16_t speed; /* 16.16 multiplier, 1.0 = authored rate; must be >= 0 */
    uint16_t flags;
    uint16_t reserved;
} sat_skel_clip_player_t;

sat_result_t sat_skel_clip_player_init(sat_skel_clip_player_t* player, const sat_skeleton_clip_t* clip);
/* Restarts on another clip at time 0 (or any time), keeping speed. */
sat_result_t sat_skel_clip_player_set_clip(
    sat_skel_clip_player_t* player, const sat_skeleton_clip_t* clip, sat_fx16_t start_time);
sat_result_t sat_skel_clip_player_set_speed(sat_skel_clip_player_t* player, sat_fx16_t speed);
void sat_skel_clip_player_set_paused(sat_skel_clip_player_t* player, int paused);

/* Advances by dt (16.16 seconds of wall/simulation time, scaled by speed).
 * Reports events crossed in this step (in playback order, wrapping loops)
 * into events_out and root travel in root_delta_out; either may be null.
 * *event_count_out receives the number reported. SAT_ERR_CAPACITY (state
 * untouched) if the events do not fit. A paused or finished player leaves
 * its time unchanged and reports nothing. */
sat_result_t sat_skel_clip_player_advance(
    sat_skel_clip_player_t* player, sat_fx16_t dt,
    sat_clip_event_t* events_out, uint16_t event_capacity,
    uint16_t* event_count_out, sat_vec3_t* root_delta_out);

sat_result_t sat_skel_clip_player_sample(
    const sat_skel_clip_player_t* player, sat_skeleton_pose_joint_t* out,
    uint16_t capacity);

/* Crossfade between whatever was playing and a new pose source. The caller
 * owns the `from` snapshot buffer. begin() freezes the current output pose
 * (typically the last pose evaluated) and fades to the live pose over
 * `duration` (16.16 seconds, 0 = cut). Interrupting a fade is safe: begin
 * again with the current blended pose. */
typedef struct sat_pose_fade {
    sat_skeleton_pose_joint_t* from;
    uint16_t joint_count;
    uint16_t reserved;
    sat_fx16_t duration;
    sat_fx16_t elapsed;
} sat_pose_fade_t;

sat_result_t sat_pose_fade_init(
    sat_pose_fade_t* fade, sat_skeleton_pose_joint_t* storage, uint16_t joint_count);
sat_result_t sat_pose_fade_begin(
    sat_pose_fade_t* fade, const sat_skeleton_pose_joint_t* current,
    sat_fx16_t duration);
void sat_pose_fade_advance(sat_pose_fade_t* fade, sat_fx16_t dt);
int sat_pose_fade_active(const sat_pose_fade_t* fade);
/* pose is the live (target) pose; it is overwritten with the faded result. */
sat_result_t sat_pose_fade_apply(
    const sat_pose_fade_t* fade, sat_skeleton_pose_joint_t* pose);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SKELETON3D_H */
