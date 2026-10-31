# VDP2 Video Modes (E09)

Shows the same test pattern through every TV screen mode the library
supports: 320/352/640/704 dots wide, 224/240/256 lines, non-interlaced or
single-density interlaced. The mode changes by a full `sat_shutdown()` plus
`sat_init()`, because the VDP2 manual requires the BIOS horizontal-resolution
routine and a reset when leaving dedicated-monitor modes.

The pattern is an NBG0 cell map with a one-dot grid, colour bars, a one-dot
checker and a diagonal, plus a VDP1 one-dot border and corner ticks. The HUD
reports the mode, width/height, interlace flag, NTSC/PAL and CRAM mode;
telemetry records the TVMD value the library wrote.

```powershell
.\build-example.ps1 vdp2_video_modes
.\run-example.ps1 vdp2_video_modes
.\harness\run-vdp2-video-modes.ps1 -Bios bios\saturn_bios_us.bin
.\harness\run-vdp2-video-modes.ps1 -Bios bios\saturn_bios_eu.bin -Pal
```

| Input | Action |
| --- | --- |
| A | Next mode |
| C | Previous mode |
| START | Exit |

The mode advances automatically every 60 fields so a harness capture reaches
several modes without a pad script.

## Supported matrix

| Width | 224 | 240 | 256 (PAL only) |
| --- | --- | --- | --- |
| 320 | non-interlaced, single | non-interlaced, single | non-interlaced, single |
| 352 | non-interlaced, single | non-interlaced, single | non-interlaced, single |
| 640 | non-interlaced, single | non-interlaced, single | non-interlaced, single |
| 704 | non-interlaced, single | non-interlaced, single | non-interlaced, single |

Single-density interlace keeps one logical field (224/240/256 lines) and
produces a picture twice as tall (448/480/512). The VBlank interrupt still
fires once per field, so `sat_video_refresh_hz()` remains 60/50 and a full
interlaced frame spans two VBlanks.

Double-density interlace at these heights is refused with
`SAT_ERR_UNSUPPORTED`: it needs a different VDP1 picture in each field, which
only the 640/704x448 mode provides (see `dino_demo`). A 256-line picture is refused on an NTSC console.

## Evidence and limits

`harness/run-vdp2-video-modes.ps1` checks the mode matrix, the TVMD
HRESO/VRESO/LSMD bits, the refusal codes, NBG0 plane-A words in VDP2 VRAM,
and each supported picture's size. The NTSC run covers the five 224/240-line
modes; the `-Pal` run covers all eight, including 704x256 and 640x512. Both
pass on the pinned Ymir revision. The same builds were also launched in
Mednafen 1.32.1 through `run-example.ps1`; window captures show the HUD and
the 352x240, 320x480, 640x480 and 320x256 modes. That is a manual cross-check,
not the full matrix. Physical hardware is still **PENDING** in
`docs/HARDWARE_ACCEPTANCE_CHECKLIST.md`; emulator output is not hardware
certification.
