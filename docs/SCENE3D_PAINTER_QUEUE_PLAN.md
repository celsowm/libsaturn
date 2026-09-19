# Scene3D deferred painter queue — architecture and acceptance

## Why this belongs in LibSaturn

Skybridge's original `stage_box` submitted gems with platform geometry and then
unconditionally painted the pig last. A camera-only turn made a closer gem
disappear behind a farther pig; the underlying player X/Y/Z never changed.
Sorting one mesh's faces did not sort *different* objects. A library-level
scene queue is a reusable 3D primitive, not a collectible or physics feature.

The existing immediate `sat_scene3d_draw_model`, `sat_draw_mesh` and native
VDP1 interfaces remain intact. The opt-in queue is declared in
`include/saturn/scene3d.h` and implemented in
`src/core/scene3d_api.cpp`, with deterministic no-heap ordering isolated in
`src/core/scene3d_queue_logic.hpp`.

## Public contract (implemented)

- Initialize once with caller-owned `sat_scene3d_queue_item_t[]` and an
  explicit capacity; invalid buffers return `SAT_ERR_INVALID_ARG`.
- Each hardware frame: `sat_scene3d_queue_begin(queue,camera)`, submit
  visible objects, `sat_scene3d_queue_flush(queue)`, then normal VDP1
  `sat_end_frame()`. Flush does *not* start or end the hardware frame.
- `submit_draw` holds a callback, context pointer, world-space anchor and
  painter pass. The callback runs *synchronously during flush* and can call
  the existing native/world-space VDP1 draw APIs. It must not mutate or
  recursively submit to the queue. Caller data must remain valid through
  flush. The queue copies the camera and each anchor.
- `submit_model` copies transform and material descriptors, retains pointers
  to caller-owned model/texture/face arrays and delegates the actual draw to
  the existing immediate `sat_scene3d_draw_model`. Its caller-owned scratch
  scene must be inactive before submission. Queue flush begins/ends the
  scratch scene for each model; it remains reusable for multiple models.
- Entries are ordered by ascending pass, then *descending signed camera-space
  depth*, then original submission sequence. Depth is a full 3D dot product
  against target-eye, including camera pitch: not squared eye distance, world
  Z, or a 2D camera-yaw proxy. Per-item depth is recalculated at submission
  each frame; `queue_depth()` supplies normalized camera depth for culling
  and distance-fade decisions without leaking the ordering algorithm.
- Deterministic in-place heapsort, O(n log n) comparisons and O(1) scratch,
  no `malloc` or `qsort`; each queue item owns its descriptors. Full queue
  returns `SAT_ERR_CAPACITY` *without changing the queue*. On first drawing
  error, flush stops, releases the active state, and the next begin resets
  it; there is no accidental replay of already-submitted VDP1 commands.
- Multiple painter passes are *explicit overrides*. A later pass always
  overdraws an earlier pass. Objects that must occlude each other through
  natural depth changes must use the SAME pass.

Minimal procedural usage:

```c
static sat_scene3d_queue_item_t entries[20];
static sat_scene3d_queue_t queue;

sat_scene3d_queue_init(&queue, entries, 20);

/* Each VDP1 frame, after the game updates the camera: */
sat_scene3d_queue_begin(&queue, &camera);
sat_scene3d_queue_submit_draw(&queue, &platform_position,
    0, draw_platform, platform_context);
sat_scene3d_queue_submit_draw(&queue, &pig_position,
    1, draw_pig, pig_context);
sat_scene3d_queue_submit_draw(&queue, &gem_position,
    1, draw_gem, gem_context);
sat_scene3d_queue_flush(&queue);
```

All return codes must be checked by the caller. The callback has signature
`sat_result_t (*)(void* user, const sat_camera3d_t* camera)`; it can submit
world quads with the passed view-projection matrix, but must not manipulate
the queue being flushed.

## Completed stages

1. **Contract:** separate object existence/visibility from painter ordering;
   define caller-owned queue, callback lifetimes, capacity and pass semantics.
2. **Queue:** pure sorting logic, full camera 3D depth, deterministic stable
   ties, capacity and reentrancy/error behavior.
3. **Model path:** optional deferred `sat_model_asset_t` entry transformed
   from a caller-supplied local-space visual centre, reusing immediate drawing.
   Existing model and direct VDP1 APIs continue working.
4. **Skybridge migration:** two courses submit up to ten deck callbacks, one
   pig, eight gems. The game keeps activation, fade and collectible state;
   `g_items`, `g_actors`, game-specific actor depth and sorting are removed.
   The library owns all object sorting and normalized camera depth.

## Explicit technical limitations — do not overpromise

- The VDP1 has no Z-buffer. A single object-centre anchor cannot guarantee
  correct occlusion of intersecting or very large meshes. The sample uses
  three explicit passes: world decks, supporting deck, pig+gems. The third
  pass gives natural camera-dependent ordering of the pig and gems, but
  *all* actor/platform visibility in arbitrary intersections is not fixed.
- Proper intersecting-platform rendering requires subdivision into faces/
  smaller pieces and may require per-polygon ordering or physically splitting
  intersecting quads. A single total order cannot represent cyclic overlap.
- Distance-faded indexed sprites blend through VDP2 color calculation.
  Earlier VDP1 polygons and VDP2 scenery are distinct compositing targets;
  the painter queue does NOT change hardware semantics or magically provide
  conventional RGBA blending.
- Callback geometry can issue multiple commands; partial VDP1 capacity
  failures cannot roll back commands already emitted to the current frame.
  Queued model data and callbacks cannot be destroyed before flush.
- The 64-bit depth accumulator assumes the same scene-coordinate range as
  the fixed-point math3d library (hundreds of world units, not int32 extrema).
- A caller should partition very large scenes with broad-phase visibility
  tests; the painter queue is *not* a full spatial grid or physics engine.

## Acceptance gates and next work

**Host gate:** `make test` includes `tests/host/test_scene3d_api.cpp`
covering camera-orbit inversion, pitched camera, deterministic equal-depth
ties, pass priority, empty/reusable frame, queue-full behavior, no callback
reentrancy, partial-error closure and a queued compiled model. The separate
Skybridge gameplay tests continue to cover physical coordinates, gems,
moving platforms and both courses. Capture and check *the actual CI result*
for each commit before declaring a gate complete.

**Cross-build gate:** compile the Skybridge ISO and affected examples with
the SH-2 toolchain (`make EXAMPLE=skybridge_3d IP_TEMPLATE_KIND=sbl all`);
preserve command-limit warnings and verify `.init_array`/static-constructor
checks. The repository's `examples-all` gate may also fail on unrelated
examples: distinguish the Skybridge build result from the aggregate result.

**Visual gate (not implied by compilation):** use
`harness/scripts/skybridge_camera_orbit.pad` and the two user-reported camera
views plus a gem within a player's screen silhouette. With identical world
XYZ values, the closer object must be in front. Repeat with a jump, a
collapsed deck and moving Course-2 elevators, both scene passes and
stationary camera fade. Take video or neighboring emulator frames for
flicker checks and compare VDP1 command counts/frame timing with the
pre-queue build. Document the emulator/BIOS/ISO SHA used. Without emulator
access, mark this gate unverified rather than asserting visual success.

**Subsequent work:** expose more granular platform-face submission, bounded
visibility/frustum helpers and perhaps explicit partial ordering for
intersecting meshes only after the visual gate identifies a reproducible
problem. Do not add an implicit generic Z-buffer, per-pixel alpha, a hidden
heap, forced GLB conversion, or scene physics to this API.
