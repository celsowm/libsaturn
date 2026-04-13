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
- Separate texture demo in `examples/text_sprite`.
- 8-bit indexed asset converter in `tools/convert_indexed8.py`, with output in `C/H` for embedding in build.

## Main Structure

- `include/saturn/saturn.h`: Public C API.
- `src/core`: Core implementation, startup and linker script.
- `src/hal`: Direct hardware register access.
- `examples/mvp_2d_scene`: MVP validation demo.
- `examples/red_square`: Red square moved by D-pad.
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

This creates:

- `emulators/mednafen/run-mednafen.ps1`
- `emulators/kronos/run-kronos.ps1`

For Mednafen, keep JP BIOS in `firmware/sega_101.bin` and US/EU BIOS in `firmware/mpr-17933.bin`.
The launcher attempts to automatically copy from `bios/saturn_bios_jp.bin` and `bios/saturn_bios_us.bin` (or `bios/saturn_bios_eu.bin`).
By default, the Mednafen launcher uses `region_autodetect=1` with fallback `region_default=na` and forces `ss.h_overscan=0` / `ss.videoip=0` to avoid cutting/artifacts on the license screen.

Kronos follows manual installation in `emulators/kronos/kronos.exe`.

## Acceptance Checklist

Run the guided checklist:

```powershell
.\scripts\check-acceptance.ps1 -Emulator both
```

Output:

- `build/acceptance-report.txt`

Full protocol: `docs/acceptance.md`.

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

## Target Emulators

- Kronos (debug).
- Mednafen (timing/compatibility).

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