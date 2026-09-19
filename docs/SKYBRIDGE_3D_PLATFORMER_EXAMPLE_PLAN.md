# 3D Platformer Showcase — "Skybridge" implementation plan

Status: PLANNED (no playable example implemented by this document).
Target: `examples/skybridge_3d/`, a small, polished, playable Saturn-native 3D platformer/demo with a deliberately simple cube avatar.

## 1. Product and acceptance target

Build a **game, not a rotating-cube tech demo**: an attractive, readable 3D course above an animated sea, traversed by a player-controlled cube. The 3D geometry is genuine VDP1 geometry; the sea and panoramic backdrop are VDP2. Design for a stock Saturn with a digital pad and no RAM expansion, without demanding a new generalized game engine.

A complete first release has an uninterrupted 2–3 minute course (three short connected areas), a start position, eight pickups, two checkpoints, moving platforms, a collapsing bridge, a reachable finish, a fall/retry flow, a visible timer and pickup counter, a pause state, and basic jump/land/collect/checkpoint/win sounds. The player must be able to finish it without developer-only controls.

Visual direction: a sunny low-poly coastal ruin / sky garden. Strong readable silhouettes, colored ledges, checker/stripe accents, distant islands, layered clouds, ocean motion, warm/cool face lighting, near-platform contact shadows, and a restrained non-blocking HUD. A cube is fine for the character, but give it a contrasting front face / orientation marker, squash/stretch on takeoff/landing, a brief flashing damage/fall state, and a projected shadow so movement reads clearly.

**Hard requirement:** a slower but complete and visually coherent game is preferable to an unplayable "60 FPS" claim. Measure display frames per game update and build an explicit quality ladder.

## 2. Baseline capabilities and known traps (inspect again at implementation time)

These paths existed when the plan was written:
- `examples/physics_3d/main.c` and `include/saturn/collide3d.h`: fixed-point 3D bodies, AABB contacts and static collision helpers.
- `examples/runtime_3d/main.c`, `include/saturn/scene3d.h`, `include/saturn/mesh3d.h`, `include/saturn/render3d.h`: camera/model facade, quad mesh builders, projection, culling, per-mesh painter sorting and Gouraud for RGB polygons.
- `examples/vdp2_nbg0_rbg0_combo/main.c` and `examples/infinite_explorer/`: NBG0 panorama + RBG0 perspective floor + VDP1 foreground. Reuse **public APIs** and verified math/patterns; do not paste the existing application's mission logic.
- `examples/distance_fade_3d/`, `include/saturn/fade3d.h`, `include/saturn/vdp2_color_calc.h`: quantized distance fade for the supported indexed-sprite color-calculation path.
- `examples/transparency_showcase/`: demonstrably distinct RGB VDP1 half-transparency, indexed-sprite VDP2 color calculation, and checkerboard mesh, **not** a generic alpha-blended 3D renderer.
- `include/saturn/audio.h`, `include/saturn/time.h`, `include/saturn/video.h`, `include/saturn/input.h`, `include/saturn/asset.h`: bounded audio, frame timing, pad and asset facilities.
- `docs/LIBSATURN_CURRENT_COVERAGE.md`, `docs/HIGH_LEVEL_RUNTIME_API_PLAN.md`: audit any planned feature against its actual implementation, not the title of a roadmap.

Hardware/API rules:
1. VDP1 has **no depth buffer** and the public renderer can reject whole quads that cross the near plane. Model-local painter sorting is not a global world-vs-player order. No arbitrary overlap may be assumed correct without explicit depth ordering and visual tests.
2. `sat_app_frame_begin()` calls `sat_set_clear_color()`, making the VDP1 erase opaque and potentially hiding the VDP2 scene. For this example use an explicit VBlank / transparent VDP1 erase / layer-and-color-calc commit / frame begin / draw / frame end sequence, adapted from the combined-layer example. Check layer priority and index-zero transparency.
3. RBG0 is a **flat image plane**. It can represent distant ocean or decorative ground, not elevated collision platforms or a true 3D water surface with per-wave geometry. Elevated gameplay must be VDP1.
4. The fade/alpha color-calc ratio table is global and has eight shared slots. A depth-faded textured mesh cannot be presumed to use the same translucency path as an RGB Gouraud polygon. Choose one consistent global policy for the scene; never change the table per object.
5. Do not assume NBG1–3, generalized VDP2 raster waves, an analog pad, Slave SH-2 jobs, SCU DMA, SCSP DSP reverb, or a 4 MiB RAM cart are already first-class production dependencies. They are separately gated stretch goals.
6. No hidden heap, no unconditional whole-frame VRAM uploads, no large meshes per platform, no uncontrolled per-frame file/CD reads.

## 3. Player-facing design

**Course, not an open world.** Start on a generous island and use a readable S-shaped path through three visually different sections, with occasional side paths for pickups:
- **Pier:** teach D-pad camera-relative movement, jump, shadows and ledge depth. Three broad platforms, generous recovery area, first checkpoint.
- **Wind garden:** moving platform, staggered heights, simple floating collectible arcs, a wide safety landing below a tougher jump, second checkpoint.
- **Lighthouse:** a collapsing bridge with an unmistakable visual/audio warning, a short final ascent, and a finish arch that freezes the timer and shows a replay prompt.

All gameplay-relevant platforms are actual height-aware 3D solids with well-defined top surfaces and side walls; avoid thin, visually floating cards. The water is a fall/reset plane, not solid walkable ground. Checkpoints restore position, camera direction and transient platform states without requiring a save file.

Controls for the shipped baseline: D-pad = camera-relative movement; A = jump (shorter height when released early); B = rotate camera left; C = rotate camera right; START = pause/resume. Present controls in the intro/early HUD. Digital-pad support is sufficient; do not gate the game on optional analog input.

Movement: fixed-step 16.16 integration, responsive acceleration/deceleration, limited in-air control, capped fall speed, coyote time, buffered jump input, variable jump height, jump apex and landing effects. Keep avatar facing consistent with movement and smooth the follow-camera pivot. Camera must never snap through a solid wall: design broad sight lines first, then add a bounded camera-obstruction fallback if needed. Keep a simple diagnostic/reset camera mode for tests only.

Collision contract: the cube is an AABB (or an explicitly documented narrow gameplay capsule against AABBs), not a `sat_body3_t` sphere disguised as a cube. Implement small, deterministic, axis-separated / swept player-vs-platform movement with explicit feet-on-top, side hit, underside hit, and one-way moving-platform carry rules; use existing `sat_aabb3_overlap`/`sat_aabb3_contact` as building blocks where appropriate. Discrete contact without a sweep/substep guard may tunnel at jump/fall speeds and is not an acceptable final implementation. The moving platform updates **before** player collision and the player carries its signed platform delta only while grounded. Restrict active collision queries to a bounded local cell/sector list; profile before introducing a new library-level spatial API.

## 4. Saturn hardware presentation

**VDP2 (large-area scenery):** NBG0 for a deliberately seamless or correctly bounded scrolling panoramic sky, layered cloud silhouette, and color gradient baked into indexed art. RBG0 for the ocean perspective below the horizon; generate a compact repeating indexed texture at build time, preserve a fixed VRAM layout for its bitmap, coefficients, rotation parameters, NBG0 tile/map data and sprite textures, and implement bounded water motion by varying phase/scroll and optionally small, validated palette/rotation parameter changes. Never advertise scrolling as physical wave height. Keep screen horizon/camera pitch synchronization visibly plausible while jumping and climbing: cap pitch, keep far ocean at a chosen level, and use a designed distance boundary/fog band where flat RBG0 perspective diverges from elevated VDP1 geometry.

**VDP1 (gameplay):** textured or flat/Gouraud quads for connected islands, raised platforms, arch, bridges, collectibles and cube. Reuse a small atlas and a few simple mesh templates; only visible faces should issue commands. Prefer colorful low-poly geometry and textured accent faces to high-face-count source models. Cache immutable local geometry, reuse caller-owned transform/projection buffers, and use consistent winding/backface rules. Contact shadow is an inexpensive dark ground quad or dithered mesh, projected at the *supporting platform's top y*, drawn before the avatar and only while appropriate.

**Occlusion:** calculate one bounded visible render-item list with depth bounds (not one independent sorted list per mesh). Sort world **objects or platform clusters** far-to-near; draw each opaque cluster's faces in an internally valid order. Resolve cases where depth ranges overlap, where an arch surrounds the avatar, or where a near wall passes in front of the player with intentional split geometry / surface-level ordering or region-specific visibility rules. The avatar must not simply be drawn last through every wall. Test camera-facing sides, platform undersides and camera-near clipped faces, and reduce intersection-heavy compositions where painter ordering is ambiguous. HUD is always drawn last and excluded from world fading.

**Lighting and atmosphere:** fixed palette/material face shading and selected RGB Gouraud highlights for large solids (with **documented textured-face Gouraud limits**); horizon haze and coarse scenery distance cull. Use `sat_fade3d_eval()` + global VDP2 sprite color-calc slots **only on supported indexed textured geometry** after compositing is proven. For RGB polygons choose a consistent palette-distance tint / cutoff instead of claiming identical hardware alpha. Make optional soft-looking pickups or spray with small dithered/mesh sprites and strict overdraw limits. Do not spend the main frame budget on large stacked transparent fullscreen quads.

**Sound:** short preprocessed PCM effects and an unobtrusive, bounded loop/music track through the existing audio API. Confirm sample rate, clip peak, channel mixing, concurrent-voice pressure, looping and master-volume behavior against `docs/SCSP_AUDIO_STREAMING_GUIDE.md`; a recognizably clean soundtrack beats a technically ambitious distorted one. Do not require long CD streaming for first playable build.

**Optional visual polish AFTER base is stable:** nearest-ratio sprite-color-calc for selected spray/shimmer, higher-quality sea phase via **verified** RBG0 coefficient/palette changes, more distant decor, and short animation for rotating pickups. Non-static per-line water distortion is a **separate VDP2 API and hardware validation task**, not something to silently implement with unowned register writes inside this example.

## 5. Fixed resource ownership and performance budgets

Before art production, publish a one-page memory ledger with separate: main RAM executable/static/stack/scratch; VDP1 texture VRAM and command/Gouraud areas; VDP2 VRAM banks/cycle-pattern constraints for NBG0/RBG0; CRAM palette ownership; SCSP sound RAM; generated ISO size. Provide static maxima for world clusters, active platforms, moving parts, pickups, visible items, projected vertices, sortable faces, and per-frame VDP1 commands. Count *commands submitted*, texture bytes and changed VRAM words in debug mode. Handle `SAT_ERR_CAPACITY` deterministically; drop distant decor before essential platforms, player, hazards or HUD.

Initial **measurement hypotheses, not hardware guarantees**: 320×224, 30 FPS gameplay/presentation target on an NTSC timing profile; aim for 60 when evidenced. Start with ~12–20 concurrently relevant platforms, ~8 pickups and under ~250 submitted world-face commands/frame, then increase/decrease according to measurements. Separate display-frame counter from the logical fixed-step accumulator (e.g. 60 Hz simulation, max 3 catch-up steps and an explicit overload policy); audio updates and input edge events must not be replayed once per catch-up step. Profile VBlank elapsed, update, cull/sort/project, submit, VDP1 completion and streaming/update separately. Expose quality modes `full` and `reduced` without different physics/level geometry or progression.

Do not create a mandatory hardware/virtual expansion profile. An optional 4 MiB RAM cart cache/HD asset experiment can live in a separate later example and must preserve base-console parity.

## 6. Suggested file structure

```text
examples/skybridge_3d/
  README.md                # how to play, build, hardware techniques, limitations
  Makefile.inc             # explicit generated C/H assets
  main.c                   # init, state machine, frame ordering only
  game.h / game.c          # deterministic stage, progress, checkpoints
  player.h / player.c      # cube movement, jump, sweep/contacts
  camera.h / camera.c      # follow/orbit, view target, safe bounds
  level.h / level.c        # compact data-driven platforms/colliders/sectors
  render.h / render.c      # visible-list, depth, VDP1 draws, HUD
  background.h / background.c # VDP2 NBG0/RBG0 and phase updates
  sound.h / sound.c        # bounded effect/music lifetime
  assets/                  # original self-authored or clearly licensed sources
tests/host/test_skybridge_*.cpp or C # pure platformer logic tests integrated with make test
harness/tests/test_skybridge_3d.py # black-box registers/VRAM/input timeline assertions
harness/scripts/skybridge_*.pad    # replayable progression and edge-case inputs
```

Keep pure gameplay logic host-testable and hardware/platform code behind narrow adapters. If new reusable APIs are genuinely needed, write a minimal public contract and tests **in the library** instead of sneaking example-specific register code into the core.

## 7. Execution slices and hard gates

### Slice A — movement / graybox vertical slice
Create the example folder, Makefile.inc, main loop, fixed-step cube physics, jumping, camera and a single four-platform jump route. Render placeholder solids in VDP1 with an inexpensive plain VDP2 background. Prove land, miss, side hit, underside hit, camera yaw, coyote/buffered jump, respawn and deterministic level completion. **Gate:** physical traversal is reproducible in host tests and with a scripted pad timeline.

### Slice B — composed Saturn scene
Add NBG0 sky and RBG0 ocean, transparent VDP1 erase and stable commit ordering. Preserve screenshot-visible player and geometry on both sky and sea. Validate VRAM bank boundaries, palette indices (including index 0), layer priorities, horizon and camera-height transitions. **Gate:** neither VDP2 layer disappears when another is committed or when the HUD/fade paths are activated. Capture daytime screenshots at ground, apex, elevated platform and finish.

### Slice C — complete playable course
Build all three areas, local collision queries, eight pickups, two checkpoints, moving platforms, collapsing bridge, finish and pause/retry. Show collectible reachability and safe landing telegraphing. Add scripted replays for successful full-course route and intentional failures. **Gate:** all progress can be achieved with the public controls; no unreachable pickup or game state dead-end.

### Slice D — art, depth correctness, and hardware effects
Bake procedural/self-authored assets and palettes, add varied platform silhouettes, shadows, animations, face lighting, sea phase, clouds and selective supported distance fade. Audit global painter order and near-plane failure across the whole route, especially foreground walls crossing the cube and camera orbit. **Gate:** screenshots at prescribed coordinates show no player-through-wall, sudden platform disappearance, black-index speckles, accidental alpha or layer inversion.

### Slice E — sound, performance, documentation, acceptance
Add PCM effects/loop, debug counters, full/reduced quality profile, memory ledger and a short user-facing README. Run `make test`, `make EXAMPLE=skybridge_3d`, `./build-example.ps1 skybridge_3d` (adapt to actual script syntax), and `./harness/run-harness.ps1 skybridge_3d ...` using the repository's configured BIOS/toolchain; capture screenshots/PC profiles and perform emulator **visual** review. Also run adjacent `runtime_3d`, `vdp2_nbg0_rbg0_combo`, `distance_fade_3d` and `transparency_showcase` regressions. **Gate:** no verified gameplay/runtime/visual regression, stable audio, documented achieved display/update pacing and resource usage; report actual commands executed, failures and what could not be validated on physical hardware.

Follow `AGENTS.md`: agents run build/harness themselves rather than asking the user. Implement and commit coherent slices on `main` only when asked to implement; **this document itself is planning only**.

## 8. Deliberate non-goals

No enormous open world, generic scene graph rewrite, mandatory GLB character, bone animation, arbitrary UV-mapped per-pixel 3D ground, dynamic light/shadow maps, Z-buffer, free water physics, arbitrary alpha on every 3D material, required analog pad, required CD loads on gameplay frames, new multitasking kernel, or a requirement that every future roadmap subsystem be completed before the demo is playable.

The demo should demonstrate the hardware by **assigning work to the right chips**, while remaining a straightforward example that another developer can read and modify.
