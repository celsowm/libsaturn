# Skybridge 3D

A **playable, stock Sega Saturn 3D platformer example**. The pink low-poly pig is the avatar; the sea, sky and platforms demonstrate using the machine's distinct video processors together instead of trying to rasterize the whole world on the CPU.

## Play

Move with the digital D-pad (camera-relative), press **A** to jump (release early for a shorter jump), press **B / C** to rotate the follow camera by 15 degrees, press **Y** to toggle the extra platform/yaw/surface diagnostic HUD, hold **Z** for an active brake, and press **START** to pause/resume. The second HUD row always shows the player's signed **X, Y, Z world coordinates** to one decimal place (not the interpolated camera position). Reach the gold arch on the tenth platform to complete the course. **Press START on the Course 1 completion screen to begin Course 2**; after Course 2, START restarts at Course 1. **To try Course 2 directly, press START to pause, then X to switch courses**; it resets time, gems and checkpoints and resumes the selected course. **All eight gems are optional bonuses**; the finish never requires a minimum gem count, and the end screen shows how many you collected. The fourth and seventh platforms are checkpoints in both courses. Falling into the sea respawns you at the last checkpoint in the current course, retaining its collectibles. Course 1 retains the sideways moving platform and timed collapsing bridge. **Course 2** has ten different platforms with alternating narrow/wide landing zones, four elevators moving vertically at staggered phases, and an elevated finish. The moving decks carry the pig up and down while grounded; gems and the contact shadow follow the same physical platform height.

This demo has no mandatory RAM cartridge, external art downloads, controller extension, CD streaming, game save, or third-party model assets. The generated palette, ocean, sky and PCM all come from C source. A startup loading panel reports completed initialization work and tracks real ocean rows generated, not a fixed timer.

## Build

From the repository's MSYS2 environment with the SH-2 cross toolchain installed:

```sh
make EXAMPLE=skybridge_3d IP_TEMPLATE_KIND=sbl all
make test
```

From PowerShell, use the project wrapper:

```powershell
.\build-example.ps1 skybridge_3d
.\harness\run-harness.ps1 skybridge_3d -Bios .\bios\saturn_bios_us.bin -Frames 240 -BootFrames 90 -PadScript .\harness\scripts\skybridge_smoke.pad -Screenshot '200:skybridge.png'
```

The BIOS is your own dump and is not included in this repository. Consult `harness/README.md` for the direct-injection harness limitations. An emulator screenshot is a **visual diagnostic**, not proof of correctness on physical hardware.

## How it works

- `game.h` has a pure, deterministic 60 Hz 16.16 fixed-point gameplay loop and **two compact ten-platform courses** selected by `sb_start_course()`. It is tested directly by `tests/host/test_skybridge_game.cpp`. Collision checks use a bounded array of ten world AABBs and per-axis movement; no hidden heap or physics engine. Course 2's four elevators use a staggered 180-tick triangular vertical motion, and their 16.16 Y is shared by the renderer, the landing/side/ceiling collision and collectible checks. The player is carried by the deck's actual Y delta while grounded and stops being carried as soon as they jump or leave it. A descending/rising landing compares the player's previous feet against the deck's previous top, rather than only against a static height.
- `main.c` creates a **VDP2 NBG0 512×128 indexed sky** (palette bank 1) with a smooth gradient, atmospheric haze and two individually shaped island silhouettes, and a **VDP2 RBG0 512×256 indexed sea** (palette bank 0). `scenery.h` contains pure, allocation-free palette/pixel generators, tested by `tests/host/test_skybridge_scenery.cpp`. The sea uses sparse, elongated wave bands and a small, separately indexed foam/highlight palette instead of high-frequency XOR noise. RBG0 coefficients make sea rows above the 96px horizon transparent. Two slow, nonidentical time offsets to the 48-word rotation table move the sea without rebuilding or uploading its 128 KiB bitmap every frame; just eight highlight palette words are smoothly modulated every eighth displayed frame.
- VDP1 draws the platforms, low-poly pig, pickups, checkpoints and finish arch through an **opaque indexed material path** (palette bank 4) that also supports VDP2 color calculation at distance. This avoids mixing the near RGB-coded polygons with indexed faded sprites while the VDP2 sprite color-calculation registers are active. The reusable 16×16 platform-inset texture now has **three distinct palette themes** (banks 3, 5 and 6), with narrow edge highlights and low-poly metal corner supports on landmark platforms; the collision course remains unchanged. Nearby sides remain solid and opaque; the pink pig uses indexed VDP1 surfaces while gems remain golden. Its model fits the original 4×4×5 player hitbox, so collision and moving-platform behavior are unchanged. **Platforms at view depth 66–134 gradually fade** using `sat_fade3d_eval` and eight hardware blend ratios. Each platform retains its previous fade slot with a two-world-unit hysteresis band to prevent depth-threshold oscillation; the platform the player is currently standing on stays opaque. Faded objects occupy priority 6, above RBG0 priority 5, while ordinary indexed objects and the HUD remain at priority 7. The **LibSaturn scene3d painter queue** now orders submitted objects by full camera-space depth (including pitch) without a game-specific sorting loop. Three explicit passes keep the large world decks first, the supporting deck second and the pig/gems together in the third pass, where they properly occlude one another as the camera rotates. This pass structure is an explicit trade-off for large unsplit decks on hardware without a depth buffer; the avatar is **not** always rendered after the gems. The contact shadow is a small opaque indexed surface, not a giant mesh-dither patch; the text font uses palette bank 2.
- Cloud silhouettes are transparent, shaded **VDP1 indexed sprites** (64×16 source pixels in palette bank 7, scaled to varied sizes in the upper 64 screen rows). They drift independently from the NBG0 island silhouettes instead of repeating the same zigzag motif. The NBG0 horizon pans at one pixel per 128 display frames while the VDP1 cloud wind advances one pixel per 32 frames; both respond to camera yaw, while the RBG0 sea uses its own flowing rotation-table offsets. This is lightweight two-speed parallax without pretending that NBG1 or per-line VDP2 scrolling is implemented. Clouds are submitted **before the 3D world**, never over the player, and do not enter the scene's distance-fade pipeline.\n- The frame uses a transparent VDP1 erase, stages the RBG0 rotation and NBG0 sky position and calls `sat_vdp2_layers_commit` **immediately after entering VBlank, before polling the controller**. `sat_vdp2_sprite_color_calc_configure_alpha(7)` is performed once at startup; its selector-0/selector-1 priority pair is now synchronized with the generic VDP2 layer shadow. Previously `sat_vdp2_layers_commit` reset `PRISA` to the all-normal shadow (`0x0707`), while a later color-calc commit restored the faded selector (`0x0607`), sometimes waiting for the *next* VBlank. That priority race caused visible alternating opaque/faded frames even at a constant distance, which per-platform fade hysteresis cannot solve. The explicit color-calc commit is therefore no longer repeated every frame. Do not use `sat_app_frame_begin`, which clears VDP1 to opaque and can hide VDP2 scenery.
- Startup first creates the font, then shows progress while preparing the indexed platform texture, sky pixels, ocean rows, VDP2 composition and audio. The scene only starts after these tasks succeed. Five generated PCM S8 sound effects are registered as logical assets through `sat_asset_register` and loaded via `sat_sound_load`; the looping music remains a direct `sat_sound_create` because the current typed sound loader does not preserve the source's loop setting. No asset prefetch is triggered for embedded arrays: `sat_asset_prefetch_submit` is specifically for nonresident, filesystem-backed data, and its cooperative update must be serviced explicitly by a future CD-backed game.
- The loading screen is drawn with VDP1 before VDP2 is configured and covers the screen with an opaque rectangle while keeping the *erase* transparent, avoiding the usual `sat_app_frame_begin` layering trap.

## Current scope and trade-offs

This is the **first playable implementation**, not the full production acceptance of `docs/SKYBRIDGE_3D_PLATFORMER_EXAMPLE_PLAN.md`. It deliberately uses simple low-poly geometry, no GLB assets, no bespoke runtime alpha engine, no optional Slave SH-2, DMA, cart cache, save files, or advanced per-line water effects. The current broad-phase is a ten-AABB scan *per current course*, not the large-world spatial grid. The follow camera is fixed-distance and can intersect arbitrary additional geometry if you modify the level; the world is laid out with generous sight lines. The sample uses the public caller-owned `sat_scene3d_queue_t` with space for 19 objects rather than implementing its own sorting/depth arrays. Large platforms remain an explicit earlier painter pass, and exact platform/actor intersections may still require deck subdivision or per-face ordering. The sample keeps game-specific fade decisions, collision and object visibility, not its own painter algorithm. Indexed color-calculated platforms blend against the **VDP2** backdrop, not a separately composited earlier VDP1 object; distant overlapping platform geometry is therefore intentionally kept sparse. This regression fix intentionally uses indexed, flat-shaded materials both near and far; it postpones Gouraud RGB lighting until a compatible and visually validated rendering path is available. Fixed 60 Hz game ticks are capped at three catch-up ticks per displayed iteration.

Performance, collision traversability and visual/audio behavior **must be validated in a Saturn emulator and ideally on original hardware** before making frame-rate or visual-correctness claims. Tests cover discrete gameplay invariants, optional completion and 3D collectible contact; they do not substitute for playing the entire course. See `AGENTS.md` and the published plan for the next acceptance gates.


## Generic scene3d painter queue

This game uses the reusable deferred LibSaturn queue from `include/saturn/scene3d.h`: a caller-owned array of 19 object slots is initialized once, then each frame submits currently visible platform, pig and gem callbacks with world-space painter anchors. LibSaturn computes normalized camera depth for the game's fade decisions, sorts each painter pass deterministically in O(n log n), and invokes each submitted callback exactly once on flush. The platform and game rules stay here, but the former game-specific `g_items`, `g_actors`, `view_depth()` and `sb_actor_view_depth()` have been removed. A separate generic `submit_model` API reuses the existing immediate-mode `sat_scene3d_draw_model` facade for compiled model assets. See `docs/SCENE3D_PAINTER_QUEUE_PLAN.md` for memory, validation and known limitations.

The three explicit passes are **0 world decks, 1 supporting deck, 2 pig and gems**. Passes 0/1 preserve the sample's original large-platform painter workaround, not a hardware Z-buffer. Only objects in the same pass are ordered against one another; when two large meshes intersect, a single anchor cannot guarantee perfect per-pixel visibility. The VDP2 distance-fade priority, platform activation, gem physics and HUD remain managed by their existing systems.

## Acceleration, braking and surface friction

The pig has independent X/Z 16.16 velocity controlled at 60 simulation ticks per second, with a top commanded speed of 1.5 world units/tick per axis. Normal grounded acceleration remains the original 0.2 unit/tick, while air steering and air coasting are weaker (about 0.0833 unit/tick). **Reversing direction while already moving** applies a stronger 0.4 unit/tick brake before building speed in the opposite direction. **Holding Z** actively brakes the horizontal velocity to zero (0.6 unit/tick on the ground; 0.125 in the air), taking precedence over D-pad input while held. Z does not cancel vertical jumps or the carry applied by moving platforms/elevators.

Each platform explicitly declares `SB_SURFACE_NORMAL`, `SB_SURFACE_SLICK` or `SB_SURFACE_GRIP` in `game.h`. **Releasing the D-pad** on normal ground slows by 0.2 unit/tick, on a slick deck by 0.05 unit/tick (longer slide), and on a grippy deck by 0.4 unit/tick (quicker stop); coasting in air retains the previous 0.0833-unit rate. These are deterministic per-tick arcade controls, not a claim of realistic mass-based friction. Releasing the input and actively braking clamp at *exactly zero* without overshooting into reverse motion.

Course 1's fixed second platform remains **normal**, preserving its movement behavior while the separate camera/VDP1 regression is investigated. Course 1 deck 4 (checkpoint A) has a warm-colored grippy surface and deck 6 a cyan slippery surface. Course 2 deck 4 is grippy and deck 8 slippery (1-based deck numbers). Slick and grippy floor bases have distinct accent colors; the optional Y diagnostic HUD displays `NORMAL`, `ICE`, `GRIP` or `AIR` based on the player's actual supporting platform. None of these surface effects changes the vertical collider, gem contact, checkpoints, jump impulse or camera.

The rules above belong to this example's gameplay controller; the generic LibSaturn 2D and 3D physics APIs already expose `drag`/`floor_friction` parameters but are **not** silently changed by this feature. `tests/host/test_skybridge_game.cpp` checks per-surface stopping, active braking, reversal, air control and uninterrupted elevator carry.

## Course 2: narrower decks and vertical elevators

Course 1 remains the original horizontal-moving/collapsing platform demo. Course 2 is a separate raised course with ten deck definitions, narrower jump targets (some only seven world units in half-width), and four gold-accented elevators at indices 2, 4, 6 and 8. Each elevator cycles vertically through eight world units in 180 simulation ticks; the offsets are staggered so they do not all reach their high/low points together. Gem height, checkpoint markers, the pig's contact shadow, elevator support columns and the player's grounded feet all read **the same current platform height**, preventing the avatar from visually floating above or sinking into a moving lift.

START after Course 1 completion begins Course 2 and clears that course's gems, time and checkpoints. START after Course 2 completion restarts Course 1. While paused, X deliberately switches between courses and immediately resumes gameplay so the elevator demonstration can be tested without finishing Course 1. The title HUD shows C1/C2 and the X/Y/Z row remains live. Checkpoints remain on the fourth and seventh decks; the goal remains accessible without collecting any gems. The original course's special moving/collapse objects are activated by each platform's kind, not a globally hard-coded platform index. The new host regression runs all four elevators through a complete 180-tick period, checks grounded carry and jump detachment, moving-height gem collision, both course resets, a narrow-deck footprint and completion with zero gems.

To compare elevator motion without traversing Course 1, the new `harness/scripts/skybridge_course2_lifts.pad` controller script pauses then selects C2. With your own Saturn BIOS and a built ISO, capture multiple frames with the camera stationary:

```powershell
.\harness\run-harness.ps1 skybridge_3d -Bios .\bios\saturn_bios_us.bin -Frames 300 -BootFrames 90 -PadScript .\harness\scripts\skybridge_course2_lifts.pad -Screenshot '140:lifts_a.png','230:lifts_b.png','290:lifts_c.png'
```

Look for distinct fixed and vertically moving gold decks, a constant HUD C2 indicator, and gems staying centred above their own decks; screenshots alone cannot establish whether the moving-platform carry is collision-correct.


## Low-poly pink pig avatar

The player is now an original, fully procedural low-poly pig rather than a cube. Its indexed VDP1 geometry consists of a pink body and head, a protruding snout with two dark nostrils, two triangular ears, eyes, four short animated hooves and a small raised tail. It reuses the existing `box3()` and `put_quad()` pipeline, with no GLB or runtime asset loader required. The pig is deliberately limited to the previous 4×4×5 collision volume and stays opaque even when the distant platforms fade through VDP2 color calculation. Neither pickup radius nor physics, checkpoint, course or elevator handling changes.

`game.h` stores a persistent cardinal `facing_x/facing_z` direction, updated when horizontal velocity has a clear dominant axis. An idle state, jump or camera-only turn does not cause its nose to swing toward the viewer. It spawns facing the camera to show its snout; walking away reveals its ears and tail. Host tests check facing persistence and resets. **Visually validate** the pig from behind, from both sides and during a jump in an emulator: polygon submission and facial details cannot be proven by gameplay tests alone.

## Optional gems and 3D contact

The eight collectibles are distinct **golden octahedra** (eight triangular facets, floating gently above platform centres), deliberately different in color and silhouette from the pink pig. Their centre and collision radius are shared through `game.h`. Pickup uses the character's actual 3D position and extents: circle-versus-player AABB in the XZ plane, plus overlapping vertical intervals that account for the gem's half-unit bob. The check runs while grounded **and airborne**, so a jump through a gem collects it; standing anywhere on its platform does not automatically award it. A gem on the moving deck follows the deck's current X, and a gem disappears while its collapsing deck is inactive.

The goal checks only whether the player is grounded on the final platform. Finishing with 0/8, 3/8 or 8/8 gems is valid; the score is displayed on the completion overlay. `tests/host/test_skybridge_game.cpp` covers distant/diagonal/vertical misses, midair collection, one-time pickup, moving deck positions, collapse state and zero/partial-gem victories.

## Fixed second platform: consecutive steps and jump

The user's regression is **Course 1's fixed second platform**, stage index
1 at Z=36, with the player near X=7, Y=0, Z=30. The collapsing platform is
stage index 7 at Z=235 and is **not** involved. The previous change only
clipped world polygons near the eye; the user confirmed it **did not fix the
bug**, and jumping also made the HUD disappear. That implicates VDP1 frame
processing or a draw-time error as well as ordinary visibility: the HUD is
submitted after world geometry into the same VDP1 command list.

The additional rendering safeguards are:

- `sat_clip_quad_screen()` in `render3d.h` is a reusable,
  allocation-free **solid-color** viewport clipper. After the near-plane
  clipping and 3D projection, Skybridge bounds all uniform indexed quads
  to the native 320×224 screen before submitting VDP1 distorted sprites.
  The old near-only clip could produce enormous off-screen VDP1 raster
  commands even though the polygon was in front of the eye.
- Patterned floor insets have no UV-aware clipper yet. Draw the textured
  inset only if the **original four corners** survive the near plane
  unchanged and its projected corners are entirely on-screen. A clipped
  triangle may also have `clipped_count == 1`, so count alone is NOT a
  sufficient safety check. The clipped solid floor remains visible.
- When the chase camera is physically inside the **horizontal footprint
  of an already-passed, non-supporting deck**, avoid rendering that deck
  immediately around the eye. At the reported Z≈30 position, the camera
  is roughly Z≈−12, inside the previous pier's Z=[−18,18] footprint. This
  safeguard works for both courses and does not change the pig, collision,
  collectibles, actual deck placement or the supporting platform.

Tests in `tests/host/test_render3d_logic.cpp` cover near+screen clipping
for successive Z=30..33 steps and several jump heights. They verify that
generated triangles fit inside VDP1's screen bounds. **They do not verify
real VDP1 raster time or prove the HUD no longer disappears**.

Visual gate: build the latest Skybridge ISO and reproduce X≈7, Y=0,
Z≈30→31→32, then jump in place on the second fixed platform. The HUD,
pig and distant platforms must update each frame; an older pier may
disappear when the camera crosses it, but **the current platform cannot
vanish or freeze**. Check whether the displayed TIME keeps changing if
any object disappears and record two neighboring frames to distinguish
a game-code stall from a VDP1-frame backlog. Do not label this case a
collapsing bridge or claim fixed based solely on a host test.

## Pig/gem camera occlusion regression

If the player stays at a fixed world XYZ position while only B/C rotates the camera, the octahedral gem can move in front of or behind the pig in screen space. **VDP1 has no Z-buffer**: the older `stage_box()` drew each gem as part of its platform and then unconditionally drew the pig last, so the pig covered *even a gem that was physically closer to the camera*. The LibSaturn `sat_scene3d_queue_t` now accepts the platforms, pig and gems as deferred drawing items. It renders the pig and gems **back-to-front using their own 3D camera-space depths, including the pitched camera's Y direction**, without keeping a Skybridge-specific actor list or sorting loop. The pig is not automatically last. All 3D positions, gem hit tests, pickup state, shadow and collision remain unchanged.

The reusable host regression `tests/host/test_scene3d_api.cpp` submits two stationary actors from opposite camera positions: reversing the view reverses their painter order, while world positions stay unchanged. It also verifies pitch-sensitive order, stable depth ties, capacity errors, callback reentrancy, explicit passes and queued models. Confirm in the emulator by capturing the same XYZ position with a gem in view, before and after rotating the camera approximately 180 degrees. When the gem is closer, its yellow silhouette should appear over the pink pig where they overlap; when the pig is closer, it should cover the gem. The yellow vertical poles next to some platforms are **checkpoint/finish decorations**, not collectible gems. Large platform geometry remains in explicit earlier painter passes; the library orders within each pass by camera depth, but exact per-polygon occlusion for intersecting geometry would require subdivision or a more advanced scene renderer.

## Camera-only orbit regression

In the previous build, the avatar and the collectible were both orange/yellow cubes, the avatar's white direction marker appeared only from one angle, and the camera orbited at different x/z radii (33 vs 43). The collision engine also treated any partial player/platform AABB overlap as a stable floor, even when the player's center was already outside the deck. Together these made a stationary camera turn look as if the avatar had jumped off or was hanging in front of a floating platform.

The avatar is a pink low-poly pig with a persistent world-space facing, snout, ears, hooves and tail; pickups are gold **eight-facet 3D diamonds**, not player-shaped cubes. The camera's horizontal arm has a constant radius of 42 world units and a constant eight-unit look-ahead along either horizontal axis. Landing/grounding requires the cube's footprint to remain mostly over the supporting platform; the earlier broad collision test remains in place for lateral/head contact, and coyote time still permits edge jumps. Rotating the camera cannot alter world-space player coordinates or platform support.

Reproduce the exact *camera only, no movement* case with a newly built ISO and your own BIOS, saving frames before and after six 15-degree B taps (90 degrees total):

```powershell
.\harness\run-harness.ps1 skybridge_3d -Bios .\bios\saturn_bios_us.bin -Frames 165 -BootFrames 90 -PadScript .\harness\scripts\skybridge_camera_orbit.pad -Screenshot '75:orbit_before.png','145:orbit_after.png'
```

The pink pig should remain on the *same physical starting platform* in both images, while the golden collectible changes apparent screen-space position with normal parallax. The XYZ row should show the same player coordinates before and after a camera-only turn. The test script switches on the extra debug HUD with Y; both screenshots should show `DECK 1`, while `YAW` differs by 90 degrees (the initial yaw 0 and six B taps produce yaw 270). The two screenshots alone do not prove the player's world position: that invariant is covered separately by `tests/host/test_skybridge_game.cpp`. If the apparent contact still looks wrong, inspect the avatar's small contact shadow and the deck's edges in the moving camera video rather than inferring collision from overlapping screenshots.

## Scenery visual regression

After compiling the updated ISO, compare three emulator screenshots (or video frames) with the camera still and three more while rotating B/C. The island silhouette should not look like evenly repeated teeth; clouds should form distinct irregular clusters and move horizontally against the slower island skyline. The sea should exhibit broad bands and occasional foam glints, not uniform blue-white noise. Check that the 96px RBG0/sky boundary stays in place, that only the water animates below it, and that neither a cloud nor a platform outline alternates between visible and invisible on successive frames. The VDP1 clouds are 2D background art, **not** actual volumetric clouds; no extra NBG1 layer, full-frame bitmap uploads, or geometry changes to the playable path were introduced.

\n## Distance-fade regression

**Temporal acceptance:** with the controller released, a distant platform should remain at the same transparency level across successive displayed frames. Compare neighboring emulator frames at a fixed camera location; the ocean may move underneath the blended surface, but the platform must not alternate between opaque and translucent. The latest code addresses the global `PRISA` priority replay race, independently of the existing per-object distance hysteresis. Host tests and an ISO cross-build cannot establish temporal visual stability; verify it with a multi-frame emulator capture and, when possible, original hardware.

To verify the fade and the repaired controls, walk right at default camera yaw (screen-right should move toward world -X), stand at both the near and far edges of the first, second, and checkpoint platforms (the pink pig and each platform's solid sidewalls must remain visible), then compare emulator screenshots with the camera approaching the second or third platform; the platform should progress through several transparency steps as its view depth crosses 134 down to 66, rather than suddenly appearing at a culling boundary. The near platform and player must stay opaque while the ocean and sky remain visible through the far one. The `distance_fade_3d` example independently verifies the supported LibSaturn indexed-sprite/VDP2 blend path. `make test` checks fade quantization in host tests; visual correctness on the chosen emulator/hardware still requires actual screenshots.
