# VDP2 Parallax Lab

A four-layer hand-painted coast drawn by VDP2 alone. The SH-2 updates four
scroll registers per field; every background pixel comes from VDP2 VRAM.

The art is a 512x256 reduction of the four layers in
`vdp2_parallax_lab_with_pngs.zip` (sky, mountains, sea/cliffs, foreground).
The source is not seamless horizontally, so the camera pans 176 dots and
back instead of wrapping over a seam. The pan uses a trapezoidal velocity
profile and 16.16 fixed point: short ease-in/out ramps and a constant middle,
so NBG0/NBG1 keep a regular cadence instead of creeping at the ends. Alpha is
thresholded into the VDP2 transparent dot code; the sky is opaque.

```powershell
.\build-example.ps1 vdp2_parallax_lab
.\run-example.ps1 vdp2_parallax_lab
.\harness\run-vdp2-parallax-lab.ps1 -Bios bios\saturn_bios_us.bin
.\harness\run-vdp2-parallax-lab.ps1 -Bios bios\saturn_bios_us.bin -Long
.\harness\run-vdp2-parallax-lab.ps1 -Bios bios\saturn_bios_eu.bin -Pal
```

| Input | Action |
| --- | --- |
| A | Pause/resume the pan |
| START | Exit |

## VDP2 layout

| Layer | Content | Format | VRAM | Palette |
| --- | --- | --- | --- | --- |
| NBG0 | foreground palms, rocks, flowers | 512x256 8bpp bitmap | `0x00000` | 256 colours, bank 0 |
| NBG1 | sea, cliffs, lighthouse | 512x256 8bpp bitmap | `0x20000` | 256 colours, bank 1 |
| NBG2 | mountains and town | 4bpp cell map, two-word names | map `0x40000`, chars `0x48000` | 16 colours, block 4 |
| NBG3 | sky and clouds | 8bpp cell map, two-word names | map `0x44000`, chars `0x60000` | 256 colours, bank 2 |

Two-word pattern names let each 8x8 tile carry its own 15-bit character
number, so the complete 512x256 image fits in one plane with no repeated
tile strip. The pan factors are 8/24/52/100 percent, from the source lab.

`tools/convert_vdp2_parallax_lab.py` quantizes the four PNGs and emits the
big-endian bitmap words, cell data and pattern-name maps. It reserves dot 0
for the transparent cutouts and uses the tool's own 16-colour mountain
palette; no runtime image decoding or heap allocation is involved. The sea
RGB is lightly softened before quantization because its one-pixel wave
highlights alias badly when a slow layer scrolls.

## Evidence and limits

The Ymir harness checks the four public layer configurations, the scroll
model, the VDP2 register geometry, the uploaded maps/chars and the live
picture in NTSC and PAL. The default run is 4 tests plus a skipped `-Long`
test; the Mednafen 1.32.1 cross-check captured the live pan at frames 87 and
497. The art is a still-image parallax demonstration, not a game scene, and
the source layers were not authored for Saturn. All timings are
emulator-side; physical hardware remains **PENDING**.
