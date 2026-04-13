# Acceptance Checklist

This document is the reference for the manual acceptance flow for the MVP.
Use the script `scripts/check-acceptance.ps1` to execute the protocol and generate a report.

## Prerequisites

- Windows PowerShell.
- MSYS2 with UCRT64 profile.
- SH2 toolchain available in the MSYS2 environment PATH (`sh2eb-elf-gcc`).
- `mkisofs`, `genisoimage` or `xorrisofs` available in the MSYS2 environment.
- Mednafen installed (`scripts/download-emulators.ps1`) and/or Kronos installed manually.
- Mednafen BIOS configured as `sega_101.bin` (JP) and `mpr-17933.bin` (US/EU).

## Checklist Execution

```powershell
.\scripts\check-acceptance.ps1 -Emulator both
```

Options:

- `-Emulator mednafen|kronos|both`
- `-IsoPath <path>`
- `-ReportPath <path>`
- `-Msys2Root <path>`

## What the script does

1. Validates SH2 tool, `make` and `mkisofs`/`genisoimage`/`xorrisofs` in MSYS2.
2. Executes clean build (`make clean && make all`) to generate ISO.
3. Shows commands to open the image in the selected emulator (`.cue` in Mednafen when available).
4. Collects manual confirmation for acceptance criteria.
5. Saves report in `build/acceptance-report.txt` (or custom path).

## Acceptance Criteria

1. Disk image starts and enters the main loop.
2. Stable 2D sprite rendering for 1800 frames.
3. Input without ghost presses for 5 minutes.

## Result and Exit Codes

- `0`: PASS (all criteria approved).
- `2`: FAIL (at least one criterion failed).
- `3`: PARTIAL (criteria skipped, for example Kronos absent).