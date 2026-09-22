# Runtime ownership and example migration ledger

This is the implementation companion to
`EXAMPLE_WIDE_DRY_SOLID_RUNTIME_REFACTOR_PLAN.md`. The canonical game-facing
3D frame contract is `sat_scene_t` in `include/saturn/scene.h`; it delegates to
the single scene-wide face painter in `scene3d_faces.h`. Storage is always
caller-provided and every frame has an explicit HUD reservation.

| Responsibility | Public owner | Deliberate exception |
| --- | --- | --- |
| 3D camera state and orbit/follow policy | `scene3d.h`, `orbit_camera3d.h`, `follow_camera3d.h` | raw matrix math for diagnostics |
| Scene submission, clipping, stable painter order | `scene.h` → `scene3d_faces.h` | raw VDP1 probes; static-view data is replayed through `sat_scene_t` |
| Geometry and immutable model data | `mesh3d.h`, `model3d.h` | authored stage tables |
| Animation pose preparation | `anim3d.h` | offline asset baking |
| 2D camera, atlas regions and drawing | `render2d.h`, `texture.h`, `sprite_anim.h` | raw sprite probes |
| Fixed-step simulation and collision | `physics.h`, `collide2d.h`, `collide3d.h` | intentionally different 2D/3D manifolds |
| Files, assets, CD prefetch, music | `file.h`, `asset.h`, `audio.h`, `cd.h` | synchronous CD block probe |
| VDP2 environments and transfer math | `vdp2.h`, `vdp2_rbg0_ground.h` | raw NBG/RBG0 teaching probes |
| VDP1 command budget and overlay quota | `vdp1.h`, `scene.h` | no hidden growth |
| Mode-7 coefficient/rotation transfer lifecycle | `vdp2_environment.h` | game-owned bitmap/palette and typed matrix overrides |
| Typed persistent save wire format | `save_schema.h` | caller-owned serializer and device I/O |
| Bounded RAM/VRAM/command/audio planning | `resource_plan.h` | caller chooses limits and quality tier |
| Screen-space HUD text/value/bar | `hud.h` | raw probe overlays may draw directly |
| 2D sprite atlas state and region selection | `sprite_anim.h` | authored procedural pixels remain game-owned |
| Bounded logical asset registration | `asset.h` manifest registration | CD source mounting remains explicit |

## Example classification

`canonical`: game-facing runtime contracts; `feature`: focused capability;
`probe`: intentionally raw hardware evidence.

| Classification | Examples |
| --- | --- |
| canonical | `hello_world`, `mvp_2d_scene`, `runtime_2d`, `runtime_3d`, `skybridge_3d`, `pacman_2d`, `pacman_3d`, `infinite_explorer`, `basic_3d_animation`, `basic_3d_texture`, `sega_bg`, `text_sprite` |
| feature | `audio_showcase`, `cd_streaming_jukebox`, `distance_fade_3d`, `input_move`, `physics_2d`, `physics_3d`, `ram_cart_demo`, `red_square`, `save_backup_demo`, `transparency_showcase`, `vdp2_nbg0_rbg0_combo`, `voxel_terrain` |
| probe | `cd_block_probe`, `input_debug`, `save_cartridge_probe`, `tvstat_probe`, `vdp2_nbg0_ground`, `vdp2_nbg0_ground_api`, `vdp2_nbg0_image`, `vdp2_rbg0_ground`, `voxel_display_probe` |

## Validation ledger

| Slice | Result |
| --- | --- |
| Host C++ suite | PASS — all existing host tests |
| Python suite | PASS after restoring pure `gen_ip_bin.build_ip_bin` and CLI aliases |
| Saturn build | PASS — all 33 entry points built and linked through `build-example.ps1`; the final Skybridge follow-camera migration was rebuilt and linked with the common library |
| Modified Ymir harness | PASS — `hello_world`, `runtime_3d`, `basic_3d_texture`, `basic_3d_animation`, `skybridge_3d` (including 300-frame gameplay), `infinite_explorer`, `pacman_2d` and `pacman_3d` booted and emitted 320×224 captures; migrated 3D paths were visually inspected |
| Mednafen launcher | PASS — installed Mednafen found both BIOS files and launched rebuilt `pacman_2d.cue`, `physics_3d.cue`, `distance_fade_3d.cue`, `runtime_3d.cue` and `pacman_3d.cue` for controlled smoke intervals; no emulator process was left running |
| VDP2 environment slice | PASS — `sat_vdp2_ground_environment_t` owns bounded coefficient/rotation staging and transfer; Skybridge uses it and passed a 300-frame Ymir gameplay capture |
| Explorer environment migration | PASS — Explorer now uses the bounded environment controller for coefficient/mode-7 lifecycle and commits only its typed yaw matrix override; 120-frame Ymir boot passed |
| Explorer render ordering | PASS — Explorer no longer carries its own insertion sort; visible item indices use the shared bounded `sat_sort_indices_desc` path and a 120-frame modified-Ymir smoke passed |
| Explorer canonical scene route | PASS — rocks, contact shadows and drones now submit through the same bounded `sat_scene_t` painter as the animated Egg Mobile; landmarks and HUD remain explicit screen-space overlays; 120-frame modified-Ymir smoke passed |
| Skybridge follow-camera policy | PASS — bounded `sat_follow_camera3d` now owns anchor smoothing, authored offsets and fall/reset snapping; game code retains only yaw/input and course-specific offsets; host tests, Saturn build and 120-frame modified-Ymir smoke passed |
| VDP2 layout validation | PASS — environment activation now rejects bitmap/rotation/coefficient VRAM overlap, out-of-range CRAM spans and invalid layer priorities; host rejection cases plus 120-frame Skybridge/Explorer Ymir runs passed |
| Pac-Man 16-view visual gate | PASS — `harness/scripts/pacman3d_16_angles.pad` drove one L edge through all 16 headings; modified Ymir emitted 16 320×224 captures, with board, pellets, actors and HUD visible in inspected headings 0/4/8/12 |
| Skybridge camera visual gate | PASS — the camera-orbit script produced 320×224 before/after captures at frames 120/170 after loading; both retain the pig, sea/sky layers, HUD and deck, and the PNG hashes differ after the six B taps |
| Skybridge course visual grid | PASS — rebuilt modified-Ymir captures covered the Course 1 fixed-second-platform/jump regression at frames 160/185/212, Course 2 elevators, Course 3 genuine hole piers (with the hole HUD hint and changed gem count), and Course 4 tilting-ramp presentation; the camera-only before/after pair retained the deck, pig, sea/sky and HUD |
| Save schema slice | PASS — `save_backup_demo` uses a versioned header/checksum payload; two-process modified-Ymir persistence acceptance passed with boot counts 1 → 2 |
| Resource planner slice | PASS — caller-owned bounded planner has explicit limits, required/optional overflow errors and alignment validation; Skybridge reserves WRAM, VDP1 commands and audio staging before activation and rebuilt/booted in Ymir |
| HUD/sprite/manifest slice | PASS — HUD text/value API is covered by host tests and used by Runtime 3D/save/jukebox; Runtime 2D selects atlas regions through `sat_sprite_region_anim_t`; jukebox registers its CD tracks through a bounded asset manifest |
| Audio lifecycle qualification | PASS — modified Ymir playback trace for `cd_streaming_jukebox` ran 240 frames with a C play transition, observed active SCSP samples and zero underruns; refill counters remained zero in this short run because the trace did not cross a refill boundary |
| CDFS source manifest slice | PASS — `sat_cdfs_register_source_manifest` validates all extents before backend registration; a 280-frame Ymir script switched tracks twice and paused/resumed playback, observing active samples and zero underruns |
| Resource/performance snapshot | PASS — current ELF section sizes, modified-Ymir PC samples, runtime_3d SH-2 instruction counts and representative VDP1/VDP2/CRAM write-word comparisons are recorded in `docs/RESOURCE_PERFORMANCE_LEDGER.md`; fixed frame-cycle budgets are not misreported as CPU work |
| Scene command telemetry | PASS — `sat_scene_stats` reports submitted/flushed/culled/fallback/rejected faces plus VDP1 commands used/capacity and HUD reservation; `sat_view_cache_stats` reports baked entries, cache hits and ready views |
| Skybridge dual-SH2 integration | PASS — `MASTER`, explicit `SLAVE`, and `AUTO` builds reached the same 360-frame gameplay script; the final Slave run recorded 355 frames with Slave instructions, while the harness supplied Ymir's reset-vector handoff for the real guest lifecycle |
| Scene facade replay/API test | PASS — `sat_scene_replay_view_item` keeps pre-projected static views on the canonical scene world-pass path; host coverage verifies replay count, command stats and active/flush lifecycle |
| Persistent instance slice | PASS — Basic 3D texture/animation, Runtime 3D, Explorer, Pac-Man 3D and Skybridge retain immutable mesh/material bindings outside the frame loop; Pac-Man's three mouth variants and ghost body are prepared once, while only transforms/material state are updated per frame |
| Static view cache slice | PASS — bounded `sat_view_cache_t` owns caller-provided per-view entries, stable far-to-near bake ordering and replay; Pac-Man 3D migrated its 16 camera views, retained pellet occupancy tags and now replays board/cache items through `sat_scene_t` |
| Surface/collider slice | PASS — bounded `sat_surface3d` owns planar height, hole-to-solid-slice decomposition and full-footprint support; Skybridge collision/render wrappers now consume the shared surface math and passed host/Ymir validation |
| Physics 3D scene route | PASS — the feature example now uses `sat_camera3d` plus `sat_scene_t` for floor, AABB faces and dynamic sphere faces, and `sat_hud_t` for its overlay; its ray/AABB/spatial probes remain game-owned; 120-frame Ymir capture passed, including a B-triggered bounce capture |
| Distance-fade scene route | PASS — fade slot selection remains demo-owned, while camera depth, textured quad projection, ordering and color-calc emission now use `sat_scene_t`; two 320×224 Ymir captures retained the 8-slot fade showcase and HUD |
| Runtime 3D camera ownership | PASS — `runtime_3d` now fits and updates `sat_orbit_camera3d_t` from model bounds; no example-local yaw/pitch/view-matrix implementation remains; a 180-frame Ymir run and 320×224 capture passed |
| Pac-Man cache replay ownership | PASS — the rebuilt Pac-Man 3D path ran 340 frames in modified Ymir with `pacman3d_16_angles.pad`; captures at frames 90/180/300 retained maze, pellets, actors and HUD while all static board/cache emission went through `sat_scene_t` |
| Runtime 2D visual gate | PASS — direct Ymir screenshot inspected at 320×224; atlas sprite, dynamic texture, music/status HUD and metrics remain readable after the single-input-poll migration |
| Resource/performance deltas | PASS — fixed-window Pac-Man transfer deltas were re-audited against startup progress and aligned input timing; no-input and 16-angle gameplay tails are -0.21% and -0.20% VDP1 words respectively; broader hardware timing remains open |
| RAM expansion cartridge grid | PASS — rebuilt modified-Ymir probe accepted `none`, `1m` (1,048,576 bytes) and `4m` (4,194,304 bytes), including the eight-byte bank-edge read/write assertion; expanded-cart guest screens reported status 0 and `BANK CROSSING: PASS` |
