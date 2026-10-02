# LibSaturn — Sonic-Class 2D Runtime Refactor Plan

**Status (2026-09-29):** Planned. The recent arbitrary 2D tile-slope work has landed, but the broader surface-oriented runtime described here has not yet been implemented.
Reviewed 2026-10-02 against the repository and the local `.external/sa2` tree; the
corrections are folded into the sections below and the evidence is recorded in
section 37.

**Purpose:** Evolve LibSaturn so a future high-speed 2D platformer with the architectural demands of Sonic Advance 2 can be implemented cleanly without embedding Sonic-specific gameplay in the library and without building a second engine beside LibSaturn.

**Primary external reference:** .external/sa2

The implementation harness/worktree is expected to contain a local Sonic Advance 2 decompilation under .external/sa2. Treat that tree as **read-only reference material**. It may be absent from the GitHub repository itself and MUST NOT become a runtime dependency, build dependency, vendored public API, or modified subtree. Internet access is not required for the SA2 analysis when this local tree is present.

Breaking changes are acceptable when they remove a weak abstraction. Preserve useful simple APIs, lower abstraction levels, deterministic behavior, explicit ownership, bounded memory and Saturn hardware constraints.

---

## 1. Architectural thesis

The existing 2D physics path is appropriate for ordinary platformers:

- sat_body2_t provides an AABB body and world-space velocity;
- sat_body2_step provides basic integration;
- sat_body2_move_tiles resolves solid/one-way tiles;
- sat_body2_move_tiles_surface now supports parameterized linear tile surfaces;
- SAT_TILE_SLOPE_UP and SAT_TILE_SLOPE_DOWN remain 45-degree convenience presets;
- sat_body2_move_boxes handles simple box collision;
- sat_spatial_t already provides a caller-owned uniform-grid broad phase.

Do **not** keep growing this API until it becomes a Sonic engine.

The target architecture is a spectrum:

1. simple body/tile solver for small games;
2. generic oriented-terrain query layer;
3. optional surface-oriented character controller;
4. optional static/kinematic 2D world and support tracking;
5. generic triggers, paths, large-map streaming, camera policy, entity activation and animation runtime;
6. existing low-level VDP1/VDP2 APIs remain available independently.

The advanced layer must reuse lower-level math/collision primitives where semantics coincide, but it must not make the simple layer depend on the advanced runtime.

---

## 2. SA2 reference areas and lessons to extract

The implementation harness must inspect .external/sa2 directly before freezing APIs.

### 2.1 Terrain representation and directional collision

Primary references:

- .external/sa2/include/core.h
- .external/sa2/include/game/shared/stage/terrain_collision.h
- .external/sa2/src/game/shared/stage/terrain_collision.c
- .external/sa2/src/game/shared/stage/collision_1.c
- .external/sa2/src/game/shared/stage/collision_2.c
- .external/sa2/src/game/shared/stage/collision_3.c

Requirements to extract:

- reusable height-map collision profiles;
- per-tile rotation/orientation;
- metatile-based maps;
- multiple collision maps/layers;
- directional probes rather than only downward floor collision;
- collision queries reused by players, enemies, projectiles and stage objects;
- surface rotation/orientation returned to movement logic;
- floor, walls and ceilings represented as oriented surfaces instead of separate game-specific tile enums.

LibSaturn must capture these generic requirements without copying SA2 data formats.

**Audit correction (2026-10-02):** `terrain_collision.h` holds only prototypes with
decompiler names (`sub_801EB44`, ...); it is not where the data layout lives. The
layout is the `Collision` struct in `core.h`. The terrain algorithms are all in
`terrain_collision.c` (the tile sensors) plus the player sensor wrappers in `player.c`
(`sub_8022F58`, `sub_8029BB8`, `sub_802195C` and siblings). `collision_1.c` to
`collision_3.c` are **object** collision (player against platforms, springs, enemies,
hit boxes, damage), not terrain; `Coll_Player_Platform` in `collision_1.c` is the
moving-support reference. The measured details are in section 37.3.

### 2.2 Player movement

Primary references:

- .external/sa2/include/game/shared/stage/player.h
- .external/sa2/src/game/shared/stage/player.c
- character-specific player files where useful

Generic observations:

- ground-relative speed is distinct from air X/Y velocity;
- player orientation/rotation is persistent state;
- movement state and support state are separate;
- collision layer can change at runtime;
- support can be a moving object;
- landing/detach convert between surface-relative and world-relative motion.

LibSaturn must not implement Sonic acceleration constants, boost, spin dash, rings, damage, abilities or animation state.

### 2.3 Layer switching

Reference:

- .external/sa2/src/game/sa2/stage/interactables/leaf_forest/toggle_player_layer.c

Requirement:

Collision filtering needs generic layers/masks and runtime selection. “Foreground” and “background” are game meanings, not LibSaturn enums.

### 2.4 Authored constrained movement

References:

- .external/sa2/src/game/sa2/stage/interactables/grind_rail.c
- .external/sa2/src/game/sa2/stage/interactables/leaf_forest/corkscrew.c
- .external/sa2/src/game/sa2/stage/interactables/leaf_forest/gapped_loop.c

Requirements:

- generic deterministic paths;
- position/tangent/normal sampling;
- signed advancement;
- circle/arc/Bezier-like authored trajectories;
- triggers that can transfer gameplay control to a path follower.

Do not add SAT_TILE_LOOP, SAT_TILE_RAIL, sat_sonic_loop or equivalent game concepts.

### 2.5 Camera

References:

- .external/sa2/include/game/shared/stage/camera.h
- .external/sa2/src/game/shared/stage/camera.c
- .external/sa2/src/game/shared/stage/screen_shake.c
- boss/stage code that changes camera bounds

Requirements:

- follow policy is distinct from render transform;
- dead zones/shifts;
- world clamps;
- temporary/scripted clamps;
- camera delta/velocity;
- screen shake;
- visibility and activation ranges.

LibSaturn already has sat_camera2d_t for rendering. Add policy above it.

### 2.6 Regional entities and collectibles

References:

- .external/sa2/src/game/shared/stage/entities_manager.c
- .external/sa2/src/game/shared/stage/rings_manager.c
- .external/sa2/include/game/shared/stage/entity.h

Requirements:

- many static descriptors remain inactive;
- activation is camera/spatial-region based;
- descriptors become live objects and can return to inactive state;
- collectibles are spatially indexed instead of globally scanned.

This motivates a lightweight region activation layer, not an ECS.

Audit notes: SA2 stores static entities in a per-region table of start indices
(`READ_START_INDEX` in `rings_manager.c`), an offline-built read-only structure. It
also overwrites each descriptor's `x` byte with a state value
(`MAP_ENTITY_STATE_INITIALIZED`), so its descriptors are not immutable. LibSaturn
keeps descriptors immutable and tracks activation in a separate bounded structure
(see section 17).

### 2.7 Gameplay tasks

Inspect:

- .external/sa2/include/task.h
- .external/sa2/src/task.c
- representative enemies/interactables

Requirements:

- bounded scheduling;
- deterministic order/priority;
- task-local state;
- safe destroy during update;
- optional destructor;
- update-function replacement as state transition.

This is distinct from sat_parallel.

Audit notes: SA2's tasks are a priority-sorted doubly-linked list capped at 128
(`MAX_TASK_NUM`), with a destructor, task-local data and `TASK_DESTROY_DISABLED`.
SA2 even runs screen shake as a task. LibSaturn takes the semantics (priority order,
safe destroy, destructor) but not the linked list; see section 18.

### 2.8 Sprites and animation

Inspect:

- .external/sa2/include/sprite.h
- animation update code
- representative player/enemy/interactable animation users

Requirements:

- real clip playback;
- frame advancement and completion;
- variants;
- frame metadata/hitboxes;
- pivot/origin;
- predictable VRAM/resource ownership.

---

## 3. Non-negotiable design rules

1. No Sonic-specific public API.
2. Keep simple physics simple.
3. Public API remains C-friendly; implementation may be C++.
4. No mandatory runtime heap.
5. Deterministic traversal/results.
6. No floating point in Saturn gameplay runtime.
7. No giant ECS or platformer-runtime god object.
8. Reuse render2d, texture handles, VDP2 layers, spatial broadphase, resource planning and existing collision math.
9. Prefer offline preprocessing.
10. Master SH-2 correctness first; Slave SH-2 is optional optimization.
11. Capacity/error behavior must be explicit.
12. Multiple abstraction levels remain first-class and independently usable.

---

## 4. Proposed module boundaries

Final names may change after repository audit, but responsibilities should remain narrow.

Potential modules:

Final names (frozen by Phase 0, see section 37.8):

- saturn/math2d.h — binary angles, sine/cosine tables, 2D dot/length/normalize/atan2/lerp;
- saturn/terrain2.h — oriented terrain/profile queries;
- saturn/character2.h — optional surface-oriented character controller;
- saturn/physics2_world.h — static/kinematic colliders, sensors and support handles
  (sensors live here, not in a separate module);
- saturn/path2.h — deterministic 2D paths;
- saturn/follow_camera2d.h — camera policy above render2d, including screen shake;
- saturn/stage_map2.h — logical large-map/metatile streaming to VDP2;
- saturn/entity_stream2.h — static descriptor activation/deactivation;
- saturn/task.h — bounded gameplay scheduler;
- saturn/sprite_clip.h — animation clip player, separate from the simple sprite_anim.h.

Lower-level modules must not require higher-level runtime initialization.

Header dependency note: `sat_vec2_t` lives in `collide2d.h`, but the scalar
fixed-point functions (`sat_fx16_mul`, `sat_fx16_div`, `sat_fx16_sqrt`, `sat_sin_deg`)
live in `math3d.h`. A 2D-only module such as terrain2 would otherwise include a 3D
header. Phase 0 decides whether to split a scalar fixed-point header out (preferred)
or accept the include; Phase 12 re-checks it.

---

## 5. Terrain2: oriented terrain queries

Terrain2 must answer:

- distance to support along arbitrary direction;
- point of contact;
- surface normal;
- surface tangent;
- layer/category/material metadata;
- collision flags;
- stable collider/profile identity where useful.

A conceptual hit result may contain:

~~~c
typedef struct sat_terrain_hit2 {
    sat_vec2_t point;
    sat_vec2_t normal;
    sat_vec2_t tangent;
    sat_fx16_t distance;
    uint32_t flags;
    uint32_t material;
    uint16_t collider_id;
    uint8_t layer;
    uint8_t reserved;
} sat_terrain_hit2_t;
~~~

Do not freeze this exact layout before checking alignment and existing math types.

Design note from the SA2 audit (section 37.3): SA2 senses in four axis-aligned
directions chosen by quantizing the character's current angle, with a table-driven
O(1) lookup per tile, and returns a whole-pixel signed distance. Arbitrary-direction
probing is not what the reference needs. Decide in Phase 0 whether the four
axis-aligned probes are the primary fast path (with arbitrary direction as a slower
generic form) or whether arbitrary direction is dropped; do not implement a general
ray march for the hot path by default.

Required query forms:

- downward ground probe;
- upward ceiling probe;
- left/right wall probe;
- arbitrary gravity-direction probe;
- arbitrary surface-normal probe;
- local surface sampling;
- layer/category filtering.

Terrain queries must be usable by enemies and generic objects without using character2.

Performance requirements:

- resolve only relevant map/profile cells;
- fixed-point/integer math;
- bounded work;
- avoid full-map scans;
- retain O(1) or close-to-O(1) local profile sampling where practical.

---

## 6. Terrain profiles beyond linear slopes

sat_tile_surface_t remains the simple linear-slope path.

The advanced terrain system needs reusable local profiles comparable in capability to SA2 height maps.

Choose a compact representation after measuring memory and SH-2 cost. Possible forms include:

- one integer height per local pixel;
- lower-resolution samples with deterministic interpolation;
- compact span/segment representation;
- pre-expanded orientations when cheaper than runtime transforms.

Requirements:

- deterministic;
- read-only generated data;
- rotatable/flippable;
- floor/wall/ceiling use;
- no runtime allocation;
- offline validation;
- no angle-enum explosion.

---

## 7. Layers, categories, masks and metadata

Introduce game-defined filtering bits for:

- active layer/layer mask;
- category;
- collide-with mask;
- sensor-only state where useful;
- material/user flags.

The library owns filtering mechanics; the game owns bit meanings.

Queries should reject irrelevant candidates before narrowphase.

Acceptance:

- same world coordinate may resolve against different terrain layers;
- runtime layer switching works;
- sensors and solids may overlap without incorrect collision response.

---

## 8. Character2 surface controller

Character2 owns generic support/motion resolution, not gameplay rules.

Potential state includes:

- world position;
- air velocity;
- scalar ground/tangent speed;
- support normal;
- support tangent;
- support handle/ID;
- supported/airborne flags.

Required behavior:

- support tracking;
- arbitrary oriented support;
- ground-relative scalar speed;
- tangent/world velocity conversion;
- floor -> slope -> wall -> ceiling continuity;
- landing;
- detach;
- optional adhesion;
- configurable snap distance;
- configurable maximum surface-angle discontinuity;
- configurable minimum speed for steep/inverted attachment;
- blocking against non-support walls/ceilings;
- small step/gap handling;
- externally supplied gravity/acceleration.

Gameplay remains responsible for acceleration, rolling, jump decisions, boost, abilities, input, damage and animation.

Reference behavior to cover (from the SA2 audit): two parallel sensors a few pixels
apart along the surface, the nearer result wins and supplies the surface angle;
a "no angle information" tile value means flat; support is accepted or refused by
comparing the sensor distance with a small pixel threshold (4 px in SA2's jump and
detach checks); and gravity inversion is a mirrored mode, not separate code. The
sensor spacing and the thresholds are configuration of the controller, not constants
of the library.

“Grounded” in this advanced system means “has valid support,” not “standing on world-up floor.”

---

## 9. Generalized one-way surfaces

Keep simple SAT_TILE_ONE_WAY behavior for ordinary platformers.

Advanced one-way collision is relative to the surface normal:

- block from one side;
- pass from the other;
- work for horizontal, vertical and sloped surfaces;
- compose with layers/masks.

Add host tests for all three orientations.

---

## 10. Physics2 world and moving support

An optional 2D world should provide stable identity for non-tile colliders.

Collider classes:

- static;
- kinematic;
- sensor.

Support behavior:

- stable support handle;
- previous/current transform;
- platform delta;
- platform velocity;
- carry supported characters;
- configurable support velocity contribution on detach;
- support invalidation;
- one-way moving platforms;
- vertical/downward moving support;
- crush detection only if it can be defined generically and cleanly.

Study physics3_world for ownership/handle lessons without blindly copying its design.

SA2 reference (`Coll_Player_Platform`): support is re-established every frame by
testing the player's rectangle against the platform's hit box; the platform
identity is a raw pointer (`stoodObj`); losing contact simply returns the player to
the airborne state. There is no carry delta, no support velocity on detach and no
stale-handle protection, so those requirements are new design work in LibSaturn,
not something to extract from SA2.

All capacities/storage must be explicit.

---

## 11. Triggers and sensors

Provide a lightweight generic sensor path with at least AABB volumes.

Required semantics:

- overlap query;
- layer/category filter;
- enter;
- stay if practical;
- leave;
- safe removal;
- deterministic event ordering;
- high-speed crossing support via sweep or previous/current interval logic.

Use sat_spatial_t or shared world broadphase instead of O(n) scans.

---

## 12. Path2

Provide deterministic fixed-point primitives:

- line;
- polyline;
- circular arc;
- circle;
- quadratic Bezier;
- cubic Bezier.

Capabilities:

- sample position;
- tangent;
- normal where meaningful;
- signed advance;
- clamp;
- wrap;
- nearest-point approximation;
- optional offline arc-length tables for expensive curves.

Use cases: rails, camera tracks, moving platforms, enemy patrols, pipes and scripted trajectories.

No Sonic-specific names.

---

## 13. Fixed-point math audit

Before adding helpers, inspect existing math.

Likely reusable additions:

- 2D dot/project/reject;
- tangent from normal;
- deterministic normalization with defined zero behavior;
- fixed-point vector/angle conversion;
- signed orientation/angle tests;
- fixed-point lerp;
- curve evaluation;
- distance/progress helpers.

Document overflow ranges and avoid expensive divisions in hot paths where precomputed forms are practical.

Current inventory (2026-10-02): `collide2d.h` has 2D add, sub, scale, reflect,
approach, box/circle tests, raycast and sweep. `math3d.h` has `sat_fx16_mul`,
`sat_fx16_div`, `sat_fx16_sqrt` and degree-based `sat_sin_deg`, `sat_cos_deg`,
`sat_tan_deg`. There is no 2D dot/length/normalize, no atan2 and no binary-angle type.

**Decision required in Phase 0 (blocks Terrain2 and Character2):** the angle
representation. SA2 uses a 256-step unsigned byte angle (`u8 rotation`) with
lookup tables. Recommended: a byte or 16-bit binary angle with a precomputed
sine/unit-vector table, which avoids divisions and square roots in the hot path;
degree-based functions stay for the existing 3D code. Whatever is chosen, normals and
tangents for authored terrain are precomputed offline.

---

## 14. Follow Camera2D

render2d owns the final camera transform. This module owns follow policy.

Required:

- target follow;
- dead zone;
- horizontal/vertical look-ahead;
- velocity-dependent look-ahead;
- smoothing/lag;
- world bounds;
- temporary runtime clamps;
- snap/reset;
- screen-shake composition;
- viewport size;
- world visible bounds;
- activation bounds;
- prefetch bounds;
- optional asymmetric predictive margin.

Provide helpers so game code does not repeatedly implement camera-range checks and world-to-screen culling math.

The controller should output sat_camera2d_t or data directly consumable by sat_render2d_set_camera.

---

## 15. Screen shake

If current runtime code does not already provide a correct generic facility, add a deterministic component with:

- duration;
- X/Y amplitude;
- axis mask;
- decay/envelope;
- optional frequency;
- deterministic seed/phase.

Shake composes with follow/clamp policy instead of modifying every object's screen coordinate.

SA2 reference (`screen_shake.c`): amplitude decays by a fixed step per frame, a frame
counter bounds the duration, a phase advances per frame through a sine table or a
pseudo-random value, flags select the X and Y axes and a positive-only or
negative-only sign constraint, and an option updates only every other frame. The
result is stored as an offset on the camera (`shakeOffsetX/Y`) and cleared when the
shake ends. The component above covers this; add the sign constraint and the
update-every-N-frames option, and keep the shake a plain value that needs no task.

---

## 16. Large stage/metatile streaming

Build an optional high-level map streamer on top of existing VDP2 APIs.

Do not replace low-level VDP2 control.

Content model:

- tileset;
- metatiles;
- large logical map;
- multiple logical layers;
- X/Y tile flip;
- palette selection;
- optional animated-tile metadata;
- explicit bounds.

Runtime:

- camera selects logical visible window;
- incremental motion uploads only newly visible rows/columns;
- large teleports rebuild bounded window safely;
- dirty regions are explicit;
- CPU staging is caller-owned;
- VRAM/cycle ownership remains consistent with current VDP2 layer manager.

Prefer ring/window streaming instead of full visible-map rebuilds.

At least two independent logical map layers should be easy to use, without naming them foreground/background in the generic API.

---

## 17. Entity region activation

Create a static-descriptor streamer, not an ECS.

Inputs:

- immutable spawn descriptors;
- spatial partition/index;
- activation bounds;
- optional predictive extension.

Lifecycle:

- activate callback;
- deactivate callback or explicit inactive set;
- deterministic descriptor order;
- hysteresis;
- no duplicate activation;
- caller-owned live object pool;
- persistent gameplay state remains game-owned.

It must handle a camera jump across multiple regions in one frame.

Index and state: the descriptors are static, so the index is built offline, either
a region-sorted descriptor array with a per-region start-index table (as SA2 does) or
an equivalent compact form. Do not build it at runtime with `sat_spatial_t`, which
is a dynamic insert/clear structure suited to colliders. Activation state lives in a
separate caller-owned bitset (one bit per descriptor), not in the descriptor. The
bitset size and the region table size are part of the resource accounting in
section 25.

---

## 18. Gameplay task scheduler

Audit existing runtime scheduling first.

If no suitable generic scheduler exists, add one distinct from sat_parallel.

Audit result (2026-10-02): no gameplay scheduler exists; `parallel.h` schedules work
across the two SH-2s and is not a substitute. This is also the module most at risk of
growing into a framework, so it is built last and only if the integration example or
a real consumer needs it. Use a fixed array with generation-checked handles; do not
copy SA2's linked list.

Required:

- fixed capacity;
- deterministic priority/order;
- generation-checked handles if appropriate;
- task-local caller-known storage;
- optional destructor;
- safe self-destroy;
- safe peer destroy;
- update-function replacement;
- stale-handle/capacity errors;
- no hidden allocation.

No terrain/camera/physics module should require this scheduler.

---

## 19. Sprite animation runtime

Keep simple existing frame-selection helpers useful.

Add a real animation-player abstraction with:

- clips;
- frames;
- per-frame duration;
- source region;
- pivot/origin;
- optional generic rectangles/markers;
- optional frame event IDs;
- loop;
- one-shot;
- optional ping-pong;
- playback rate;
- pause/resume;
- finished state;
- restart/switch semantics.

Generic metadata must be sufficient for game-defined hurtboxes, attack boxes, interaction rectangles or attachment points without those meanings appearing in LibSaturn.

Integrate with logical texture regions/resource planning, not raw VRAM addresses.

---

## 20. Offline asset pipeline

The runtime must not parse SA2/GBA structures.

Use .external/sa2 only as reference and, optionally, as input to development-only import tools that emit generic LibSaturn assets.

Build generic host tools for:

- terrain profile conversion;
- rotated/flipped profile preparation;
- normal/tangent preprocessing where useful;
- metatile compilation;
- logical map packing;
- entity region partitioning;
- sprite atlas/frame metadata;
- animation clips;
- path preprocessing/arc-length data;
- resource-size validation.

Generated binary formats require bounds checks, documented alignment, deterministic output and version/magic where appropriate.

Provenance rule: no data derived from SA2 (extracted tables, maps, graphics,
converted output) may be committed to this repository. Development-only import tools
may read `.external/sa2` locally; the committed test fixtures and example assets are
original or synthetic.

---

## 21. Spatial acceleration

Reuse sat_spatial_t for dynamic colliders and sensors (insert/clear per frame).
Static descriptor regions use the offline index described in section 17 instead.

Extend it only for real consumers, potentially with:

- deterministic result ordering;
- callback/early-exit query;
- swept AABB query;
- efficient dynamic updates.

Avoid:

- every entity vs every entity;
- every character probe vs every collider;
- every collectible scanned every frame.

Document expected complexity and worst cases.

---

## 22. Surface material metadata

Terrain/collider hits should expose compact game-defined material/flags/user tags.

Gameplay may interpret these as ice, conveyor, hazard, sticky ground, water-running surface, etc.

LibSaturn must not implement those policies.

---

## 23. VDP2 background/raster audit

Inventory existing VDP2 capabilities before adding anything.

Map SA2 requirements against current:

- normal scroll layers;
- line scroll;
- vertical cell scroll;
- windows;
- color calculation;
- color offset;
- RBG0;
- palette APIs.

Add only missing generic convenience layers such as:

- camera-driven parallax;
- bounded line-table generators;
- stage-map layer integration;
- palette cycling helpers.

Raw VDP2 APIs remain available.

---

## 24. Palette animation

If current palette APIs make cycling cumbersome, add bounded generic operations:

- update subrange;
- rotate/cycle range;
- timed sequence with caller-owned state.

Do not re-upload texture pixels for palette-only effects.

Respect CRAM ownership and frame visibility.

---

## 25. Resource-plan integration

Extend resource planning where quantities are meaningful:

- terrain profile bytes;
- terrain map bytes;
- collider slots;
- spatial entries;
- task slots/state bytes;
- entity descriptors/regions;
- stage-stream scratch;
- VDP2 map-window storage;
- animation metadata;
- prepared texture regions.

resource_plan stays a planner, not allocator.

Mapping note: `sat_resource_kind_t` enumerates memory regions (main RAM, WRAM, VDP1
commands and VRAM, VDP2 VRAM, CRAM, audio staging, RAM cart), not object types.
Collider slots, task slots, descriptor bitsets and similar quantities are therefore
expressed as bytes charged to the region that holds them (normally main RAM or WRAM),
with the quantity stated in the entry's documentation, and no new kinds are added.
Only the VDP2 map-window storage and prepared texture regions map to existing VRAM
kinds directly. Each module exposes a `*_requirements()` helper in the style of
`sat_resource_plan_requirements`.

---

## 26. High-speed movement and tunneling

Audit:

- body/tile sweep;
- terrain probes;
- moving platforms;
- sensors;
- support reacquisition.

Prefer swept tests, conservative advancement, bounded movement segmentation and predictive probes.

Do not solve high-speed motion by blindly multiplying full physics substeps.

Measure expensive 64-bit/division hot paths on SH-2 where practical.

SA2's tile lookup (`sub_801EF94`) caches the last divide-by-12 for the X and Y tile
coordinates because dividing by the metatile size is costly. LibSaturn should avoid
the division entirely: power-of-two metatile dimensions, or a precomputed reciprocal
or lookup table, chosen in the Phase 1 profile format decision.

---

## 27. Explicit anti-goals

Do not add:

~~~c
SAT_TILE_LOOP
SAT_TILE_RAIL
SAT_TILE_CORKSCREW
SAT_TILE_SPRING
SAT_TILE_BOOSTER
SAT_TILE_WATER
SAT_TILE_ICE
~~~

Do not add angle-specific slope enums.

Do not add a Sonic Player class.

Do not add a giant ECS.

Do not make stage_map depend on character2.

Do not make terrain2 depend on render2d.

Do not require task scheduling for paths or animation.

---

## 28. Host-test matrix

### Terrain2

- flat ground;
- legacy 45-degree parity;
- arbitrary linear slope;
- shallow/steep slope;
- irregular profile;
- rotated/flipped profile;
- wall;
- ceiling;
- arbitrary cast direction;
- layer/category filtering;
- material/flags;
- one-way normal behavior;
- adjacent-profile continuity.

### Character2

- flat support;
- slope traversal;
- floor -> wall;
- wall -> ceiling;
- ceiling -> wall;
- support discontinuity;
- detach threshold;
- landing;
- tangent/world velocity conversion;
- collision layer switch;
- support removal;
- high-speed travel.

### Physics2 world

- static collider;
- moving horizontal/vertical platform;
- carry delta;
- support velocity on detach;
- one-way moving platform;
- invalid support handle;
- capacity/stale handle.

### Sensors

- enter/stay/leave;
- removal while overlapping;
- category filter;
- high-speed crossing;
- deterministic ordering.

### Path2

- line/polyline boundaries;
- arc endpoints;
- circle wrap;
- quadratic/cubic endpoints;
- tangent;
- signed advance;
- clamp;
- nearest point;
- repeatability.

### Camera policy

- dead zone;
- look-ahead;
- clamps;
- changing clamps;
- snap;
- shake;
- visible bounds;
- predictive activation bounds.

### Stage map

- no movement produces no unnecessary writes;
- one-tile X/Y movement;
- diagonal movement;
- multi-cell movement;
- teleport/rebuild;
- multiple layers;
- bounds;
- dirty accounting.

### Entity stream

- activate/deactivate once;
- reactivation;
- hysteresis;
- camera teleport;
- prediction margin;
- stable order;
- capacity refusal.

### Task scheduler

- create/destroy;
- stable priority;
- self/peer destroy;
- update replacement;
- destructor;
- capacity;
- stale handle.

### Animation

- variable duration;
- loop;
- one-shot;
- finished;
- restart;
- clip switch;
- speed;
- pause;
- optional ping-pong;
- frame events;
- pivot/metadata.

---

## 29. Integration example

Add an original example such as:

examples/high_speed_platformer

Do not reproduce Sonic assets, character design, stage layout or presentation.

### 29.1 Lifecycle: incubate in-tree, then extract

The example starts in this repository and is later moved to its own repository, the
way the Ikemen example moved to
[celsowm/ikemen-saturn](https://github.com/celsowm/ikemen-saturn). The two stages
have different rules.

**Incubation (in-tree):** the example grows alongside the library phases. A minimal
skeleton may exist as soon as Phase 1 lands, and each later phase adds the stress
it needs. This is where API problems are found, so a gap in the library is fixed in
the library, never worked around in the example.

**Extraction:** after the public APIs are stable (Phase 12 audit passed), the example
moves to an independent repository that consumes LibSaturn only through the installed
CMake/Conan package. The library keeps no knowledge of the example afterwards. The
steps and acceptance checks are Phase 13.

### 29.2 Rules that keep extraction cheap

Applied from the first commit of the example, not at extraction time:

1. Include only public `saturn/*` headers. Never `src/`, never a private header,
   never another example's file.
2. No dependency on library-internal build fragments. The example owns its build
   rules, assets pipeline and generated-asset directory (generated data lives under
   the build tree, not in the source tree).
3. Any generic helper the example needs is either promoted into LibSaturn (when it is
   generic and has a second plausible consumer) or stays private to the example. It is
   never shared by reaching into another example.
4. Game-specific code (character tuning, acceleration, abilities, stage content,
   art) stays in the example. LibSaturn gets mechanisms, not this game's policy.
5. Every file the build needs must be tracked. Check git-ignore rules for generated or
   binary inputs (the Ikemen extraction found a required boot template that `*.bin`
   had silently ignored).
6. All example assets are original or synthetic, so the repository can be public.
7. Keep a boundary check, equivalent to ikemen-saturn's `test_consumer_contract.py`,
   that fails on forbidden includes or build references. It can be a host test inside
   the example from the start.

Stress:

- high horizontal speed;
- linear and profiled slopes;
- floor/wall/ceiling traversal;
- moving support;
- one-way support;
- layer switching trigger;
- generic circular/Bezier path;
- rail-like path follower;
- generic collectibles;
- regional entity activation;
- large logical map;
- camera look-ahead/bounds/shake;
- sprite animation;
- multiple VDP2 layers/parallax.

This is an API acceptance test, not a game framework.

---

## 30. Implementation phases

Every phase must update this document with actual APIs, files, status and deviations.

Ordering and dependencies: Phases 1 and 2 are the risky ones (Character2 depends on
Terrain2). Phases 4, 5, 7 and 9 are independent of them and of each other and can
land in any order. Phase 3 depends on the Phase 0 decision about sharing a broadphase.
Phase 8 is last and conditional. Phase 11 needs everything it exercises.

### Phase 0 — Repository and SA2 audit
**Status:** Complete (2026-10-02). Audit in section 37; decisions and final names in
section 37.8. The SH-2 cycle baseline was not taken; each phase measures its own hot
paths instead (section 37.5).

- inspect current LibSaturn 2D/VDP2/spatial/resource architecture;
- inspect the referenced .external/sa2 systems and additional relevant code;
- record concrete requirements;
- freeze module dependency direction;
- establish current physics/runtime baselines where practical.

**Gate:** update this plan with final names/layout before public advanced API implementation.

### Phase 1 — Terrain2 foundation
**Status:** Complete (2026-10-02). `math2d` and `terrain2` landed with host tests; the
cross build for SH-2 and the full `make test` pass.

Deliver profiles, orientation, masks, arbitrary-direction probes, normals/tangents, metadata, one-way semantics and tests.

**Gate:** generic enemies/objects can query terrain without character2.

Delivered:

- `include/saturn/math2d.h`, `src/core/math2d/{tables.hpp,logic.hpp,api.cpp}`: `sat_angle_t`
  (256 per turn) and `sat_angle16_t`, `sat_sin8/cos8/sin16/cos16`, `sat_atan2_8/16`,
  `sat_angle_diff`, `sat_angle_quadrant`, `sat_vec2_dot/length/normalize/perp/project/reject`,
  `sat_lerp_fx16`, `sat_vec2_lerp`. Test: `tests/host/test_math2d.cpp`.
- `include/saturn/terrain2.h`, `src/physics/2d/{terrain2_logic.hpp,terrain2.cpp}`:
  `sat_terrain_profile2_t` (22 bytes), `sat_terrain_map2_t` (metatile table plus up to four
  layers, power-of-two metatiles, explicit outside policy empty/solid/clamp),
  `sat_terrain_query2_t` (layer, category mask, ignore flags), `sat_terrain_hit2_t`
  (point, normal, tangent, distance, flags, material, tile, collider id, layer, angle).
  Functions: `sat_terrain2_probe` (axis-aligned, O(range/8) tile reads, signed whole-pixel
  distance, negative when the sensor is already inside), `sat_terrain2_cast` (arbitrary
  direction, dominant-axis pixel march, at most 512 steps), `sat_terrain2_sample`,
  `sat_terrain2_solid_at`, plus `sat_terrain_profile2_from_columns/validate` and
  `sat_terrain_map2_init/add_layer/set_outside/validate/requirements`. Test:
  `tests/host/test_terrain2.cpp` (flat ground, legacy 45-degree parity, shallow/steep/stair
  slopes, all four flip combinations, walls, ceilings, layers, category and flag filters,
  material and flags, one-way, cast, outside policies, adjacent-profile continuity, and an
  exhaustive comparison of every probe against a brute-force pixel oracle over three random
  maps and all four directions).
- Wired into `cmake/LibSaturnSources.cmake`, the Makefile host tests, `saturn.h` and
  `docs/PUBLIC_API_OWNERSHIP.md`.

Deviations and decisions made while implementing:

- A profile's rows must be a single run anchored to a side, so shapes with a valley or a hill
  (a row with a hole) are rejected by `from_columns` and `validate`; split them across tiles.
  This matches SA2, where each row stores one signed extent.
- A one-way profile's blocking direction follows its flipped surface normal, so a Y-flipped
  platform catches upward probes only. A point query (`solid_at`) has no direction, so it never
  reports a one-way profile as solid; `sample` does report it.
- When a profile's own surface does not face the probe (a floor tile hit from the side, the map
  edge), the hit reports the face the probe actually ran into (angle from the probe direction),
  not the stored angle. A hit on the outside of a solid map edge has `collider_id` 0xFFFF.
- `range` counts pixels examined (1..255): a first solid pixel at offset k needs range > k, and a
  sensor deeper in solid than range returns `SAT_ERR_NOT_FOUND`.
- Open for later phases: none of the sampling is sub-pixel; Character2 (Phase 2) combines two
  probes for sub-pixel-looking angles.

### Phase 2 — Character2
**Status:** Complete (2026-10-02). `character2` landed with host tests; the cross build for
SH-2 and the full `make test` pass.

Deliver support state, tangent speed, air velocity, landing/detach, floor/wall/ceiling continuity, configurable adhesion/snap policy and tests.

**Gate:** host simulation traverses a loop-like oriented test surface without game-specific collision code.

Delivered:

- `include/saturn/character2.h`, `src/physics/2d/{character2_logic.hpp,character2.cpp}`:
  `sat_character2_t` (position of the feet contact point, `air_velocity`, `ground_speed`,
  `support_normal/tangent/id/angle`, `flags`, `layer`), `sat_character2_config_t` (foot, wall
  and head sensor geometry, `step_up`, `snap_down`, `max_angle_step`, `steep_angle` plus
  `min_steep_speed`, `ceiling_attach`, `gravity_quadrant`, `segment_px`, `max_segments`,
  category mask and ignore flags), `sat_character2_result_t` (events, velocity before the step,
  surface angle). Functions: `init`, `config_default`, `config_validate`, `is_supported`,
  `world_velocity`, `attach`, `detach`, `step`. Events: LANDED, DETACHED, HIT_WALL, HIT_CEILING,
  SLIPPED. The game owns acceleration, slope force, jump and abilities: it edits `ground_speed`
  or `air_velocity` before each step.
- `tests/host/test_character2.cpp` over the shared `tests/host/terrain2_test_world.hpp` (the
  profile table and world builder also used by `test_terrain2`): flat run, 45-degree ramp up and
  down, gaps narrower and wider than the feet, walls, 8 px step climbed or blocking, a full
  rectangular loop with 45-degree corners in both directions (floor, both walls, ceiling,
  every mode switch, exact ground speed kept), slipping off a wall at low speed, landing velocity
  conversion on flat and sloped ground, detach/attach helpers, layer switching, flat and slanted
  ceilings, 30 and 60 px/step movement without tunnelling, inverted gravity as a mirrored world,
  argument validation and determinism.
- Wired into `cmake/LibSaturnSources.cmake`, the Makefile host tests, `saturn.h` and
  `docs/PUBLIC_API_OWNERSHIP.md`.

Deviations and decisions made while implementing:

- Support is found with two foot sensors plus a centre sensor: the feet pick the surface angle
  and the nearer surface, the centre line gives the contact point. Without it the character rode
  up to the foot half-width above a 45-degree slope; a ledge under one foot only keeps that
  foot's contact.
- Mode transitions are driven by what the sensors see, with the usual overlap (floor 224..32,
  walls 33..95 and 161..223, ceiling 96..160). A surface ahead in the support's own mode is a slope
  left to the foot sensors; one in another mode within `max_angle_step` of the support (a quarter
  pipe, a ramp meeting a wall) becomes the support at once; anything steeper is a wall.
- Only a shallow overlap of `step_up` pixels is recovered. A character buried deeper (for
  example after the game switches its layer inside geometry) is not rescued: `attach` returns
  `SAT_ERR_NOT_FOUND`.
- Gravity direction is a configuration quadrant, not a flag on the character, so inverted
  gravity is the same code in a mirrored frame.

### Phase 3 — Physics2 kinematics and sensors
**Status:** Complete (2026-10-02). `physics2_world` landed with host tests; the cross build for
SH-2 and the full `make test` pass.

Deliver stable collider ownership, moving support/carry, sensors, broadphase integration and tests.

Delivered:

- `include/saturn/physics2_world.h`, `src/physics/2d/physics2_world.cpp`: colliders are
  axis-aligned boxes of three classes (static, kinematic, sensor) named by a generation-checked
  `sat_collider2_t` handle (`slot << 16 | generation`, 0 is never valid). Functions:
  `sat_physics2_world_init/reset/requirements/set_emit_stay`, `sat_collider2_desc_init`,
  `sat_physics2_add/remove/is_valid/get`, `set_center/set_box/delta`, `query` (overlap by kind and
  category, ordered by handle), `find_support`, `carry`, `launch_velocity`, `step`.
- Moving support: `find_support` probes along any of the four quadrants across a foot width and
  returns the nearest solid face inside the window `[-embed, reach)` with its point, normal,
  distance and the collider's delta since the last step; `carry` adds that delta to a position;
  `launch_velocity` returns delta times the collider's `launch_scale`. A removed collider makes
  `carry`, `launch_velocity` and `delta` answer `SAT_ERR_NOT_FOUND`, which is the support-invalidation
  signal. One-way colliders name their solid face (`one_way_face`) and only support a probe that
  lands on it, in any orientation.
- Sensors: for every sensor and every solid collider whose category matches the sensor mask,
  `step` emits ENTER / STAY (optional) / LEAVE ordered by (sensor handle, other handle), plus ENTER and
  LEAVE flagged CROSSING for a collider that passed through between two steps without overlapping at
  either one (relative-frame sweep, so a moving sensor works too). Removing either side while
  overlapping gives a LEAVE flagged REMOVED. Touching edges are not overlap, matching `collide2d.h`.
- Broadphase: the caller-owned `sat_spatial_t` indexes each collider's box swept since the last step
  (rebuilt lazily after any change), so queries, support and sensors use one index; no O(n) scan.
- Resource accounting: `sat_physics2_requirements(slot_cap, pair_cap)` sizes the slots, both pair
  arrays and the candidate scratch; the spatial arrays stay the caller's.
- `tests/host/test_physics2_world.cpp` (init/validation, handles and slot reuse, capacity, overlap
  queries, support windows in all four directions, one-way, horizontal and vertical platforms with
  carry, detach with launch velocity, one-way rising platform, stale handles, enter/stay/leave,
  removal while overlapping, high-speed crossing and grazing, deterministic ordering, event/pair/index
  capacity reporting, and a brute-force comparison through the broadphase).

Deviations and decisions made while implementing:

- Physics2 does not depend on Terrain2 or Character2, and Character2 does not consume Physics2:
  a character on a platform is composed by the game from `find_support`, `carry` and
  `launch_velocity` (the Phase 11 example does this). Making Character2 walk on a collider would
  have coupled the two modules for flat tops only.
- Colliders are boxes only; sensors need nothing else here and Terrain2 owns tile shapes.
- Crush detection is not provided: it could not be defined without choosing a game policy.
- A STAY event is opt-in because it costs a write per overlapping pair per step.

### Phase 4 — Path2
**Status:** Complete (2026-10-02). `path2` landed with host tests; the cross build for SH-2 and
the full `make test` pass.

Deliver line/polyline/arc/circle/quadratic/cubic primitives, sampling/tangent/advance/nearest and tests.

Delivered:

- `include/saturn/path2.h`, `src/physics/2d/path2.cpp`: one `sat_path2_t` value type tagged by
  `sat_path2_kind_t` (line, polyline, arc, circle, quadratic, cubic), addressed by arc-length distance
  in 16.16 pixels. Initialisers `sat_path2_init_line/polyline/arc/circle/quadratic/cubic`;
  `sat_path2_sample` (position, unit tangent, unit left-of-travel normal, the clamped or wrapped
  distance), `sat_path2_advance` (signed delta, `SAT_PATH2_CLAMP` or `SAT_PATH2_WRAP`, flags
  `HIT_START/HIT_END/WRAPPED`), `sat_path2_nearest` (closest point, its distance and the gap),
  `sat_path2_length`, `sat_path2_is_closed`. A polyline borrows its points and a cumulative-length
  array (`sat_path2_polyline_entries` sizes it); a Bezier optionally borrows an arc-length table that
  `sat_path2_build_table` fills at runtime or an offline tool writes and `sat_path2_attach_table`
  validates (`sat_path2_table_requirements` sizes it, 2..1025 entries).
- `tests/host/test_path2.cpp`: line and polyline boundaries, closed polylines and repeated vertices,
  quarter, eighth and full arcs in both directions, circle wrap, quadratic and cubic end points,
  length against the analytic value, tangent and normal unit length, a cusp, arc-length tables against
  uniform parameter on a lopsided curve, malformed offline tables, clamp and wrap including several
  laps and landing exactly on an end, nearest point on every kind (inside, outside, before the start,
  beyond the end, ties), copied paths and monotone distance.

Deviations and decisions made while implementing:

- Without a table a Bezier falls back to a uniform parameter (exact end points and total length, but
  not constant speed); a table is what makes distances true arc lengths. Both are documented in the
  header and the lopsided-curve test shows the difference.
- Arc and circle nearest-point accuracy is limited by `sat_atan2_16` (about 0.3 px at 200 px radius);
  lines and polylines are exact.
- Lengths are 16.16 and limited to 32767 px; longer shapes are rejected at init.
- Path2 depends on `core.h`, `collide2d.h` and `math2d.h` only. Advancing along a path with a speed,
  an easing or a direction reversal is the game's job; `advance` only moves a distance.

### Phase 5 — Follow Camera2D
**Status:** Complete (2026-10-02). `follow_camera2d` landed with host tests; the cross build for SH-2
and the full `make test` pass.

Deliver dead zone, smoothing, look-ahead, bounds, temporary clamps, shake, visibility/activation/prefetch bounds and tests.

Delivered:

- `include/saturn/follow_camera2d.h`, `src/physics/2d/follow_camera2d.cpp`: `sat_follow_camera2d_t`, a
  plain value holding the view centre (shake-free), the last step's `delta`, the current look-ahead, the
  world bounds, the sliding clamp and a shake. `sat_follow_camera2d_config_default/validate/init`;
  `sat_follow_camera2d_step(cam, target, velocity, out)` moves the centre and fills the
  `sat_camera2d_t` for `sat_render2d_set_camera` (target lands in the middle of the viewport, zoom and
  rotation untouched); `sat_follow_camera2d_get`, `sat_follow_camera2d_snap`.
- Dead zone (half size plus a shift, so the target can sit low on screen), per-axis follow fraction,
  per-axis speed cap, velocity-driven look-ahead with its own cap, gain and ease.
- `sat_follow_camera2d_set_bounds` keeps the visible window inside the world (a stage smaller than the
  screen is centred); `set_clamp` / `clear_clamp` add a temporary clamp (locked screen, boss room) whose
  edges slide at a given speed from the previous bounds and are intersected with the world.
- `sat_shake2d_t` (usable on its own): sine or deterministic noise, constant / linear / per-step decay
  envelopes, X/Y amplitudes (zero turns an axis off), a sign constraint, `update_every` N steps,
  frequency and seed. The offset is added only to the camera given to render2d: the centre, `delta`,
  and the activation and prefetch ranges are unaffected.
- `sat_follow_camera2d_range` for VIEW (what is drawn, shake and pixel snapping included), ACTIVATION
  and PREFETCH (shake-free view plus a margin plus `predict_steps` of the view's current motion on the
  side it is heading), `sat_follow_camera2d_overlaps` (touching edges do not overlap),
  `sat_follow_camera2d_world_to_screen` / `screen_to_world`. `SAT_FOLLOW_CAMERA2D_PIXEL_SNAP` rounds the
  window to whole pixels.
- `tests/host/test_follow_camera2d.cpp`: config validation, camera output, dead zone and shift,
  smoothing and speed cap, look-ahead (cap, reversal, ease, snap), world bounds (clamp, small stage,
  snap, removal), clamps (locked screen, slide in and out, no-world case), every shake envelope and
  shaping option, shake kept out of gameplay state, pixel snap, ranges with asymmetric prediction,
  overlap edges, and determinism.

Deviations and decisions made while implementing:

- The module lives next to the other 2D modules in `src/physics/2d/` and depends only on `core.h`,
  `collide2d.h`, `math2d.h` and the `sat_camera2d_t` type of `render2d.h`; it never calls render2d.
- Zoom and rotation stay at identity; a game that wants them edits the output camera.
- A clamp that cannot intersect the world on an axis wins over the world on that axis.
- `clear_clamp` with no world bounds drops the clamp at once (nothing to slide back to).
- Bounds changes take effect on the next step, not immediately.

### Phase 6 — Large stage map streaming
**Status:** Complete (2026-10-02). `stage_map2` landed with host tests; the cross build for SH-2 and
the full `make test` pass.

Deliver logical map/metatile descriptors, VDP2 window streaming, multiple layers, incremental dirty updates, tests and resource accounting.

Delivered:

- `include/saturn/stage_map2.h`, `src/graphics/vdp2/stage_map2.cpp` (hardware-free logic) and
  `src/graphics/vdp2/stage_map2_vdp2.cpp` (the only hardware-facing part: `sat_stage_map2_commit_vdp2`
  uses `sat_vdp2_vram_write_words`, `sat_stage_map2_apply_scroll_vdp2` uses `sat_vdp2_layer_set_scroll`).
- Content model: a shared metatile table (`sat_stage_map2_tileset_t`, power-of-two metatile edge of
  1..16 cells, up to 16384 metatiles) and up to four independent logical layers
  (`sat_stage_map2_layer_desc_t`), each with its own map of metatile indices, ring, scroll ratio against
  the camera (parallax), outside policy (empty / clamp / wrap), character bias and optional palette
  override. Cell words are VDP2 one-word pattern names; map entries carry X/Y flip that mirrors the whole
  metatile and toggles the cell flip bits.
- Streaming: each layer keeps the visible window plus a margin resident in its VDP2 ring
  (`sat_stage_map2_ring_t`: a grid of 1/2/4 pattern-name pages per axis, any page addresses;
  `sat_stage_map2_ring_from_layer` derives it from a `sat_vdp2_layer_config_t`).
  `sat_stage_map2_set_view` stages only the columns and rows that entered the window, nothing when the
  view stays in the same cell, and a bounded window rebuild for a jump without overlap.
- Staging is caller-owned (`sat_stage_map2_storage_t`: words plus runs). `set_view` and `mark_dirty` only
  stage; `sat_stage_map2_commit` sends the runs through a writer in staging order, stops at the first
  error and keeps the rest pending. A refused update (`SAT_ERR_CAPACITY`) changes nothing.
- Explicit dirty regions: `sat_stage_map2_mark_dirty` restages the resident part of an edited rectangle,
  `invalidate` forces a rebuild, `discard` drops pending writes and invalidates.
- Accounting: `sat_stage_map2_requirements` / `_requirements_bytes` (staging for two updates' worth of
  full rebuilds of every layer), `sat_stage_map2_pending`, `sat_stage_map2_stats` (updates, rebuilds,
  cells staged and committed, runs committed).
- `tests/host/test_stage_map2.cpp` drives a fake VRAM through the real writer path and checks, after
  every commit of a long random walk (including teleports and negative coordinates), that every resident
  cell holds what a full render would put there. It also covers: no movement writes nothing, one-cell X
  and Y moves, diagonal and multi-cell moves, rebuild thresholds, parallax and scroll wrapping, flips,
  bias, palette override, outside policies while streaming, a 2 x 2 page ring with out-of-order page
  addresses, 2 x 2 characters, dirty regions across the ring wrap, capacity refusal, commit failure and
  resume, invalidate/discard, validation, and `ring_from_layer`.

Deviations and decisions made while implementing:

- Animated-tile metadata was left out: animating a tile means swapping its character data, which the
  game does with the existing VDP2 character upload, not by rewriting cells.
- Two-word pattern names and bitmap layers are refused; the module is for cell layers with one-word names.
- The module does not depend on the camera, terrain or character modules: it takes the viewport's top-left
  pixel (for the follow camera, the VIEW range's centre minus its half size). The scroll it reports is the
  view wrapped to the ring, ready for `sat_vdp2_layer_set_scroll`.
- Column strips are one-word runs, one per row, because a pattern name table is row-major; the cost is
  the commit call count, shown in `runs_committed`.

### Phase 7 — Entity region activation
**Status:** Complete (2026-10-02). `entity_stream2` landed with host tests; the cross build for SH-2
and the full `make test` pass.

Deliver partition/index, activation/deactivation, hysteresis, predictive bounds and tests.

Delivered:

- `include/saturn/entity_stream2.h`, `src/physics/spatial/entity_stream2.cpp`.
- Offline-built CSR index (`sat_entity_index2_t`): immutable 8-byte descriptors
  (`sat_entity_desc2_t`: x, y from the index origin, game-defined kind and data) sorted by region in
  row-major order, plus a `uint16_t` region start table (`region_count + 1` entries). Power-of-two
  region edge (8 px to 32 Kpx). `sat_entity_index2_validate` checks table shape, monotonicity and that
  every descriptor lies in the region that lists it; `sat_entity_stream2_init` runs it once.
  `sat_entity_index2_bytes` sizes the tables for the resource plan.
- State: two caller-owned bitsets (active and retired), `sat_entity_stream2_state_words` words in
  total. Descriptors are never written.
- `sat_entity_stream2_update(stream, activation_box, result)`: (1) active descriptors outside the box
  grown by `hysteresis` are deactivated, ascending index; (2) inactive, not retired descriptors inside
  the box are activated by walking only the overlapped regions, ascending index. Activation order is
  therefore the descriptor order. A camera jump across any number of regions is handled in one update.
- Callbacks: `activate` returns `SAT_ENTITY_ACTIVATED`, `SAT_ENTITY_DEFER` (pool full: the update stops
  and retries that descriptor first next time) or `SAT_ENTITY_DECLINE` (retire it); `deactivate` is
  optional. Callbacks may call `release`/`retire` on the stream; the deactivation scan re-reads the
  bitset after each callback.
- Game-initiated state: `release` (inactive, no callback, re-activates when inside the box again),
  `retire` / `unretire` (a collected ring never respawns), `reset` (stage restart, no callbacks).
- Stats: updates, activations, deactivations, deferrals, declines, peak active.
- `tests/host/test_entity_stream2.cpp`: index validation and init refusals; activation set equal to a
  brute-force model for a long random walk with teleports, with and without hysteresis; no duplicate
  activation or deactivation (the game-side model asserts it); stable ascending order; hysteresis
  against an edge that hovers over a descriptor (no churn); reactivation; multi-region jumps; box
  edge semantics (half-open, empty box); pool full with deferral, order kept and resume; retire,
  unretire, decline, reset; callbacks that edit the stream; no deactivate callback; empty index and
  an index far from the origin; prediction margin fed from the follow camera's ACTIVATION range.

Deviations and decisions made while implementing:

- The predictive extension is not in this module: the activation box comes from the follow camera's
  ACTIVATION range (which already stretches by the motion), so a player-centred or any other source
  just builds its own box. The prediction test composes the two.
- Descriptor positions are non-negative 16-bit offsets from an int32 origin (65536 px per axis from
  the origin) and the table is 16-bit, so one index holds up to 65535 descriptors; larger stages use
  several indices. Boxes are 16.16 world pixels and so limited to +-32767 px.
- Descriptors are points; an entity with an extent grows the box or the region size.
- Deactivation scans the active bitset (O(descriptors / 32) per update, skipped when nothing is
  active) rather than keeping an active list, which keeps the state to one bit per descriptor.

### Phase 8 — Gameplay task scheduler
**Status:** Complete (2026-10-02). `saturn/task.h` landed with host tests; the cross build for SH-2
and the full `make test` pass.

The audit found no existing facility, so the precondition holds, but build this last
and only on a demonstrated need (see section 18). Deliver fixed capacity, deterministic order, generation handles, safe lifecycle and tests.

Delivered:

- `include/saturn/task.h`, `src/core/task/task.cpp` (listed in `cmake/LibSaturnSources.cmake`), host
  test `tests/host/test_task.cpp` (16 tests, one a 3000-step randomized run against a reference
  model), ownership row in `docs/PUBLIC_API_OWNERSHIP.md`.
- API: `sat_task_scheduler_init(scheduler, slots, order, capacity)` over two caller-owned arrays
  (`sat_task_slot_t[capacity]`, `uint16_t[capacity]`; `sat_task_scheduler_slot_bytes`),
  `sat_task_create(desc, &handle)` with `sat_task_desc_t` (update function, optional destroy function,
  caller-owned data pointer, `int16_t` priority), `sat_task_destroy`, `sat_task_set_update`,
  `sat_task_is_alive`, `sat_task_data`, `sat_task_scheduler_run`, `sat_task_scheduler_clear`,
  `sat_task_scheduler_count`, `sat_task_scheduler_stats`.
- Order: ascending priority, ties in creation order. `order` is a sorted index array kept by stable
  insertion, so a step is O(live tasks) and creation is O(live tasks) in the worst case; there is no
  linked list and no hidden allocation.
- Handles are `{slot, generation}`; generation bumps at destroy time (skipping 0 on wrap), so a handle
  is stale the moment its task dies, even before the slot is reused. A zeroed handle is always stale.
- Lifecycle during a run: self destroy, peer destroy (a peer later in the order is skipped, an earlier
  one is just gone), update replacement (a peer later in the order runs the new function in the same
  step); tasks created during a run are queued and first run in the next step. Destroyed slots are
  freed when the run ends, so they count against the capacity until then. The destroy function runs
  once, at destroy time, and may destroy peers but not run or clear (`SAT_ERR_BUSY`); `clear` destroys
  in run order and refuses creation from destroy functions.

Deviations and decisions made while implementing:

- Nothing in the library or in `high_speed_platformer` (now an external repository) consumes the scheduler, as section 18 requires
  of the terrain, camera and physics modules; the example steps its own state. It is here as the
  generic facility, and no demonstrated consumer exists yet.
- No `set_priority`: reordering a live task has no use case and would complicate the stable order.

### Phase 9 — Sprite animation runtime
**Status:** Complete (2026-10-02). `sprite_clip` landed with host tests; the cross build for SH-2
and the full `make test` pass.

Deliver clips/player, variable durations, playback policies, pivots/metadata/events, texture-region integration and tests.

Delivered:

- `include/saturn/sprite_clip.h`, `src/graphics/2d/sprites/clip.cpp` (pure logic, host-testable) and
  `src/graphics/2d/sprites/clip_draw.cpp` (texture glue). `sprite_anim.h` stays as it was: it selects a
  frame by number and does not grow.
- Immutable data: `sat_clip_frame_t` (source region, pivot, duration in ticks, event id, shape range,
  flags), `sat_clip_t` (frames, mode ONCE / LOOP / PING_PONG, `loop_start`), `sat_clip_set_t` (clips plus
  one shape array), `sat_clip_shape_t` (a generic rectangle or point with game-defined `kind`, `index`,
  `flags`; the library gives them no meaning). `sat_clip_set_validate` checks durations >= 1,
  non-empty sources, mode, loop start and shape ranges; `sat_clip_set_region_count` gives the content
  budget against `sat_texture_region_capacity()`.
- `sat_clip_player_t`: `init`, `play(clip, RESTART | KEEP_IF_SAME | KEEP_PHASE)`, `restart`, `seek`,
  `set_rate` (16.16, >= 0), `pause` / `resume`, `step` (one tick times the rate) and `advance` (a
  variable 16.16 number of ticks), state queries, and `frame()` for the current frame.
- Time: ticks, durations of whole ticks. The player keeps the fractional elapsed time in the frame, so a
  rate of 0.25 over 240 steps lands exactly on frame 0 of a 6-frame loop 10 times (no drift), and one
  step may cross several frames. Events fire when a frame is entered and are delivered in order through a
  callback, even across several frames of one step. `play` / `restart` / `seek` deliver the entered
  frame's event; KEEP_IF_SAME and KEEP_PHASE do not.
- Policies: ONCE holds the last frame and reports finished (`SAT_CLIP_STEP_FINISHED`; the finishing step
  advances no frame). LOOP returns to `loop_start`. PING_PONG plays 0 1 2 1 0 1 2 (end frames once per
  pass); a one-frame ping-pong holds and counts cycles. KEEP_IF_SAME leaves the playing clip alone
  (even a finished one), KEEP_PHASE carries the frame position and the fraction of the frame over,
  scaled to the new clip's length (walk to run).
- Geometry: the pivot is a continuous coordinate from the source's top-left corner; flipping mirrors
  around it (`sat_clip_frame_dest`, `sat_clip_shape_rect`, `sat_clip_shape_point`), with 16-bit clamping.
  `sat_clip_frame_shapes` and `sat_clip_frame_find_shape(kind, index | 0xFF)` read a frame's shapes.
- Texture integration: `sat_clip_set_prepare_regions` calls `sat_texture_prepare_region` for every
  frame; `sat_clip_player_draw` draws the current frame through `sat_draw_texture` (a `static_assert`
  pins `SAT_CLIP_FLIP_*` to `SAT_FLIP_*`). The glue is the only part linked against hardware code, so
  the host test links `clip.cpp` alone.
- `tests/host/test_sprite_clip.cpp`: validation failures, variable durations with events and loop
  flags, one-shot finish and restart, the three switch modes, ping-pong and `loop_start` sequences,
  fractional and double rates, rate 0 and negative rates, multi-frame steps with ordered events, pause,
  resume and seek, idle-player behaviour, pivot / flip geometry for frames, rectangles and points,
  clamping, shape lookup.

Deviations and decisions made while implementing:

- Durations and rates are in ticks, not seconds: a fixed-step game gets one tick per step, a PAL / NTSC
  game scales by its own step rate. This is the same time base the rest of the 2D modules use.
- The draw glue takes a destination from the pivot and does not scale or rotate; pass a
  `sat_draw_params_t` for tint or blend. Per-frame hitboxes are generic shapes, so Character2 /
  Physics2 integration is done by the game, not by this module.
- Clip data is produced offline by the animation packer (Phase 10); hand-written tables work too.

### Phase 10 — Offline tools
**Status:** Complete (2026-10-02). The generic tools landed with Python and host tests; the full
`make test` passes. Reference: `docs/STAGE2D_TOOLS.md`.

Deliver generic terrain/profile compiler, map/metatile compiler, entity partition builder, animation packer, optional path preprocessing and validation tests.

SA2-specific parsers, if any, live only in tools/import territory and emit generic assets.

Delivered:

- `tools/stage2d_tool.py` (`build` / `check`) over the package `tools/stage2d/`: one JSON spec in, a
  deterministic C header and source out, plus a bytes-per-section report and a `--max-bytes` budget
  gate (the resource-size validation of section 20).
- Terrain/profile compiler (`terrain.py`): profiles from column heights, an ASCII mask or a variant of
  another profile; the row table derived exactly as `sat_terrain_profile2_from_columns` does and both
  tables validated like `sat_terrain_profile2_validate`; rotated and flipped variants (`flip_x`,
  `flip_y`, `rot90`, `rot180`, `rot270`) with the angle carried along; `angle: "auto"` for floor and
  ceiling ramps (the normal and tangent are derived by the runtime from the angle, so the tool stores
  only the angle).
- Map / metatile compiler (`metatiles.py`, `grid.py`): one compiler for terrain tile words (shift 0..5,
  up to four layers sharing a table, flips and the four game bits written as `name|fx|fy|uN`) and for
  stage_map2 cell words (shift 0..4, up to 16384 metatiles); identical metatiles are shared in
  first-seen order, padding uses a fill word, metatile 0 is the all-fill one.
- Entity partition builder (`entities.py`): the CSR index of `sat_entity_index2_t`, region-major with
  the author's order kept inside a region, an origin and grid size that default to the items' extent or
  can be pinned to a map, and every 16-bit limit checked with a message that names the entity.
- Animation packer (`clips.py`): clips, frames, sheet-grid cells or explicit sources, named or numeric
  pivots, events, generic shapes with shared runs; the sets `sat_clip_set_validate` accepts.
- Path preprocessing (`paths.py`): Bezier arc-length tables at 64 chords per interval, for
  `sat_path2_attach_table`.
- Tests: `tests/tools/test_stage2d.py` (profile and angle rules, transforms, metatile dedup, entity
  CSR against a brute-force ordering, clip packing, table properties, error messages, determinism, CLI
  exit codes, a provenance scan, and byte equality of a fresh build with the committed fixture) and
  `tests/host/test_stage2d_generated.cpp`, which compiles the committed generated C and runs it through
  the real runtime validators, probes, a stream, a player and path sampling, and compares the byte
  accounting with the runtime `*_requirements` helpers. Fixtures
  (`tests/fixtures/stage2d/synthetic_stage.json`) are synthetic.

Deviations and decisions made while implementing:

- Output is C arrays, not a binary container: the runtime modules consume typed arrays by pointer and
  the plan has no loader, so a container format would have no reader. If streaming from CD needs one
  later it gets a magic, version and section table then.
- Importers (`tools/import`) are not written: no importer is needed for original content, and the
  provenance rule keeps any such tool and its output out of the repository.
- A full profile column is written as +8 and a full row as -8, as the runtime derivation does; the two
  describe the same mask.
- The size report charges SH-2 struct sizes (4-byte pointers) for the clip structs.

### Phase 11 — Integration example (incubated in-tree)
**Status:** Complete (2026-10-02); extracted in Phase 13. Reference: the README of
[celsowm/high-speed-platformer-saturn](https://github.com/celsowm/high-speed-platformer-saturn).
The paths below describe the in-tree incubation and no longer exist in this repository.

Deliver the original high-speed platformer stress example under
`examples/high_speed_platformer`, following the section 29.2 rules. It may start as a
skeleton earlier and gains a stress area per landed phase; this phase is complete when
it exercises everything listed in section 29, builds for Saturn, and has been run in
the Ymir probe with scripted input and captured screenshots.

Delivered (all under `examples/high_speed_platformer/`):

- `game.c/.h`: the game with no hardware (`hsp_game_init/step`, view origin, hero speed, platform
  boxes). Terrain2 + Character2 run the hero; Physics2 holds the sensors (three layer switches around
  the loop), the pickups' shapes and the two platforms; Path2 drives the platforms (a Bezier and a
  circle) and the rail the hero hangs from; Follow Camera2D owns the camera (dead zone, speed
  look-ahead, locked finish room, shake on hard landings and dash pads); `entity_stream2` activates
  rings, pads, springs and flags by region; `sprite_clip` plays the hero, ring, sparkle and pad clips.
- `view.c/.h`, `art.c/.h`: three VDP2 layers (NBG0 terrain, NBG1 hills at 1/2 speed, NBG2 clouds at 1/4)
  fed by `stage_map2` rings and committed right after VBlank; procedural 4bpp cells and an INDEX8 sprite
  sheet drawn by code; VDP1 sprites, platforms and a HUD (RINGS, SPEED, STAGE CLEAR).
- `main.c`: manual frame loop with 60 Hz fixed steps (up to three catch-up steps); publishes
  `g_hsp_telemetry` for the harness.
- `tools/gen_stage.py` writes the `stage2d_tool.py` spec and the layout; `stage.mk`, `host_test.mk` and
  `Makefile.inc` own the build rules. Generated C goes to `build/generated/high_speed_platformer/`;
  nothing generated is tracked.
- `harness/high_speed_platformer.pad` and `tools/probe_check.py`: the scripted Ymir run and its check.
- Tests: `tests/host/test_high_speed_platformer.cpp` plays the same `game.c` through the real stage and
  asserts platform riding, one-way plank landing, the loop with two layer switches and no death, rail
  and the stage end (rings, streaming spawn/despawn, dash pad, spring, top speed above 17), and
  `tests/host/test_high_speed_platformer_boundary.cpp` enforces section 29.2 (public `saturn/*`
  includes only, no `examples/common`, no reference to another example, files the extraction needs).
  The Makefile picks up an example's tests through `-include examples/*/host_test.mk`.

Verification:

- `make test` passes (host sim and boundary included); SH-2 cross-build via `build-example.ps1`.
- Ymir probe, scripted pad, 409 frames: stage cleared, 66 rings, 2 layer switches, 0 deaths, top speed
  17.7 px/step, 196 entities spawned and 116 despawned. Screenshots (`probe_check.py --shots`) show the
  terrain, parallax, sprites, HUD, the camera clamp and STAGE CLEAR.
- Not done: a Mednafen capture of the VDP2 layers (the Ymir probe is the reference used).

Deviations and decisions made while implementing:

- Platform riding is composed by the game (carry by the platform's per-step delta and Physics2 support
  lookup), as Character2 attaches to terrain only. Support is kept when the previous stand is still the
  best support, so a descending platform does not drop the hero.
- Character2 attach requires ground; airborne placement is `sat_character2_init` plus an air velocity.
- Fix found by the SH-2 build: `sat_physics2_launch_velocity` dereferenced an unchecked slot, which made
  GCC reference `abort`; it now returns `SAT_ERR_NOT_FOUND` (already covered by
  `test_physics2_world.cpp`).
- The example does not consume the Phase 8 task scheduler; the game steps its own state directly.
- Pad frame F corresponds to game tick F-8 in the probe (boot and loading), hence the jump at frame 118
  for tick 110 of the host sim.
- The bot's run skips the finish-room rings, so the probe check requires 50 rings rather than all.

### Phase 12 — Performance, SOLID/DRY and documentation audit
**Status:** Complete (2026-10-02). One duplication removed; everything else audited and left as is, with
the reasons below. `make test` and the SH-2 cross build of the example pass, and the Ymir probe run of
the example gives the same counters as before the change.

Audit Big-O, divisions/64-bit operations, memory, duplicate camera/support math, animation timers, map streaming duplication, compatibility leftovers, header dependencies and lower-level standalone linkability.

Run full host and Saturn build gates.

Findings:

- **Duplication, fixed:** the saturating `int64 -> int32` narrowing was copied as `clamp32` into
  `follow_camera2d.cpp`, `path2.cpp` and `physics2_world.cpp`. It is now `saturate32` in
  `src/core/math2d/logic.hpp`, used by all three; no behaviour change.
- **Camera / entity box math:** not duplicated. `follow_camera2d` produces the activation box once and
  `entity_stream2` takes a box and grows it by its own hysteresis; neither rebuilds the other's range.
  `entity_stream2`'s region edge helpers are the only floor/ceil to pixels in that file;
  `follow_camera2d`'s `floor_px`/`round_px` snap 16.16 values to whole pixels, which is a different
  operation (kept private to the camera).
- **Support math:** Character2 (terrain support), Physics2 (moving support, `sat_physics2_find_support`)
  and the example's carry step each own one stage; the example composes them and contains no slope or
  support projection of its own.
- **Map streaming:** `stage_map2` is the only ring/window streamer in `src`; the older layer code
  (`vdp2_layers`) configures registers and shares no logic with it.
- **Animation timers:** `sprite_clip` is the only generic clip timer; `sprite_anim` (older, uniform
  frames) is kept for simple examples, as section 32 asks. The example has no hand-written animation
  timer.
- **Big-O:** per step, Terrain2 queries are O(probe length); Character2 is O(1) over them; Physics2 is
  O(slots) for the carry/slot sweep and O(candidates) per collider through the spatial structure;
  `entity_stream2` walks the regions the box covers and, to release, the active bitset (one word per
  32 descriptors, so a 3000-descriptor stage is under 100 word reads per update), never the descriptors;
  `stage_map2` writes only the cells that scrolled into the ring; the new task scheduler is O(live) per
  step and per create. Nothing scans the level or all entities per frame.
- **Divisions and 64-bit:** the 2D modules use 64-bit intermediates for products (`(a*b)>>16`) and
  saturate on the way down. Real 64-bit divisions occur only in Path2: arc-length to parameter
  (`(s<<16)/length`, a few per sampled path per step) and the Bezier nearest-point search (33 coarse
  samples plus 14 refinements, run on attach or on demand, not per step). `entity_stream2` divides only
  in its one-time index validation (`r / region_cols`), never per update. Followers of fixed paths could precompute a reciprocal; the
  example samples three paths per step and the probe holds 60 Hz, so it was not done without a
  measurement that needs it.
- **Memory:** every module takes caller-owned storage; grep for `malloc`, `new`, `std::vector`,
  `virtual`, `TODO`/`FIXME` over the new sources and headers finds nothing. Sizes are reported by the
  `*_requirements`/`*_bytes` helpers and the stage2d tool's section report.
- **Compatibility leftovers:** none added; the earlier `physics.h` / `collide2d.h` APIs are untouched.
- **Header dependencies:** each new public header compiles alone as C11 (`-pedantic`) and as C++20.
  Includes are `core.h`, `collide2d.h`, `math2d.h` and, where a type needs it, `terrain2.h`
  (character2), `spatial.h` (physics2_world), `render2d.h` (follow_camera2d, for `sat_camera2d_t`),
  `vdp2.h`/`vdp2_layers.h` (stage_map2), `texture.h`/`geometry2d.h` (sprite_clip). `task.h` includes
  only `core.h`.
- **Standalone linkability:** every module's host test links the module's own source plus at most
  `math2d/api.cpp` (and `spatial/2d.cpp` + `collision.cpp` for Physics2); `task.cpp` and `clip.cpp`
  link alone. Lower-level users that never call these modules pull none of them in (static library,
  one object per module).
- **Hardware claims:** the example's VDP2 layers were verified in the Ymir probe only (screenshots);
  no Mednafen or hardware run was done. Frame-rate statements rest on the probe's frame counters.

### Phase 13 — Extract the example to its own repository
**Status:** Complete (2026-10-02). Repository:
[celsowm/high-speed-platformer-saturn](https://github.com/celsowm/high-speed-platformer-saturn).

Precondition: Phase 12 passed and the public APIs the example uses are considered
stable. Extract after, not during, API churn.

Deliver, following the Ikemen extraction as the precedent:

- a new independent repository for the example, with history preserved
  (`git filter-repo`) where useful;
- a CMake project that finds LibSaturn with `find_package(LibSaturn CONFIG REQUIRED)`,
  links `LibSaturn::Saturn`, uses the shipped `sh2eb-elf.cmake` toolchain and
  `libsaturn_configure_executable`, `libsaturn_add_binary` and `libsaturn_add_disc`;
  a Conan recipe that consumes the library's Conan package;
- the example's host tests, asset converters and Python requirements (declared in a
  `requirements.txt`, checked at configure time) moved with it;
- CI that builds the toolchain and package, then the example, on a clean runner;
- the boundary check from section 29.2 passing against the installed prefix.

Validation, all required before the library side is cleaned:

- the old in-tree build and the new external build, run in the Ymir probe under the
  same scripted input, give byte-identical screenshots;
- the example's host tests give the same results before and after;
- a clean checkout of the new repository builds from scratch.

Then remove the example and every example-specific build rule, test, converter and
document from LibSaturn, leave only links in README/CHANGELOG, re-run the full
LibSaturn host, package and Saturn gates, and push to `main`.

Known traps from the Ikemen extraction (see section 37.6): Conan `exports_sources`
must list every file the installed package needs, a generated-argument list needs
`$<SEMICOLON>` through the CMake wrapper, tools called by file path lose their exec
bit on a clean Linux checkout, and a CI runner has no Pillow unless the workflow
installs it.

Delivered:

- The new repository keeps the example's history (`git filter-repo`) and is a plain CMake project:
  `find_package(LibSaturn 0.1 CONFIG REQUIRED)`, `LibSaturn::Saturn`, the shipped toolchain,
  `libsaturn_configure_executable`, `libsaturn_add_binary` (limit 983040 bytes) and
  `libsaturn_add_disc`; `saturn` and `host` presets driven by `LIBSATURN_PREFIX`; `requirements.txt`
  (standard library only, checked at configure time); a Conan 2 recipe that `requires("libsaturn")`.
- Two package additions made the host side possible without a LibSaturn checkout:
  `LibSaturn::Sim2D` (the hardware-free 2D modules shipped as sources under `share/libsaturn/sim`,
  compiled by the consumer's own compiler; the file list is `cmake/LibSaturnSim2D.cmake` and
  `tests/tools/test_package_sim2d.py` keeps it closed under the private includes and free of hardware
  headers) and `LIBSATURN_STAGE2D_TOOL` (the installed `stage2d_tool.py` and its `stage2d/` package).
- The boundary test moved with the example and now checks it against the installed prefix's include
  directory instead of a checkout. `gen_stage.py` takes `--tools-dir` (or `$LIBSATURN_TOOLS_DIR`), the
  installed disc-tools directory, instead of deriving a repository path.
- CI (`.github/workflows/ci.yml` in the new repository) builds the toolchain and the package from
  LibSaturn `main` with a local composite action (the Ikemen one), then runs the host tests, the
  firmware and the disc on a clean runner and uploads the disc; the first run on GitHub
  passed (11 minutes, toolchain build included).

Validation:

- Old in-tree build vs new external build: the same scripted pad run in the Ymir probe gave
  byte-identical screenshots at frames 100, 250 and 400, and `probe_check.py` gave the same result
  from the new repository (stage cleared, 66 rings, 2 layer switches, 0 deaths, top speed 17.7). The
  `app.bin` differs by 36 bytes (embedded paths and flags only).
- Host tests: the simulation test gave the same results before and after.
- A fresh `git clone` of the new repository built from scratch against an installed prefix: host
  tests (simulation and boundary), firmware and bootable disc (app.bin 234384 bytes, 314 sectors).
- Conan: `conan create` of LibSaturn, then of the new repository, with `saturn-sh2eb`; the recipe
  packages the app, ELF and disc.
- LibSaturn after the cleanup: full host suite, package gates (`scripts/test-package`, including
  `-Conan`) and the SH-2 cross builds of the remaining examples.

Deviations and decisions made while implementing:

- `LibSaturn::Sim2D` and the stage tool installation are new package surface that the plan did not
  list; without them the example's simulation test could not run outside a checkout.
- The `-include examples/*/host_test.mk` hook stays in the Makefile as a generic extension point; it
  matches nothing at present.
- No example-specific rule, test, converter or document remains in LibSaturn: only the README and
  CHANGELOG link to the new repository (this plan keeps its history).

---

## 31. Saturn performance constraints

Account for:

- SH-2 performance/cache behavior;
- expensive division;
- 64-bit math cost;
- Work RAM;
- VDP1 VRAM/command capacity;
- VDP2 VRAM/cycle patterns;
- CRAM;
- B-bus contention;
- fixed frame budget.

Prefer compact arrays, precomputation, fixed-point, stable handles, dirty regions, incremental streaming, broadphase and offline conversion.

Avoid linked-list-heavy hot paths, virtual dispatch, per-frame allocation, full-map/entity scans, recursive dynamic structures and hidden copies.

Slave SH-2 acceleration requires an equivalent Master path and measured benefit before becoming canonical.

---

## 32. Repository-wide cleanup targets

As phases land, search for:

- duplicated camera-range checks;
- repeated world-to-screen culling math;
- repeated slope/surface projection;
- manual platform carry;
- ad-hoc sensor overlap loops;
- hand-coded animation timers;
- repeated map-window calculations;
- accidental O(n) collectible/entity scans;
- examples importing another example's generic helper;
- new magic angle/slope constants.

Do not force advanced APIs into simple examples. Pac-Man, for example, should remain able to use the simple stack.

---

## 33. Documentation/ownership requirements

For each landed module document:

- backing-storage owner;
- borrowed vs copied descriptors;
- lifetime;
- handle generation/staleness;
- coordinate convention/Y direction;
- fixed-point formats;
- normal/tangent convention;
- layer/mask semantics;
- one-way convention;
- support identity/velocity;
- update order;
- task destruction semantics;
- stage-map VRAM ownership;
- animation tick units;
- failure/capacity behavior.

Update PUBLIC_API_OWNERSHIP.md and resource/performance ledgers when appropriate.

Do not claim hardware verification without evidence.

---

## 34. Completion questions

Before marking this plan complete, answer with repository evidence:

1. Can terrain expose arbitrary oriented support?
2. Can a generic object query floor/wall/ceiling without a player controller?
3. Can a character move continuously floor -> wall -> ceiling?
4. Is ground-relative speed distinct from air velocity?
5. Can support be moving?
6. Can one-way behavior be normal-relative?
7. Can collision layers switch at runtime?
8. Can rails/loops/scripted trajectories use generic paths?
9. Can large logical maps stream incrementally through VDP2?
10. Can thousands of static descriptors remain inactive outside the active region?
11. Can high-speed motion avoid obvious terrain/sensor tunneling?
12. Can camera follow/look-ahead/clamps/shake avoid repeated game boilerplate?
13. Can sprite animation express clips, timing, pivots and generic metadata?
14. Can normal gameplay remain free of hidden heap allocation?
15. Are major algorithms host-testable?
16. Can lower-level users ignore the advanced runtime?
17. Could an SA2-class port use these systems without implementing a second collision/map/entity engine beside LibSaturn?
18. Does the example build and pass its boundary check using only the installed package, so it can live in its own repository?

If a “no” represents a generic engine gap, address it before completion.

---

## 35. Definition of done

This plan is complete only when:

- accepted phases have real code;
- public headers are coherent and narrow;
- host tests pass;
- relevant Saturn examples cross-build;
- the original integration example builds, and has been extracted to its own
  repository that consumes the installed package;
- docs match implementation;
- obsolete duplicate abstractions are removed;
- no core feature exists only as a TODO;
- performance/capacities are documented;
- unverified hardware claims are explicitly labeled.

A documentation-only phase is not implementation completion.

---

## 36. Final target

The goal is not “Sonic support.”

The goal is:

**oriented terrain + generic character support + kinematic world + paths + streaming + deterministic runtime services, all optional and composable across LibSaturn's abstraction spectrum.**

---

## 37. Phase 0 audit record (2026-10-02)

This section records evidence only. It does not freeze any API.

### 37.1 References

All 21 SA2 files named in section 2 exist under `.external/sa2`. `.external/` is
gitignored, consistent with treating it as local read-only reference material.

### 37.2 Current LibSaturn inventory against the plan

| Area | State in the repository | Plan section |
|------|-------------------------|--------------|
| Body/tile solver | `sat_body2_step`, `_move_tiles`, `_move_tiles_surface`, `_move_boxes`, `_separate`; `SAT_TILE_SLOPE` with `sat_tile_surface_t` (commit `3b28766` preserved the 45-degree math) | 1, 6 |
| Spatial broadphase | `sat_spatial_t` uniform grid: insert, box query, pair enumeration; no ordered result, no early-exit callback, no swept query | 21 |
| Camera | `sat_camera2d_t` and `sat_render2d_set_camera/get_camera`; no 2D follow policy (3D has `follow_camera3d.h`) | 14 |
| Screen shake | none | 15 |
| Gameplay scheduler | none (`parallel.h` is dual-SH-2 work distribution) | 18 |
| Sprite animation | `sprite_anim.h`: direction x frame selection and region frame selection only; no clips, durations or events | 19 |
| Palette | register/unregister/bank lookup only; no range update or cycling | 24 |
| VDP2 | line scroll and vertical cell scroll exist in `vdp2_layers.h`; also color calc, color offset, RBG0, compose | 23 |
| Resource plan | eight memory-region kinds, byte accounting, finalize | 25 |
| 2D math | add/sub/scale/reflect/approach, no dot/length/normalize/atan2 | 13 |
| Handles | `physics3_world` is the in-repo precedent for ownership and handles | 10 |

### 37.3 SA2 facts measured from the local tree

- **Collision data (`core.h`, `Collision`):** `s8 height_map`, `u8 tile_rotation`,
  `u16 metatiles`, `map[MAP_LAYER_COUNT]` (two layers), `u16 flags`, level and pixel
  dimensions. Tiles are 8 px; a metatile is 12 x 12 tiles (96 px, `camera.h`).
- **Height profile (`terrain_collision.c`):** eight `s8` entries per tile, indexed by
  tile index times 8 and a pixel row or column. Each byte packs **two signed 4-bit
  values**: the low nibble is read by the row-indexed sensors (horizontal extent, for
  walls) and the high nibble by the column-indexed sensors (vertical height, for floor
  and ceiling). Values are -8..8: 0 is empty, 8 is full, 1..7 partial, the sign says
  which side is solid. So one 8-byte profile plus one rotation byte per tile serves
  both axes at 4 bits per sample.
- **Tile word (`tilemap.h`):** 10-bit tile index (`0x3FF`), X flip (`0x400`), Y flip
  (`0x800`), 4-bit palette. A flip mirrors the pixel index; a Y flip also converts a
  partial height to the opposite side (h > 0 becomes h - 8, h < 0 becomes h + 8).
- **Rotation:** one `u8` per tile (256 steps per turn; the trig tables are 1024-period,
  indexed with `rotation * 4`). The sensor returns it by reference with the distance;
  an X flip negates it and a Y flip maps it to `-0x80 - rotation` (a half turn minus
  the angle) when the height is non-zero. An odd value is treated by the player
  wrappers as "no angle" and replaced by 0.
- **Optional per-tile flags:** a 2-bit-per-tile `flags` table can zero a height when a
  bit (`0x80`) is set in the layer argument. The player sets it while moving up
  (`qSpeedAirY < 0`) or slowly down (`< 3.0`), which behaves like a one-way surface;
  this is inferred from usage and not yet traced end to end.
- **Metatile lookup (`sub_801EF94`):** pixel to tile (`>> 3`), tile to metatile by a
  divide by 12 (cached for the last X and Y), then `map[layer][...]` for the
  metatile and a 12 x 12 table of `u16` tile words.
- **Sensor driver (`sub_801E4E4`, `sub_801F07C`):** probes up to three tiles along the
  direction (step of 8 px) and returns a **signed whole-pixel distance** to the
  surface (zero or negative when already inside), plus the rotation. There is no
  sub-pixel result.
- **Direction selection (`player.c`, `sub_8022F58`):** the character angle plus 0x20,
  masked to 0xC0, picks one of four axis-aligned sensor pairs. So SA2 senses in four
  quantized directions relative to the character, not in arbitrary directions.
- **Foot sensors (`sub_8029BB8`, `sub_802195C`, `sub_8021A34`, `sub_8021B08`):** two
  sensors offset by `2 + spriteOffset` pixels either side of the position; the lower
  distance wins and supplies the rotation, the other distance is returned too.
  Gravity inversion mirrors the angle (`-0x80 - rotation`) rather than using other code.
- **Attach and detach:** jump and takeoff checks compare the sensor distance with 4 px
  (`< 4` blocks, `> 3` allows); one helper zeroes ground speed when the angle is in the
  upper half turn. The full position-update path that decides detaching was not traced.
- **Moving support (`collision_1.c`, `Coll_Player_Platform`):** re-tested every frame
  against the platform's hit box with a raw `stoodObj` pointer; no carry delta, no
  support velocity on detach (boss stages subtract the camera delta from ground speed).
- **Slope gameplay (`player.c`):** slope acceleration is gameplay code gated by
  angle windows of the form `(rotation + k) & 0xFF < n`; it stays out of LibSaturn.
- **Layer:** the query takes a layer argument whose low bit selects one of the two
  collision maps; the player carries a `layer` byte (`PLAYER_LAYER__FRONT/BACK`).
- **Player state (`player.h`):** separate `qSpeedAirX`, `qSpeedAirY` and
  `qSpeedGround`, a persistent `u8 rotation`, a `moveState` bitfield and a raw
  `stoodObj` pointer for moving support.
- **Camera (`camera.h`):** position, `shiftX/Y`, min/max clamps, per-frame `dx/dy`,
  `shakeOffsetX/Y`, a background-update callback, and five overlapping
  `IS_OUT_OF_RANGE*` macros marked "Merge all these". Regions are 256 px.
- **Tasks (`task.h`, `task.c`):** priority-sorted doubly-linked list, at most 128,
  destructor, task data, destroy-disable flag.

### 37.4 Decisions that blocked the Phase 1 gate (all resolved in 37.8)

1. Angle representation (section 13).
2. Terrain profile format and per-tile metadata; the SA2 layout above is the
   baseline to measure alternatives against, not a format to copy.
3. Whether Physics2 and Terrain2 share a broadphase (section 10 vs section 21).
4. Scalar fixed-point header split (section 4).
5. Final module names and dependency direction (section 4).
6. Probe directions: four axis-aligned directions as the fast path, or arbitrary
   direction as well (section 5).
7. Distance precision: SA2 returns whole pixels; decide whether Terrain2 returns
   whole pixels, or a fixed-point distance, given the cost on SH-2.
8. Whether Terrain2 stores the two nibbles per sample (SA2-style, 4 bits per axis) or a
   wider sample, and whether metatile dimensions are powers of two (section 26).

### 37.5 Phase 0 items deferred to the phases that need them

- Trace of SA2's position-update path (the `PLAYERFN_UPDATE_POSITION` and rotation
  update) to see exactly how detach, snap and re-attach are decided; the sensors and
  thresholds above are known, the full sequence is not.
- Read-through of SA2's camera follow code (`camera.c`, 1216 lines), the entity
  manager (`entities_manager.c`), the grind rail, loop and corkscrew objects, and the
  sprite animation update, for sections 12, 14, 17 and 19.
- Cycle baseline of the current slope solver on SH-2 or in the harness (only the host
  test baseline in section 37.7 exists).
- Final names and layout, which the Phase 0 gate requires.

### 37.6 Extraction precedent

The Ikemen example was extracted to `celsowm/ikemen-saturn` on 2026-10-02 as a pure
package consumer. Evidence that the same path works: it builds firmware and a disc
through the installed CMake package and through Conan, its host tests and a
byte-identical probe screenshot comparison passed against the in-tree version. The
Linux and CI failures found while doing it are the traps listed under Phase 13.
Whether its firmware and oracle CI jobs pass on a GitHub runner was not yet confirmed
when this section was written.

### 37.7 Baseline (2026-10-02, commit 6d28732)

Host tests built fresh with `g++ -std=c++20 -Wall -Wextra -O1` in the MSYS2 ucrt64
shell and run: `test_physics_logic`, `test_spatial_logic` and `test_collide2d_logic`
all print PASS (exit 0). These cover the body/tile solver including surface slopes,
the uniform-grid broadphase and the 2D collision math, and are the regression floor
for Phases 1 to 3. No SH-2 cycle or probe-frame measurement was taken.

### 37.8 Decisions (Phase 0 gate)

Made on 2026-10-02 from the audit above.

1. **Angles:** `sat_angle_t` is `uint8_t`, 256 steps per turn, 0 along +X, 64 along
   +Y (down on screen). A 256-entry fixed-point sine table backs `sat_sin8` and
   `sat_cos8`; path code gets a 16-bit angle with linear interpolation. Surface angle
   is the tangent direction with the solid on the right-hand side of travel, so a flat
   floor is 0, a flat ceiling 128, a wall with solid to its right 192. The outward
   normal is `(sin a, -cos a)`. An X flip maps `a` to `-a`, a Y flip to `128 - a`.
2. **Distance precision:** terrain probes return whole-pixel distances stored in
   `sat_fx16_t` (integer pixels shifted by 16). Sub-pixel position stays in the
   caller's body; the sensor truncates it. This is what the reference needs and it keeps
   the per-tile work to integer table lookups.
3. **Probe directions:** four axis-aligned probes are the fast path (O(1) per tile,
   at most `range / 8 + 1` tiles). A bounded arbitrary-direction `cast` is provided as
   the generic slower form (one pixel step along the dominant axis).
4. **Profile format:** 8 x 8 px tiles. Per profile: eight signed column extents and
   eight signed row extents in -8..8 (positive anchors the solid to the bottom or
   right, negative to the top or left, 0 empty, +-8 full), a surface angle, flags, a
   category byte and a 16-bit material. Columns are authoritative for the pixel mask
   and vertical probes, rows for horizontal probes; the offline tool validates that
   both describe the same shape. This is 22 bytes per profile; nibble-packing was
   rejected as a size saving not worth the decode cost on SH-2.
5. **Map layout:** tile words are 16 bits (10-bit profile index, X flip, Y flip, 4
   bits for the game). Metatiles are `1 << metatile_shift` tiles per axis (power of
   two, no division). Up to four logical layers of metatile indices share one metatile
   table. Positions outside the map follow an explicit policy (empty, solid or clamp).
6. **One-way:** a profile flagged one-way blocks a probe only when the probe direction
   opposes the surface normal (`dot(direction, normal) < 0`), for any orientation.
7. **Broadphase:** Physics2 world and sensors use a caller-owned `sat_spatial_t`
   (dynamic insert/clear). Static entity regions use an offline CSR index instead.
8. **Math headers:** the new 2D modules depend only on `core.h` and `collide2d.h`
   (`sat_fx16_t` is in `core.h`), not on `math3d.h`, so no header split is needed.
   `saturn/math2d.h` holds the 2D helpers.
9. **Resource accounting:** each module exposes a `*_requirements()` byte helper and
   charges memory-region bytes (section 25); no new resource kinds.
