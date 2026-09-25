# Bounded Physics 3D World — first working slice

`physics3_world.h` adds a small, fixed-tick simulation facade over the existing
`sat_sphere_aabb3_contact` collision primitive. It does not create a second
collision engine, scene renderer, or game-specific tilting mechanic.

## Supported in this slice

- Caller-owned array of stable actor IDs (reused only by world reset).
- Static and kinematic axis-aligned boxes, static and translating finite quad meshes,
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
Dynamic spheres collide with each other only when a sweep-order array is
bound with `sat_physics3_set_sphere_pairs` (see "Sphere/sphere contacts"
below). This slice does NOT simulate oriented boxes,
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
a sweep before each discrete step against registered static and translating
quad meshes. On a
face-interior crossing, the world places the sphere at contact, resolves the
incoming velocity, and moves the remaining substep using that velocity.
The optional `sat_sphere_cast_mesh` extends this query to finite faces,
**edges and vertices**, returning the first feature hit. The world uses that
fuller query when CCD is enabled; the face-only query remains available to
callers that need specifically a face-interior crossing.

When a world contains only finite meshes (static or translating) and dynamic
spheres, this option
caps substeps at `max_substeps` rather than rejecting a high-speed tick;
box/plane/kinematic worlds retain the original capacity rejection contract.
### One-way platforms

`sat_physics3_set_one_way(world, collider_id, 1)` turns a box or mesh
(static or kinematic) into a jump-through platform: a contact or CCD hit is
resolved only when its normal points up (+Y) and the sphere is not moving
away from the surface relative to the platform's motion. A sphere rising
through the underside or an edge passes, then lands on top once it falls
back. Planes and spheres cannot be one-way.

### Sphere/sphere contacts (opt-in)

`sat_physics3_set_sphere_pairs(world, order, capacity)` binds a caller-owned
`uint16_t` array of at least `world.capacity` entries. Each substep, after
every sphere has moved and resolved its collider contacts, the spheres are
sorted by their low x extent (insertion sort over last substep's order,
ties by actor ID) and swept, so only pairs overlapping on x reach the
narrowphase. An overlapping pair is separated along the centre line, each
sphere moving in inverse proportion to its `mass` (`sat_physics3_set_mass`,
0 < mass <= 4096, default 1), and an approaching pair receives an impulse
with the smaller restitution of the two. Distances use 64-bit squares, so
radii must stay below 8192 units while pairs are enabled. Contacts are
discrete: two small, fast spheres can still pass through each other within
one substep, and there is no friction or spin transfer between spheres.
`world.sphere_pair_contacts` counts the tick's pair contacts. Unbound, the
world behaves exactly as before.

The sweep scans every mesh face even for grid-backed colliders; face, edge
and vertex impact tests are bounded integer calculations at 16.16 time
resolution. Grazing hits narrower than one time unit, large-coordinate
overflow, moving boxes, sphere/sphere sweeps and non-mesh colliders
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

## Opt-in CCD against translating kinematic boxes

`sat_physics3_set_kinematic_box_ccd(world, 1)` adds a **relative-motion
swept-sphere query against the six finite faces of each kinematic AABB**
during each fixed substep. The query reuses `sat_sphere_cast_mesh` rather
than approximating a translating box by an infinite plane: finite faces,
edges, and corners participate. The collision point is transformed back to
world space at the fractional impact time; the existing contact solver
applies the box's per-tick velocity before finishing the sphere's remaining
substep. The box itself continues to its target for the tick.

When *all* colliders in the world are covered by an enabled mesh sweep or
kinematic-box sweep, requested substeps above `max_substeps` are capped
instead of failing with `SAT_ERR_CAPACITY`. Static AABBs or infinite
planes still require the original discrete substep budget; disabling both
sweep options preserves the previous step-capacity behavior. A mixed world
may enable both options independently. The kinematic geometry is assumed
to translate **linearly** between successive targets. Rotating boxes,
rotating or deforming meshes and sphere/sphere CCD are not covered.
Translation-only kinematic quad meshes can instead use the mesh sweep option.

The current finite-box query tests six faces per sphere/box/substep, so
high actor counts can be costly on SH-2. It is opt-in, stack-only and
allocation-free, but not a broad-phase-accelerated collider; benchmark
before enabling it on a large stage. The existing numerical/initial-
penetration limitations of the fixed-point mesh sweep apply.

### Explicit hierarchy synchronization

Use `sat_physics3_sync_sphere_transform` in `physics3_transform.h` to copy
one sphere's world-space position and quaternion orientation to a ROOT node
of the existing transform hierarchy. It uses the exact matrix override,
so the visual node and its descendants update after
`sat_transform3d_evaluate` without reconstructing Euler angles. This is a
one-way, post-step operation; parented targets are rejected and no physics
collision geometry is implicitly transformed or parented.

## Translating finite-mesh platforms

Register an immutable reference-space quad mesh with
`sat_physics3_add_kinematic_mesh(world, mesh, optional_grid,
&initial_offset, material, &id)`. Its `mesh_offset` is the world-space
translation applied to every reference-space vertex; the mesh's vertex and
index buffers are **never modified**. A supplied grid must index the same
untransformed mesh and remains caller-owned. Set the next-tick offset with
`sat_physics3_set_kinematic_mesh_target`. The world caches reference-space AABB extrema when the collider is added,
so translation-bounds preflight is constant-time per tick. The world moves
the mesh through the fixed substeps and runs discrete contacts in reference coordinates, then
resolves each contact with the mesh's world-space frame velocity. Finite
ramp edges, gaps, and grid-based candidate rejection remain intact.

With `sat_physics3_set_mesh_face_ccd(world, 1)`, the same finite face/edge/
vertex sweep operates on the *relative motion* of sphere and translating mesh.
The solver transforms the impact point back into world coordinates at the time
of contact; mesh geometry and spatial grid are not rebuilt. The existing
caller-provided contact capacity must fit the largest static **or kinematic**
mesh face count.

This path supports **translation only**. It does not simulate tilting,
rotating, deforming or jointed collision meshes, nor does it account for
position-dependent velocity induced by angular motion. The sweep remains
a full-mesh scan on both static and translating meshes; benchmarks on real
SH-2 hardware are still needed before enabling it for large moving stages.

## Rotating and tilting finite meshes — bounded discrete slice

`sat_physics3_set_kinematic_mesh_orientation_target(world, id, &quaternion)`
sets an **optional local-origin rotation** for an existing translating
kinematic mesh. The initial orientation is identity. The target must be a
unit 16.16 quaternion with positive shortest-path dot product corresponding
to at most about 45 degrees per tick. The world uses normalized linear
interpolation through its usual bounded substeps; the reference vertices and
optional grid are never modified or rebuilt. Collision queries inverse-rotate
the sphere into reference coordinates, then rotate contact normals back to
world coordinates. Contact resolution accounts for the per-contact-point
motion induced by rotation and translation, instead of assuming the entire
platform has one velocity. Read `actor.mesh_orientation` and
`actor.mesh_offset` to build the matching visual pose independently.

**Rotational CCD is NOT implemented.** When an actor's current/target pose is
non-identity, the engine explicitly retains the discrete substep capacity
guard even if `mesh_face_ccd` is enabled. It does not apply the existing
translation-only sweep to rotating geometry. The initial slice limits
reference coordinates to +/-128 units for rotational actors and rejects
large-angle requests; this avoids silently accepting a rotating sweep as
continuous, but tiny/fast features may still tunnel inside the allowed
substep budget. Rotational contact velocities are computed from the
start/end positions of each reference-space contact point over the tick;
angular acceleration, rotating-mesh edge CCD and deformation are outside
this slice. Use modest geometry/speeds and test on SH-2 hardware.
