# Optional world-space BVH broadphase

`sat_physics3_set_spatial_broadphase` is an opt-in, caller-owned bounded
**bounding-volume hierarchy** over finite actors in a `sat_physics3_world_t`.
It does not replace mesh-face grids, discrete/continuous narrowphase, or the
legacy direct world API. It has no heap, static mutable global data or
hardware dependency.

## Setup

Initialize the world, then bind the existing type-index scratch with
`sat_physics3_set_collider_index_scratch(world, ids, world.capacity)`.
Allocate at least `2 * world.capacity - 1`
`sat_physics3_spatial_node_t` elements and `world.capacity`
`uint16_t` candidate IDs; call `sat_physics3_set_spatial_broadphase`
with those caller-owned arrays. Keep all three arrays alive through each
`world_step`. All allocations are explicit, even if the world has only
one collider. Passing all NULL/zero arguments detaches the BVH; detach it
before detaching the type-index array.

## What is indexed

Finite static/kinematic boxes and finite static/kinematic meshes are indexed.
Static boxes and meshes use world bounds. Moving boxes use the union of the
current and full-tick target AABB. Translating meshes use their registered
reference bounds at both offsets. Rotating meshes use a conservative L1
local radius around the mesh origin over the entire translation interval,
including all intermediate orientations; rotational *CCD* remains unsupported.
Infinite planes are never indexed: they form a small, source-ordered fallback.
Dynamic spheres are query bodies, not collider candidates; opt-in
sphere/sphere contacts use their own sweep-and-prune order instead
(`sat_physics3_set_sphere_pairs`).

The BVH is rebuilt once per successful tick *after validation and before
actor mutation*, rather than once per solver iteration. Every CCD query
tests a swept sphere interval; every discrete iteration queries the sphere's
current position again, including positions modified by previous contacts.
All AABB tests and intermediate bounds are inclusive 64-bit Q16.16, including
coordinates near INT32_MIN/MAX. BVH result IDs are sorted and merged with
fallback plane IDs in original actor order, preserving collision resolution
and equal-time CCD tie ordering. The existing per-mesh face grid can still
accelerate the narrowphase after the world-level BVH selects a mesh.

## Capacity and performance

Nodes: `2 * world.capacity - 1`; query IDs and fallback IDs:
`world.capacity` each, all caller-owned. Binding validates capacities before
changing an existing configuration. Build uses a deterministic O(N log N)
in-place heapsort on the finite actor leaves and O(N) branch construction.
Query traverses only intersecting BVH bounds, then sorts the K returned
actor IDs O(K log K) and merges them with P planes in O(K+P). Worst case
(all actor bounds overlap) remains O(N) candidates, as with any broadphase.
Spatial arrangement can also affect pruning: this balanced BVH splits along
world X; scenes dominated by overlapping X intervals may benefit from a
future multi-axis partitioning strategy.

`world.spatial_queries` counts BVH candidate requests per tick and
`world.spatial_candidates_checked` counts actor IDs handed to the CCD and
discrete loops; these counters do not measure actual narrowphase calls or
emulator cycles. A request without a bound BVH continues through the original
typed/full linear scan. There is no implicit allocation or fallback index.

## Verification

`tests/host/test_physics3_spatial_index.cpp` exercises static, moving,
rotating and infinite actors; swept query, deterministic candidate ordering
and extreme 32-bit coordinates. The opt-in and legacy worlds are also run
side by side in `tests/host/test_physics3_world.cpp` with static and
kinematic boxes and an infinite plane. The host comparisons do not substitute
for Saturn emulator timing and visual validation.
