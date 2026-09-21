# Scene3D deferred painter queue — historical architecture record

> Superseded for game-facing code by `sat_scene_t` in `include/saturn/scene.h`.
> The queue API described by the original version of this document was
> deleted during the breaking cutover. New code must use the canonical scene
> contract; `sat_vdp1_draw_mesh` remains only for low-level renderer tests and
> hardware probes.

The original queue was introduced to prove that camera-space depth must be
shared across platforms, actors and pickups. That proof is now embodied by
`sat_scene3d_faces_t`, which collects faces from different meshes, applies the
same clipping/material lowering path and performs deterministic far-to-near
ordering in caller-owned bounded storage.

The current renderer still has the VDP1 limitation that a single total order
cannot represent every intersection or cyclic overlap between large polygons.
Skybridge therefore keeps authored passes and conservative near-eye/platform
visibility rules. This is an explicit hardware limitation, not a second
game-facing painter implementation. Further correctness requires subdivision
or partitioning of authored geometry and must be measured against the stock
Saturn command and CPU budgets.

Current ownership and validation are tracked in
`docs/RUNTIME_OWNERSHIP_AND_EXAMPLE_LEDGER.md` and
`docs/RESOURCE_PERFORMANCE_LEDGER.md`.
