# Skeletal animation runtime (`saturn/skeleton3d.h`)

Status: phases 1-2 of `docs/SKELETAL_ANIMATION_PLAN.md` plus the
`skeletal_animation_3d` example from phase 8 (Example A) at MVP scope.
Pose evaluation, clip sampling, playback, events, root motion, crossfade,
CPU skinning (`saturn/skin3d.h`) and scene integration are implemented
and host-tested. The importer now has a `--target skeletal` path that
captures per-joint `rest_scale` and a `--max-triangles` decimation
knob. What is not done yet: parallel tasks, the `fighter_arena_3d` and
`skinning_stress_3d` examples, the full 6-clip §15 Definition of Done
for Example A, and on-target timing.

## Model

- **Skeleton** (`sat_skeleton_asset_t`): immutable joint list in parent-first
  order. A joint's parent index is smaller than its own (or
  `SAT_TRANSFORM3D_ROOT`), so cycles cannot be expressed and evaluation needs no
  recursion. Joints are rigid: translation (16.16) and a unit quaternion.
- **Rotation** (`sat_quat16_t`): Q15 (32768 = 1.0), identity is w = 32767.
  `sat_quat16_nlerp` is shortest-arc normalised lerp, integer only. Inputs must
  be near unit length (`sat_quat16_normalize` repairs them). `sat_quat16_to_mat4`
  matches the library's row-major `sat_mat4_t`.
- **Instance** (`sat_skeleton_instance_t`): caller-owned `sat_transform3d` node
  array plus a `uint16_t` scratch array, `joint_count` entries each. Many
  instances share one asset. Hierarchy maths is the existing
  `sat_transform3d_evaluate`; there is no second evaluator.
- **Skin matrices**: `sat_skeleton_inverse_bind` + `sat_skeleton_skin_matrices`
  give `joint_world * inverse_bind` for the future skinning stage.

## Clips and playback

- `sat_skeleton_clip_t`: frame-major poses, rate `num/den` frames per second
  (`num` <= 32767), optional sorted events. Any frame is O(joints) to sample, so
  moves can start anywhere. Compressed storage is the asset pipeline's job and
  decodes into this layout.
- Looping clips interpolate the last frame into frame 0; one-shots hold the last
  frame and set `SAT_CLIP_PLAYER_FINISHED`.
- `sat_skel_clip_player_advance` reports events crossed (integer frame K with
  `floor(old) < K <= floor(new)`, counting loop wraps) and root travel. A
  capacity error leaves the player untouched. With no output buffer it only
  counts.
- Root motion: `SAT_CLIP_ROOT_X/Y/Z` hold those root axes at their frame-0 value
  in the sampled pose and report the travel as a delta. Travel is measured on
  frames 0..last; each loop adds `last - first` and the last-to-first segment
  contributes no travel.
- `sat_pose_fade_*`: crossfade from a frozen snapshot (caller storage) to the live
  pose; interrupting a fade is a new `begin` from the blended pose.

## CPU skinning (`saturn/skin3d.h`)

- Every influence has its own joint-local position (multi-anchor):
  `P = w0*(M0*P0) + w1*(M1*P1) + w2*(M2*P2)`. A conventional single-bind mesh is
  exported as `Pi = inverse_bind_i * P_bind`, so the runtime needs no inverse-bind
  matrices; `M` is just the joint's model-space matrix.
- Output vertices are the mesh's unique vertices, ordered
  `[rigid records][seam records]`: `sat_skin_rigid_t` is 8 bytes, `sat_skin_blend_t`
  (2-3 influences) is 26 bytes. Weights are Q15 and sum to exactly 32768 by
  construction (the last weight is implied). Positions are int16 with `pos_shift`
  fractional bits. Optional normals are int16 Q14.
- `sat_skin_validate` is the load-time check (joints in range, counts, weights,
  truthful `max_joint`); `sat_skin_deform` checks `max_joint` against the matrices
  given, then runs. `sat_skin_deform_range` deforms a vertex range for later
  parallel tasks; disjoint ranges write disjoint output.
- Arithmetic: 16.16 matrices times positions pre-scaled to 16.16, 64-bit
  accumulate shifted by the constant 16 (no variable 64-bit shift call on the
  SH-2), rounded to nearest. Identical influence matrices reproduce the
  single-influence result exactly; exact half-weight pairs use add-and-shift.
- Accuracy against an independent double-precision reference over 80 random
  vertices (rigid, 2- and 3-influence, three rotated joints): worst error 0.79 of
  one 16.16 LSB.

## Scene integration (`saturn/skin3d.h`)

The existing scene instance path already takes a mutable `sat_mesh_t`, so no new
renderer was added. `sat_skin_update_mesh` deforms into `mesh->vertices`, sets
`vertex_count` and returns an AABB (+margin) for `sat_scene3d_instance_t::bounds`;
indices, face count and materials are never touched. `sat_skin_validate_mesh` is
the load-time check that every index refers to a deformed vertex.

Two characters submitted to the same `sat_scene3d_faces_t` sort face by face in
one painter queue (`tests/host/test_skin_scene.cpp`: A's faces at depth 12/4 and
B's at 8/20 emit 20(B), 12(A), 8(B), 4(A), and swapping A's pose re-sorts the
next frame with the index array byte-identical). That test uses the library's
stubbed projection and VDP1 draws, so it proves ordering and bookkeeping, not
pixels; the painter's no-Z-buffer limits apply unchanged: faces that truly
interpenetrate (a punch through a torso) can still sort wrongly per face.

## Submeshes, LOD and lighting (`src/graphics/3d/animation/character.cpp`)

- Faces are stored grouped by submesh and LOD level over one shared vertex array.
  `sat_mesh_face_view` returns a `sat_mesh_t` whose index pointer is offset to a
  face range (offset `face_materials` by the same first face), so per-submesh
  visibility and LOD need no copies and no scene change.
- `sat_lod_select` picks the first level that fits a face budget and returns
  `SAT_ERR_CAPACITY` (with the last level) when none does, so the caller chooses
  explicitly; nothing is dropped silently.
- `sat_mesh_face_ramp` does flat directional lighting of the deformed mesh:
  `ambient + (1 - ambient) * max(0, n.l)` mapped to an index in a dark-to-bright
  material ramp, written as `face_materials`. Normals use the mesh winding
  (`cross(D - A, B - A)`, outward). Uses 64-bit math with a divide per face, so
  expect a `__divdi3` call per face on the SH-2 until measured and tuned.

Not yet done for rendering: Gouraud/per-vertex lighting for characters, shadows,
material overrides, alpha cutout policy.

## Measured so far

SH-2 object sizes (`sh2eb-elf-g++ -O2`, project flags): `skeleton.cpp` 3032
bytes text, `clip.cpp` 2320, `skin.cpp` 3400 (with the mesh and bounds helpers), no data/bss. `clip.cpp` pulls in
`__divdi3` through one 64-bit division in the frames-to-seconds conversion; it
runs once per advance and is a candidate for removal once profiled on the Saturn.
No Saturn or emulator timing has been taken yet.

## Tests

`tests/host/test_skeleton3d.cpp` (quaternions, validation incl. cycles and bad
parents, hierarchy, determinism, bind/skin matrices, blending, independent
instances), `tests/host/test_skin_scene.cpp` (above) and `tests/host/test_skin3d.cpp` (exact rigid, multi-anchor blends,
3 influences, validation and untouched-output-on-error, extremes, range vs full,
double reference, normals, shared asset) and `tests/host/test_skeleton_clip.cpp` (validation, sampling, loops,
root motion, events, speed/pause, rapid clip switching, crossfade). Both pass on
the host.

## `examples/skeletal_animation_3d` (Phase-8A MVP)

Phase-8A from `docs/SKELETAL_ANIMATION_PLAN.md` is wired up. The example
boots, the VDP1 draws the decimated `robot_police` rig and the HUD
reports FPS / joint count / vertex count / face count. Source GLB,
attribution and the runtime-measured numbers live in
`examples/skeletal_animation_3d/README.md`.

What the MVP ships (committed on the `feat(skeletal_3d): ship Phase-8A
MVP` line):

- One rigged humanoid driven by one looping clip:
  `robot_police_unit_animated.glb` (CC BY 4.0 by Jungle Jim, 102
  Mixamo-style joints, decimated to 943 vertices / 999 faces / ~90 KB
  ROM by the importer's `--max-triangles 1000 --quality aggressive`
  path).
- `sat_skeleton_instance_init` + `sat_skel_clip_player_advance/sample`
  + `sat_skeleton_evaluate` + `sat_skin_update_mesh` +
  `sat_scene_submit_instance` glue in
  `examples/skeletal_animation_3d/main.c`, with the large BSS working
  buffers (`g_mesh_vertices`, `g_mesh_screen`, `g_scene_faces`,
  `g_scene_keys`, `g_scene_order`, `g_mesh`, `g_mesh_indices`) in
  WRAM-L via the `SAT_SCN` macro.
- `sat_orbit_camera3d_apply_pad` for the C button, START toggles
  pause, on-screen diagnostics match the plan's
  `On-screen diagnostics` list (FPS, joint/vertex/face counts, submit
  status, pause/loop indicator).
- Asset provenance and CC BY 4.0 attribution committed under
  `examples/skeletal_animation_3d/assets/`.

Validation on a stock 1 MB WRAMH + 1 MB WRAML (the
`6c66ac03cdccc56f` build variant):

```text
ROM: skeleton=3.6 KB  skin=19.7 KB  clips=58.6 KB
     indices=7.8 KB  total=89.7 KB
DEFORM BUFFER: 11.1 KB (943 vec3s, runtime only)
app.bin: 280 KB, ISO: 680 KB, 340 sectors.
make test: 9 host + 2 sound + 15 stage2d = 26 tests green.
```

Ymir harness (300 frames after the BIOS boot): 0 misaligned accesses,
model visible in `harness/build/skel_fix_100.png` and
`harness/build/skel_fix_200.png`. The first iteration panicked with
`INVALID_ARG` from `sat_scene_submit_instance` because
`sat_skin_deform` writes the deformed vertices but does not update
`mesh->vertex_count`; the example now uses `sat_skin_update_mesh`,
which both deforms and stamps `vertex_count` for the scene submit.

Still pending for the full §15 Example A definition of done:

- Five more named clips (the free asset only ships one looping walk).
- The remaining HUD rows (SIMULATION FPS, ANIMATION CLIP/FRAME,
  VDP1 COMMANDS, SKINNING TIME, RENDER PREPARATION TIME, MEMORY
  USAGE).
- Skeleton visualisation toggle and shading toggle.
- `fighter_arena_3d` and `skinning_stress_3d` examples.
- On-target Saturn timing (no Ymir cycle profile yet).
