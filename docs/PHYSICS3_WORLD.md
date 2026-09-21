# Bounded Physics 3D World — first working slice

`physics3_world.h` adds a small, fixed-tick simulation facade over the existing
`sat_sphere_aabb3_contact` collision primitive. It does not create a second
collision engine, scene renderer, or game-specific tilting mechanic.

## Supported in this slice

- Caller-owned array of stable actor IDs (reused only by world reset).
- Static and kinematic axis-aligned boxes, static finite quad meshes,
  static infinite/two-sided plane slopes,
  and dynamic spheres with per-tick gravity.
- Next-tick kinematic targets interpolated through the same bounded substeps
  used for sphere movement, with relative contact velocity.
- Per-actor friction and restitution [0,1] in 16.16 fixed point. Pairwise
  combination uses the MINIMUM of the two coefficients. Friction damps the
  tangent to the contact plane on upward support contacts and acts each
  contacted substep.
- Multiple box/plane/mesh contacts per sphere, up to `iterations` solver passes.
- Meshes preserve finite face edges and gaps; no implicit infinite floor.
  Contact queries use the existing `sat_sphere_mesh_contact` implementation.
  The existing `SAT_BODY3_GROUNDED` and `SAT_BODY3_HIT_WALL` flags apply.
- Reproducible update order (actor index, then box index) and explicit
  `SAT_ERR_CAPACITY` if the fixed substep budget is insufficient.
  Capacity rejection occurs before positions, velocities or flags change.
- No allocations, implicit wall-clock timing, VDP dependencies, or dependence
  on dynamic memory. Call `world_step` once per fixed simulation tick.

## Explicit limits and forthcoming work

This is **discrete substepping**, NOT mathematically exact continuous collision
detection: narrow geometry, high speed, corner configurations, fast-moving
platforms, and fixed-point overflow near world-coordinate extremes require
further validation. Rejecting excess motion avoids silently accepting steps
beyond the user-selected budget but does not guarantee absence of tunneling.
This slice does NOT simulate dynamic sphere-sphere contacts, oriented boxes,
moving mesh contacts, rolling torque/inertia, manifold caching, joints, sensors,
automatic broad-phase selection or transform-graph synchronization. Friction here
models arcade tangential damping, not a full Coulomb solver. Exposing these
limits avoids implicitly promising Monkey Ball's specific rolling physics.

For now, keep world coordinates/velocities far from 32-bit limits. Callers
should avoid mutating the exposed actor storage directly; use the public
creation, velocity and kinematic-target APIs.

## Usage and validation

Add boxes/spheres once, specify the next kinematic center before each tick,
call `sat_physics3_world_step` at a fixed cadence, then read back each
actor with `sat_physics3_get_actor`. Game code selects gravity, materials
and control logic. Host tests verify floor contacts, friction/restitution,
moving platforms, stable IDs, capacity rejection, and deterministic replay.

## Finite static meshes

Call `sat_physics3_set_mesh_contacts(world, storage, capacity)` before
`sat_physics3_add_mesh(world, mesh, material, &id)`. The shared caller-owned
storage must fit the **largest face_count among registered meshes**, even if
only a few faces actually touch the sphere. This guarantees the existing
mesh-contact query does not silently truncate its output; when insufficient,
registration reports `SAT_ERR_CAPACITY`. Keep the mesh and contact storage
alive and the mesh geometry unchanged while registered. Meshes are static and
world-space: do not mutate vertices or transform them implicitly through the
renderer. A ramp, ledge or hole should be represented by bounded quad faces.

For small meshes, `sat_physics3_add_mesh` scans every face. For larger
immutable meshes, initialize a `sat_mesh3_grid_t` once with
`sat_mesh3_grid_init` (using caller-owned bucket heads, entries and
face stamps), then register it with
`sat_physics3_add_mesh_grid(world, &grid, material, &id)`.
The world calls the existing `sat_sphere_mesh_contact_grid` during each
solver iteration to limit narrow-phase testing to nearby faces. It does not
create another grid, allocate buffers or silently fall back to the linear
path if the grid is invalid. The grid and its mutable query stamps must remain
owned by the world while in use; do not rebuild or independently query it
concurrently with the simulation.

Contact scratch must still fit the **largest registered mesh face count**:
this initial integration preserves the original complete-contact guarantee.
A smaller streaming contact buffer and controlled overflow handling are
separate future work. A grid lookup may return multiple contacts in a
different order from the linear path; compare outcomes within fixed-point
tolerance when testing multi-surface corner cases.
Mesh contacts are also **discrete** and two-sided, not swept collision or a
one-way platform. `world_step` preflights caller scratch before movement,
but the existing solver's general fixed-point overflow limitations remain.
