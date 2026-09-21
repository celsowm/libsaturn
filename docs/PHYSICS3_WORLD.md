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

## Opt-in finite-mesh CCD (limited scope)

`sat_sphere_cast_mesh_faces(mesh, &sphere, &displacement, &hit, &found)`
queries the earliest radius-offset **face-interior** crossing of a finite
quad mesh in a fixed-point displacement. `hit.t` is the fractional time of
impact, `hit.center` is the sphere center and `hit.point` is on the face.
An initially overlapping sphere is handled by existing contact resolution,
not by this cast. The query is not a full swept-sphere mesh cast: grazing a
quad **edge or vertex** from outside is not detected by the interior cast.

`sat_physics3_set_mesh_face_ccd(world, 1)` opts the fixed-tick world into
a sweep before each discrete step against all registered static meshes. On a
face-interior crossing, the world places the sphere at contact, resolves the
incoming velocity, and moves the remaining substep using that velocity.
The optional `sat_sphere_cast_mesh` extends this query to finite faces,
**edges and vertices**, returning the first feature hit. The world uses that
fuller query when CCD is enabled; the face-only query remains available to
callers that need specifically a face-interior crossing.

When a world contains only static meshes and dynamic spheres, this option
caps substeps at `max_substeps` rather than rejecting a high-speed tick;
box/plane/kinematic worlds retain the original capacity rejection contract.
The sweep scans every mesh face even for grid-backed colliders; face, edge
and vertex impact tests are bounded integer calculations at 16.16 time
resolution. Grazing hits narrower than one time unit, large-coordinate
overflow, moving boxes, sphere/sphere contacts and non-mesh colliders
remain outside its guarantee. This is intentionally **not a complete
rigid-body CCD solver**. Keep gameplay coordinates local to the stage.

## Opt-in solid-sphere rolling (angular physics)

Each dynamic sphere keeps an identity-initialized orientation quaternion and
world-space angular velocity (radians per fixed tick). Existing actors retain
the earlier arcade tangential damping until explicitly enabled with
`sat_physics3_set_rolling(world, sphere_id, 1)`. Optional initial spin is set
with `sat_physics3_set_angular_velocity`. Both states are readable with
`sat_physics3_get_actor`. Only dynamic spheres with radius >= 1/8 can opt in.

For a homogeneous solid sphere (inertia `I=2/5*m*r²`), a grounded contact
computes the relative contact-point slip: tangential translation minus
`radius * (angular_velocity × normal)`. Friction applies coupled impulses:
`Δv_t=-friction*(2/7)*slip` and
`Δω=friction*(5/7)/radius*(normal × slip)`. This models transfer of
translational motion to spin, instead of merely deleting horizontal speed.
On a moving kinematic box, the solver uses velocity relative to that box.
Without contact the angular velocity persists; after each substep a normalized
quaternion is integrated using that angular velocity (world-frame Euler step).

This is a **bounded, arcade-friendly solid-sphere rolling model**, not a
complete Coulomb/rigid-body solver: there is no torque from wall impacts,
rolling resistance, contact manifold, rotationally swept contact, or strict
conservation of energy under solver iterations. Generated angular components
are saturated at ±8 radians/tick to keep the 16.16 integration bounded;
large speeds/radii near fixed-point limits remain unsupported. Rendering
orientation and parenting to the transform graph are separate integration
steps; do not treat the quaternion as an automatically updated scene node.

### Renderer-facing model matrix

`sat_physics3_sphere_model_matrix(world, sphere_id, &matrix)` exports an
unscaled row-major model matrix from the sphere's quaternion and world-space
center. When using the canonical painter, set a LOCAL-space sphere mesh on a
`sat_scene3d_instance_t`, assign `instance.world = &matrix`, and submit
with `sat_scene_submit_instance` while the matrix is still alive. The
painter copies transformed geometry during submission; there is no implicit
link between a physics actor and a scene-hierarchy node. Apply authored
visual scaling in the local mesh (or compose an explicit scale matrix).
