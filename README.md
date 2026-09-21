# libsaturn-1

Bare-metal library for Sega Saturn game development without SGL/libyaul.

## MVP Status

This repository delivers the `2D Core` MVP:

- Bare-metal runtime (startup, linker, frame loop in VBlank).
- Minimal HAL for VDP1, VDP2, SCU and SMPC.
- Public C API (`include/saturn/saturn.h`) with internal C++ core.
- Build pipeline for `ELF -> BIN -> ISO`.
- Playable 2D demo in `examples/mvp_2d_scene`.
- Simple movement demo in `examples/red_square`.
- Controller held/pressed/released debug HUD in `examples/input_debug`.
- Separate texture demo in `examples/text_sprite`.
- Procedural 360-degree VDP2 exploration game in `examples/infinite_explorer`,
  combining an RBG0 infinite ground, an NBG0 panoramic sky and VDP1 gameplay.
- 8-bit indexed asset converter in `tools/convert_indexed8.py`, with output in `C/H` for embedding in build.

## Main Structure

- `include/saturn/saturn.h`: Public C API.
- `src/core`: Core implementation, startup and linker script.
- `src/hal`: Direct hardware register access.
- `examples/mvp_2d_scene`: MVP validation demo.
- `examples/red_square`: Red square moved by D-pad.
- `examples/input_debug`: HUD showing digital pad held/pressed/released state.
- `scripts`: MSYS2 setup, toolchain build, smoke build and PowerShell automation.
- `tools`: Utilities (generation of `ip.bin`, asset converter).

## Windows Flow (PowerShell)

For Windows 10/11, use the PowerShell flow (no need to manually open bash):

```powershell
.\scripts\bootstrap-msys2.ps1 full
```

To prepare everything at once (host + toolchain + emulators), use:

```powershell
.\scripts\bootstrap-dev.ps1
```

The script tries to locate MSYS2 in this order:

1. `-Msys2Root`
2. `LIBSATURN_MSYS2_ROOT`
3. `C:\msys64`

If MSYS2 is not found, it attempts to install via `winget install MSYS2.MSYS2`. If that fails, it displays manual installation instructions.

Available commands:

```powershell
.\scripts\bootstrap-msys2.ps1 host
.\scripts\bootstrap-msys2.ps1 full
.\scripts\bootstrap-msys2.ps1 smoke
.\scripts\bootstrap-msys2.ps1 acceptance
```

Useful flags:

```powershell
.\scripts\bootstrap-msys2.ps1 full -Msys2Root C:\msys64 -LogPath .\build\bootstrap.log
.\scripts\bootstrap-msys2.ps1 host -NoInstall
```

## Emulators (Windows)

Prepare the `emulators/` folder and install Mednafen via MSYS2:

```powershell
.\scripts\download-emulators.ps1
```

If the package is not available in the current MSYS2 repo, the script attempts to install Mednafen via winget (`MednafenTeam.Mednafen`).

This creates the optional legacy emulator launcher:

- `emulators/mednafen/run-mednafen.ps1`

For Mednafen, keep JP BIOS in `firmware/sega_101.bin` and US/EU BIOS in `firmware/mpr-17933.bin`.
The launcher attempts to automatically copy from `bios/saturn_bios_jp.bin` and `bios/saturn_bios_us.bin` (or `bios/saturn_bios_eu.bin`).
By default, the Mednafen launcher uses `region_autodetect=1` with fallback `region_default=na` and forces `ss.h_overscan=0` / `ss.videoip=0` to avoid cutting/artifacts on the license screen.

For the repository's current acceptance flow, use the modified Ymir harness
with the BIOS dump in `bios/`, for example:

```powershell
.\harness\run-harness.ps1 runtime_2d -Bios .\bios\saturn_bios_us.bin -Frames 120 -BootFrames 90
```

## Acceptance Checklist

Run the host suite and the modified Ymir probe with the BIOS from `bios/`:

```powershell
make test
.\harness\run-harness.ps1 runtime_2d -Bios .\bios\saturn_bios_us.bin -Frames 120 -BootFrames 90
.\harness\run-harness.ps1 runtime_3d -Bios .\bios\saturn_bios_us.bin -Frames 120 -BootFrames 90
.\harness\run-harness.ps1 cd_streaming_jukebox -Bios .\bios\saturn_bios_us.bin -Frames 300 -BootFrames 90
```

## Host Requirements (MSYS2 Shell)

Run in UCRT64 or MINGW64 shell:

```bash
bash scripts/bootstrap.sh host
```

## SH2 Toolchain Build

```bash
bash scripts/build-toolchain.sh
```

Shortcut for host + toolchain in one command:

```bash
bash scripts/bootstrap.sh full
```

By default installs in `$HOME/saturn-tools` and produces `sh2eb-elf-gcc`.

## MVP Build

```bash
make
```

Outputs:

- `build/mvp.elf`
- `build/mvp.bin`
- `build/mvp.iso`
- `build/mvp.cue`
- `build/libsaturn.a`

Boot diagnostics profiles (IP.BIN):

```bash
make IP_PROFILE=current
make IP_PROFILE=safe
make IP_TEMPLATE_KIND=yaul
make IP_TEMPLATE_KIND=sbl
```

Each build also generates artifacts named by variant:

- `build/mvp-<ip_profile>.iso`
- `build/mvp-<ip_profile>.cue`

Automated 1x2 matrix (build + decision):

```powershell
.\scripts\build-boot-matrix.ps1
# fill build\boot-matrix-manual-results.csv
.\scripts\evaluate-boot-matrix.ps1
```

## Smoke Tests

```bash
bash scripts/smoke-build.sh
python -m unittest tests/test_asset_converter.py
```

## 2D Assets for VDP1

The `tools/convert_indexed8.py` converter generates:

- `.tex8` and `.pal` for inspection/legacy binary.
- `.h` and `.c` with pixels, palette and metadata to compile in the example.

Example used by the repository:

```bash
python tools/convert_indexed8.py \
  --input assets/sonic_head.png \
  --resize 128 96 \
  --out-prefix build/generated/text_sprite/sonic_head
```

The `text_sprite` example reduces `sonic_head.png` to fit in the simple sprite path of VDP1.
For the `text_sprite` example, `make` automatically calls this generation before compiling the binary.

## 3D Model Pipeline for VDP1

Conventional UV-mapped models cannot go straight to the VDP1: it draws
distorted sprites from four corners with no per-vertex UVs, no depth buffer
and no clipper. `tools/import_model.py` therefore bakes every source face
offline into a canonical rectangular texture:

```text
OBJ + MTL + PNG
      |
      | tools/import_model.py
      v
generated C/H model
      |
      | upload once (sat_model_upload_textures)
      v
LibSaturn textured mesh (`sat_scene_submit_instance` canonical path)
      |
      v
VDP1 distorted sprites
```

Why bake offline: source UVs are importer input, not runtime state. The
generated asset holds vertices in Saturn fixed point, quad indices in
LibSaturn A/B/C/D order, face-to-texture indices into a deduplicated texture
set, and shared indexed palette(s) -- no OBJ/MTL/PNG parsing on the Saturn,
no heap, no per-frame uploads.

VDP1 texture constraints (manual 5.1/6.6): width 8..504 in multiples of 8,
height 1..255. The importer resamples the same UV domain across the legal
aligned width (never pads with unused columns), enforces the maxima and CLI
limits (`--max-texture-width/height`, `--texture-scale`), and fails with a
face/material diagnostic when a face cannot be represented. Baked faces are
deduplicated after palette mapping (width, height, indexed bytes, palette
identity, flags), and one shared <=256-entry palette is built
deterministically (index 0 reserved for transparency when needed; fully
opaque models use `SAT_SPRITE_FLAG_OPAQUE`). Triangles travel as degenerate
quads with the duplicated UV matching the duplicated corner.

```bash
python tools/import_model.py \
  --input examples/basic_3d_texture/assets/sonic.obj \
  --out-prefix build/generated/basic_3d_texture/sonic_model \
  --symbol sonic_model \
  --palette-index 1
make EXAMPLE=basic_3d_texture
.\run-example.ps1 basic_3d_texture -Emulator mednafen -BiosProfile auto
```

Viewer controls: LEFT/RIGHT orbit yaw, UP/DOWN pitch (clamped), L/R zoom
out/in, A toggle auto-orbit, B reset, START toggle HUD. The camera orbits an
immutable model (center from `sat_model_compute_center`); textures upload
once at startup and frames draw with culling + painter sorting.

## Animated 3D Model Pipeline (GLB)

Skeletal animation never runs on the Saturn. `tools/import_model.py`
(`--target saturn`) parses skinned GLB on the host, evaluates joint TRS
channels at the clip's sample rate, and bakes every runtime frame as a pose:

```text
GLB skin + animation
      |
      | tools/import_model.py --target saturn
      | (gltf parse -> skinning eval -> QEM simplify -> pose bake)
      v
generated C/H animated model (shared indices/textures + pose stream)
      |
      | textures upload once; per frame:
      | sat_anim_advance + sat_anim_decode -> caller vertices
      v
VDP1 distorted sprites (same canonical scene path)
```

The generated asset quantizes positions to int16 per axis around a
scale/bias (decoder: `pos_fx16 = bias + scale*q/32767`), reuses the same
baked-face texture dedup as the static path, and keeps the simplified mesh
inside explicit quality gates (animated surface error, silhouette chamfer,
IoU, no facet flipped past 90 degrees, no crack edges) and Saturn budgets
(VDP1 commands incl. HUD reserve, VRAM, <=256 KiB pose stream). Any gate
breach is a hard FAIL with numbers -- the importer never emits a silently
degraded asset.

Simplification uses worst-pose QEM (MAX over baked poses) with UV-seam and
material locks, boundary quadrics on borders/seams, crease penalties and an
exact-duplicate weld pre-pass. Collapses move whole position groups: every
split copy of a surface point (flat normals, UV seams, palette-swatch UVs)
moves together, so the surface never tears open. Each face stays within a
preset angle of its source orientation in every pose, so backface culling
never opens holes. Only subset collapses are allowed, so surviving vertices
keep exact source UVs/joints/weights. `--simplify auto`
searches under `--quality <preset>` within the profile caps;
`--simplify off|N` force the triangle count.

```bash
python tools/import_model.py \
  --input examples/basic_3d_animation/assets/male_basic_walk_30_frames_loop.glb \
  --target saturn \
  --out-prefix build/generated/basic_3d_animation/male_walk \
  --symbol male_walk --palette-index 1 \
  --simplify auto --quality balanced \
  --animation all --animation-fps source
make EXAMPLE=basic_3d_animation
.\run-example.ps1 basic_3d_animation -Emulator mednafen -BiosProfile auto
```

The `basic_3d_animation` GLB is a local-only acceptance fixture (see
`examples/basic_3d_animation/assets/LICENSE.txt`); it is not committed, and
the build fails with the exact missing path when it is absent. Viewer
controls: LEFT/RIGHT orbit yaw, UP/DOWN pitch, L/R zoom, A pause/resume
animation, B reset camera+animation, C auto-orbit, START toggle HUD.

## Runtime validation

The canonical automated runtime validation is the modified Ymir harness. It
boots the Saturn BIOS for the configured warm-up, injects the built BIN, and
checks the running program through the harness probe. A real BIOS disc boot is
not inferred from that direct-injection path.

To switch BIOS/region in the example launcher without editing Mednafen's global config:

```powershell
.\run-example.ps1 mvp_2d_scene -Emulator mednafen -BiosProfile na
.\run-example.ps1 mvp_2d_scene -Emulator mednafen -BiosProfile jp
.\run-example.ps1 mvp_2d_scene -Emulator mednafen -BiosProfile eu
.\run-example.ps1 mvp_2d_scene -Emulator mednafen -BiosProfile auto
.\run-example.ps1 mvp_2d_scene -Emulator mednafen -IpTemplate yaul
.\run-example.ps1 mvp_2d_scene -Emulator mednafen -IpTemplate sbl
.\run-example.ps1 red_square -Emulator mednafen -BiosProfile auto
```

## Note About IP.BIN

The project generates `ip.bin` in `make` from a selectable boot template via `IP_TEMPLATE_KIND`:
- `yaul` (default): `assets/boot/ip_yaul_template.bin`
- `sbl`: `assets/boot/ip_sbl_template.bin`

In both cases, the build preserves the original text/boot template and only overwrites `1ST_READ` (`0x0F0/0x0F4`).
Sensitive boot blocks (`0x0100..0x05FF`) and code object area (`0x0E00..0x7FFF`) remain identical to the selected template.
For boot compatibility, the ISO writes the payload as `0.BIN` (primary) and `1ST_READ.BIN` (alias).
In Mednafen, prefer opening `build/mvp.cue` instead of `build/mvp.iso`.
Before public distribution, perform legal/licensing review of boot assets according to your release policy.

## Physics and Collisions

The collision toolkit is deliberately small, deterministic and malloc-free:

| Module | Purpose |
|---|---|
| `collide2d.h` / `collide2d_logic.hpp` | 2D boxes, circles, contacts, rays and sweeps |
| `spatial.h` / `spatial_logic.hpp` | Caller-owned uniform-grid broad phase and pair queries |
| `physics.h` / `physics_logic.hpp` | Fixed-step clock and arcade 2D body/tile movement |
| `collide3d.h` / `collide3d_logic.hpp` | 3D spheres, AABBs, planes, quads and mesh rays |
| `collide3d_api.cpp` | 3D body stepping, mesh contacts and C ABI wrappers |

All positions and velocities are 16.16 fixed point. 2D uses y-down screen/world
pixels; 3D uses y-up. Overlap is strict: touching an edge is not collision.
This lets a body rest on a floor while `GROUNDED` is set, and preserves the
Pac-Man rule `abs(delta) < 6`. Squares and dots are formed in int64 at raw
2^32 scale. Distance tests compare those squares directly; square roots are
reserved for requested depths and hit points. Raycasts and sweeps use the
`div_s64_s32` divide path, which maps to the SH-2 DIVU hardware.

The broad phase takes storage from the caller. Choose a cell size near the
largest object; the normal cost is O(n + candidate pairs), while the documented
worst case of every object in one cell is O(n²). Out-of-range coordinates clamp
to edge cells, and query stamps deduplicate a candidate in constant time.

A minimal platformer loop is:

```c
sat_step_clock_t clock;
sat_step_clock_init(&clock);
for (;;) {
    uint16_t steps = sat_step_clock_steps(&clock, 4);
    while (steps--) {
        sat_body2_step(&player, &params);
        sat_body2_move_tiles(&player, &grid, tile_at, user);
    }
    draw_player(&player);
}
```

The `physics_2d` and `physics_3d` examples exercise these APIs with static
storage, broad-phase statistics and a ray-pick HUD.
