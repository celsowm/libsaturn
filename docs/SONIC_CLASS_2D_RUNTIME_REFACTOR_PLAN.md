# LibSaturn — Sonic-Class 2D Runtime Refactor Plan

**Status (2026-09-29):** Planned. The recent arbitrary 2D tile-slope work has landed, but the broader surface-oriented runtime described here has not yet been implemented.

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

- saturn/terrain2.h — oriented terrain/profile queries;
- saturn/character2.h — optional surface-oriented character controller;
- saturn/physics2_world.h — static/kinematic colliders and support handles;
- saturn/path2.h — deterministic 2D paths;
- trigger/sensor functionality either in a narrow trigger2 module or physics2_world;
- saturn/follow_camera2d.h — camera policy above render2d;
- saturn/stage_map2.h — logical large-map/metatile streaming to VDP2;
- saturn/entity_stream2.h — static descriptor activation/deactivation;
- saturn/task.h — bounded gameplay scheduler only if no suitable existing facility exists;
- expanded saturn/sprite_anim.h or a separate narrow animation-player header.

Lower-level modules must not require higher-level runtime initialization.

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

---

## 18. Gameplay task scheduler

Audit existing runtime scheduling first.

If no suitable generic scheduler exists, add one distinct from sat_parallel.

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

---

## 21. Spatial acceleration

Reuse sat_spatial_t.

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

After the foundations stabilize, add an original example such as:

examples/high_speed_platformer

Do not reproduce Sonic assets, character design, stage layout or presentation.

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

### Phase 0 — Repository and SA2 audit
**Status:** Not Started

- inspect current LibSaturn 2D/VDP2/spatial/resource architecture;
- inspect the referenced .external/sa2 systems and additional relevant code;
- record concrete requirements;
- freeze module dependency direction;
- establish current physics/runtime baselines where practical.

**Gate:** update this plan with final names/layout before public advanced API implementation.

### Phase 1 — Terrain2 foundation
**Status:** Not Started

Deliver profiles, orientation, masks, arbitrary-direction probes, normals/tangents, metadata, one-way semantics and tests.

**Gate:** generic enemies/objects can query terrain without character2.

### Phase 2 — Character2
**Status:** Not Started

Deliver support state, tangent speed, air velocity, landing/detach, floor/wall/ceiling continuity, configurable adhesion/snap policy and tests.

**Gate:** host simulation traverses a loop-like oriented test surface without game-specific collision code.

### Phase 3 — Physics2 kinematics and sensors
**Status:** Not Started

Deliver stable collider ownership, moving support/carry, sensors, broadphase integration and tests.

### Phase 4 — Path2
**Status:** Not Started

Deliver line/polyline/arc/circle/quadratic/cubic primitives, sampling/tangent/advance/nearest and tests.

### Phase 5 — Follow Camera2D
**Status:** Not Started

Deliver dead zone, smoothing, look-ahead, bounds, temporary clamps, shake, visibility/activation/prefetch bounds and tests.

### Phase 6 — Large stage map streaming
**Status:** Not Started

Deliver logical map/metatile descriptors, VDP2 window streaming, multiple layers, incremental dirty updates, tests and resource accounting.

### Phase 7 — Entity region activation
**Status:** Not Started

Deliver partition/index, activation/deactivation, hysteresis, predictive bounds and tests.

### Phase 8 — Gameplay task scheduler
**Status:** Not Started

Only if repository audit confirms no suitable existing facility. Deliver fixed capacity, deterministic order, generation handles, safe lifecycle and tests.

### Phase 9 — Sprite animation runtime
**Status:** Not Started

Deliver clips/player, variable durations, playback policies, pivots/metadata/events, texture-region integration and tests.

### Phase 10 — Offline tools
**Status:** Not Started

Deliver generic terrain/profile compiler, map/metatile compiler, entity partition builder, animation packer, optional path preprocessing and validation tests.

SA2-specific parsers, if any, live only in tools/import territory and emit generic assets.

### Phase 11 — Integration example
**Status:** Not Started

Deliver original high-speed platformer stress example.

### Phase 12 — Performance, SOLID/DRY and documentation audit
**Status:** Not Started

Audit Big-O, divisions/64-bit operations, memory, duplicate camera/support math, animation timers, map streaming duplication, compatibility leftovers, header dependencies and lower-level standalone linkability.

Run full host and Saturn build gates.

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

If a “no” represents a generic engine gap, address it before completion.

---

## 35. Definition of done

This plan is complete only when:

- accepted phases have real code;
- public headers are coherent and narrow;
- host tests pass;
- relevant Saturn examples cross-build;
- the original integration example builds;
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
