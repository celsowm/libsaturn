# CPU skinning and character assets

The public API is in `saturn/skeleton3d.h`, `saturn/skin3d.h` and
`saturn/character3d.h`. Translations and matrices use signed 16.16;
rotations use normalized Q15 quaternions with shortest-arc nlerp. Parent-first
skeleton assets reuse `transform3d`, support caller-sized hierarchies, and
share immutable data across independent instances. Unchanged local matrices
keep their hierarchy cache; changed parents still propagate to descendants.
The host gate explicitly evaluates a 64-joint chain. The optional imported
robot exercises 102 joints.

## Deformation and ownership

Each influence has its own joint-local position:

```text
P = w0 * (M0 * P0) + w1 * (M1 * P1) + w2 * (M2 * P2)
```

Rigid records contain one int16 XYZ and an 8-bit joint index. Blend records
contain up to three anchors and joint indices plus two stored Q15 weights;
the final weight is implied so the sum is exactly 32,768. Position fractional
precision is an explicit asset field. Native layouts are 8-byte rigid and
26-byte blend records. Shared quantized records are deformed once; face UVs
and materials remain independent. Exact half-weight pairs avoid weight
multiplies. Accumulation keeps the necessary 64-bit precision.

Run `sat_skin_validate` at load. `sat_skin_deform_range` accepts disjoint,
absolute output ranges; `sat_skin_update_mesh` also updates vertex count and
optionally returns an AABB. All topology/material/storage buffers are owned
by the caller. Normals are optional; the existing normal/Gouraud helpers can
also recompute normals from a deformed mesh when that cost is acceptable.

For Q15 anchor storage, deformation uses the constant conversion `q * 2`.
The SH-2 GCC 13.2 generic conversion emitted three `__ashlsi3_r0` calls per
influence; the constant branch avoids those calls. Accumulation, translations,
blend weights and rounding remain identical. Other anchor precisions retain
the existing generic conversion. Host tests compare rigid and one/two/three
influence records with an independent wide reference across all 16 position
shifts, signed anchor extremes, large coefficients and partial ranges.

`sat_skin_requirements` calculates actual native skeleton, skin, vertex,
pose, joint-matrix and hierarchy scratch sizes. Scene storage uses
`sat_scene_requirements`; texture payloads come from model metadata. Add these
quantities to the existing `sat_resource_plan` with explicit quotas. The
linker independently enforces physical RAM and Master stack limits.

## Parallel work

After `sat_parallel_init`, register `sat_skin_parallel_register` once.
Submit a caller-owned `sat_skin_job_t` with MASTER, SLAVE or AUTO policy.
Use existing handles to wait, inspect results, cancel queued work, abort a
running worker and release terminal tasks. Source descriptors, matrices and
output remain alive until ownership returns. A failed wait does not permit
reusing or freeing the worker's output. Nested sources are published and the
worker reads them through cache-through aliases; output consumption follows
the canonical executor.

AUTO keeps jobs with fewer than 128 vertices on Master, using a named threshold
in the adapter. The example family overlaps one character's
Slave job with the next character's forced-Master job; no two CPUs write the
same actor. The optional robot retains its measured range split. Host tests
check capacity rejection before dispatch, independent outputs and exact
serial/worker agreement. Emulator diagnostics check a real posed output when
the selected mode changes and expose actual worker/master task counts.

## SKA1 binary assets and loading

`tools/import_model.py --target skeletal --skeletal-binary <file.ska>` emits
a relocatable archive alongside C/H output. It uses the existing importer,
coordinate normalization, simplifier, texture baker and verification oracle.
`SourceModel.skin_anchors` is the generic converter IR for distinct local
positions; no proprietary model parser is included.

The wire specification is centralized in `character_asset_format.h` and
`tools/model_pipeline/skeletal_binary.py`. SKA1 version 1 has a 24-word
big-endian header describing counts, quantization and table offsets. It stores
36-byte joints, 8/26-byte skin records, 12-byte bind vertices, 8-byte native
quad indices, face texture selectors, indexed8 pixels/palette, named joints
and 24-byte clip-directory entries. Texture entries retain palette-bank base.
Pose records contain three int32 translations and four int16 quaternion
components: their size is derived from those fields, not an unexplained
literal. Native-layout assertions guard direct table views.

`sat_character_asset_decode` converts an aligned mutable caller buffer once,
builds caller-owned clip/texture descriptors, validates capacities, hierarchy,
weights, indices, regions and overlapping writable payloads, and returns a
borrowed view. Do not decode an already converted buffer again; reload fresh
wire bytes. `sat_character_asset_load` reads a registered logical asset through
the existing cache. `sat_character_clip_load` reads only the selected clip's
directory, poses and events into caller storage, without loading geometry or
other clips. Random move selection does not require sequential animation decode.

Release workers and GPU texture users before `sat_character_asset_release`;
release the caller's source registration with `sat_asset_close` separately.
Two archive fixtures exceed one 64 KiB cache block, load into independent
buffers, support random clip access and remain independent when one view is
released. Character textures use the existing managed texture allocator.

## Compression decision

The original example's nine clips use 71,520 native pose bytes. Indexed
constant-channel elimination needs 16,464 bytes with exact integer poses;
int16 translation quantization needs 50,064 bytes with at most roughly
0.000031 world-unit translation error in this fixture. The generator reports
host decode timings for both random-access layouts. These timings include
Python overhead and are not SH-2 timings. Native resident clips remain selected
to avoid an unmeasured hot-path decoder; optional compact-channel decoding is
a future optimization, not a prerequisite for these stock-RAM examples.

See [readiness](FIGHTING_GAME_READINESS.md) for measured rendering costs and
[the robot study](SKELETAL_ANIMATION_PERFORMANCE_STUDY.md) for the earlier
lossless sharing and range-split measurements.
