#ifndef SATURN_CHARACTER3D_H
#define SATURN_CHARACTER3D_H
#include "saturn/skin3d.h"
#include "saturn/model3d.h"
#include "saturn/scene.h"
#include "saturn/character_asset_format.h"
#ifdef __cplusplus
extern "C" {
#endif
/* SKA1 relocatable big-endian character archive, version 1. Decode once into
 * aligned caller-owned mutable storage; all view pointers borrow that storage.
 * clips/textures are caller-owned descriptor arrays, not allocations. Payload
 * includes immutable hierarchy, multi-anchor skin, faces, palettes, textures,
 * named joints and independently addressable clips. No runtime JSON parsing.
 * Keep storage/descriptors live through any worker, instance and scene use. */
typedef struct sat_character_asset {
    sat_skeleton_asset_t skeleton;
    sat_skin_asset_t skin;
    sat_model_asset_t model;
    sat_skeleton_clip_t* clips;
    uint16_t clip_count;
    uint16_t reserved;
    const uint8_t* storage;
    uint32_t storage_bytes;
    const uint32_t* joint_names;
    const uint8_t* clip_directory;
    sat_vec3_t bind_min, bind_max;
} sat_character_asset_t;
sat_result_t sat_character_asset_decode(void* storage, uint32_t bytes,
    sat_skeleton_clip_t* clips, uint16_t clip_capacity,
    sat_model_texture_asset_t* textures, uint16_t texture_capacity,
    sat_character_asset_t* out);
sat_result_t sat_character_asset_load(const char* logical_path,
    void* storage, uint32_t capacity, sat_skeleton_clip_t* clips, uint16_t clip_capacity,
    sat_model_texture_asset_t* textures, uint16_t texture_capacity,
    sat_character_asset_t* out);
/* Read only one archive clip, without loading geometry or other clips.
 * Capacity is in pose/event entries. Source registration stays caller-owned.
 * out is committed after a complete bounded read and clip validation. */
sat_result_t sat_character_clip_load(const char* logical_path, uint16_t clip_index,
    sat_skeleton_pose_joint_t* poses, uint32_t pose_capacity,
    sat_clip_event_t* events, uint16_t event_capacity, sat_skeleton_clip_t* out);
/* Move a decoded asset to `storage` without its texture pixels and table, which
 * nothing reads once they are uploaded (about 70% of a textured archive). Every
 * view is rebased and model.textures becomes NULL, so upload first; the archive
 * header inside no longer describes the tables, so the result cannot be decoded
 * again. The old storage is free afterwards; the two must not overlap.
 * out_bytes is optional. */
sat_result_t sat_character_asset_compact(sat_character_asset_t* asset,
    void* storage, uint32_t capacity, uint32_t* out_bytes);
/* Releases the borrowed view; stop workers and destroy texture handles first.
 * Does not free caller memory, reset the global asset cache or other views. */
void sat_character_asset_release(sat_character_asset_t* asset);
const char* sat_character_joint_name(const sat_character_asset_t* asset, uint16_t joint);
const char* sat_character_clip_name(const sat_character_asset_t* asset, uint16_t clip);
/* Managed texture ownership backed by the existing texture allocator. Native
 * copies/material pointers remain valid until destroy; wait for VDP1 and jobs
 * before release. Shared immutable pixels/palettes are uploaded only once. */
sat_result_t sat_character_materials_upload(const sat_model_asset_t* model,
    sat_texture_t* owners, sat_vdp1_texture_t* native,
    sat_scene3d_material_t* materials, uint16_t capacity);
sat_result_t sat_character_materials_release(sat_texture_t* owners, uint16_t count);
/* Optional opaque contact ellipse: bounded 3..16 triangles in the canonical
 * scene. This is a ground-contact approximation, not a projected mesh shadow. */
sat_result_t sat_character_shadow(sat_scene_t* scene, const sat_vec3_t* center,
    sat_fx16_t ground_y, sat_fx16_t radius_x, sat_fx16_t radius_z,
    uint16_t rgb555, uint16_t pass, uint8_t segments);
#ifdef __cplusplus
}
#endif
#endif
