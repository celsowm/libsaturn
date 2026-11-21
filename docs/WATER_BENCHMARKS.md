# Water Lab benchmarks

## Reproduction

Use `./harness/run-water-lab.sh` and
`python3 harness/tests/test_water_lab.py`. The script builds both disc
images and collects screenshots, command bytes and big-endian telemetry
in `build/water_lab_evidence/`. The scene is procedural and seedless.
The matched-mode script pauses the simulation for dry, opaque, mesh, RGB half
and underwater captures. It also exercises 256- and 512-face workloads
and a low camera looking through the water surface.

The measured workspace was based on `10ad3a1b` plus the Water Lab
implementation. Ymir core was `5c571f68`, with a synchronous software
renderer and no RAM cartridge. The output was 320×224 non-interlaced.
Mednafen 1.32.1 loaded the playable disc and rendered the standalone
probe's indexed composition with both halves and the HUD marker visible.
All automated pixel assertions and timings below are from Ymir. Physical
Saturn acceptance is pending.

| Artifact | SHA-256 |
| --- | --- |
| Playable BIN | `ded9c6a6462abd27aab3630564e9b5217fbffb35d67a795b58fbac3f2193788a` |
| Standalone probe BIN | `0d4b1215293784dbb5f775971e39b30041ecd92159a35bedfa0cf7774aeeb19a` |

The linker maps are `build/water_lab.map` and `build/water_lab_probe.map`.
The playable ELF has 261,600 bytes of BSS. The three painter arrays take
112,640 bytes combined and the compositor workspace occupies about 117 KiB.
The probe uses 35 KiB for RGB staging and
17.5 KiB for packed indexed output and 64 KiB for the conversion lookup
table. Its VDP2 allocations cover two
128 KiB indexed bitmaps, one 128 KiB scratch reservation, and one CRAM
palette bank. The backdrop uses the final VRAM word.

## Rippling Water frame rate

Ymir ran the default 128-face dry workload and 8×8 rippling grid for 540
fields. The rates count changed fields in the 160×65 water crop over
fields 99–539. The HUD run toggled diagnostics at startup.

| Mode | Original | Shared vertices and pipelining | Sine and pacing follow-up |
| --- | ---: | ---: | ---: |
| NTSC | 20.0 FPS | 29.5 FPS | 29.9 FPS |
| NTSC with diagnostics | 17.1 FPS | 27.5 FPS | 29.0 FPS |
| PAL | 16.7 FPS | 29.3 FPS | 31.2 FPS |

The grid, ripple ribbons and mosaic rows share projected vertices through the
indexed scene instance API. Whole-degree sine samples reuse the SDK's
fixed-point results. Native frames build in the free VDP1 command bank while
the previous draw and swap finish. At the end of the NTSC run, preparation
fell from 41.2 ms to 27.8 ms. The common sequence of 180 water-crop hashes
matched before and after the change. The updated runs had no VDP1 draw
timeouts. The NTSC run without diagnostics logged 31 draws still pending
at VBlank-IN. The counter samples the start of VBlank. A draw can still
finish and request its swap before VBlank-OUT.

### NTSC frame pacing

The follow-up uses a 32-bit quotient and an exact remainder correction in the
fixed-point sine calculation.
It launches the NTSC rippling draw at VBlank for the 128-face workload with
diagnostics off and a grid of 8×8 or smaller. This gives VDP1 a full field
before the next swap window. The diagnostics run exceeded two fields when
using the same wait. Diagnostics, stress workloads and PAL use the free-running
submission path.

The default NTSC run presented a new water crop every two fields through
the measurement window, with zero late swaps or draw timeouts in 540 fields.
The preceding build produced seven three-field gaps in the same window.
All 266 comparable water-crop hashes matched. At the final sampled frame,
scene preparation fell from 27.8 ms to 26.0 ms. The free-running runs
recorded 26 late swaps with diagnostics and 31 in PAL, with zero draw
timeouts in both.

## Native modes

| Ymir sample | World commands | Preparation / flush |
| --- | ---: | ---: |
| NTSC mesh, tick 67, 8×8 | 192 | 2,961 / 625 ticks |
| NTSC RGB half, tick 67, 8×8 | 192 | 2,963 / 628 ticks |
| PAL dry, tick 140 | 128 | 1,929 / 439 ticks |
| NTSC rippling water, tick 52, 8×8 | 290 | 8,584 / 1,063 ticks |

The native commands in VRAM include 66 mesh polygons at CMDPMOD
`0x01C0` and 66 half-transparency polygons at `0x00C3`. Each count
includes two glass panels. Screen checks verify the checkerboard on
neighboring pixels, a 5-bit average over an RGB floor tile, and replacement
over an indexed mosaic tile. The low-view capture exercises underwater
geometry with near clipping and records a textured crossing on its
unchanged texture path.

In the mixed NTSC run the display-boundary counters recorded 198 list
changes in 428 measured fields, about 28 changes per second. The PAL run
opened with the rippling mode before switching to dry and measured 140
changes in 178 fields, about 39 per second. The NTSC rippling run with
diagnostics enabled partway through measured 52 changes in 168 fields,
about 19 per second. Native preparation and flush timings are endpoint
samples. The IRQ counters track display-boundary list changes and the
captures independently check visible pixels.

## Standalone transfer and composition probe

| Stage | Worst observed ticks | Approximate Ymir time |
| --- | ---: | ---: |
| VDP1 start to END and CEF observed | 264 | 1.3 ms |
| CPU RGB transfer, 35 KiB with row stride | 4,349 | 20.7 ms |
| SCU RGB transfer, two legs, 35 KiB per leg | 777 | 3.7 ms |
| Indexed conversion and upload, previous CPU path | 5,999 | 28.5 ms |
| Indexed conversion and upload, Work RAM packing and SCU upload | 3,719 | 17.7 ms |
| Indexed conversion and upload, RGB555 lookup and SCU upload | 2,074 | 9.9 ms |
| SCU readback alone, 35 KiB from VDP1 to Work RAM | 459 | 2.2 ms |

The transfer maxima include setup and synchronous waits. The raw clocks in
the two NTSC probe runs were 210,540 and 210,600 ticks/s. The new conversion
counts 17.5 KiB of indexed output. The previous path read RGB from VDP2 scratch
and wrote the inactive bitmap with CPU word stores. Converting from Work RAM
and batching the writes reduced the worst sample by about 38%.

The original 3,600-field probe confirmed 421 swaps, 420 drawing-bank reuses and zero
bank or transfer verification failures. The three injected delays each held
the previous visually tagged pair through a field. Captures also confirm
the new matched pair, 2× alignment, transparent zero, bit-15-clear RGB,
covered RGB black, and the image of the HUD marker outside the effect.

The original instrumented probe presented about 7 pairs per nominal second.
The faster probe does the CPU and whole-bank comparison on startup and
at intervals. It verifies both coverage boundaries and the guards each
frame and reads 35 KiB from the completed drawing bank with SCU DMA.
The harness samples the RGB of two neighboring pixels in every field
and checks their joint sequence. The unblended half identifies the scene,
and the blended half identifies its effect tag against that scene color.
The test permits held fields and checks the deliberate effect-layer
release and reacquisition at pair 32.

| Ymir mode | Captured fields | Matched pairs | Pairs/s from first visible pair | Bank or transfer failures |
| --- | ---: | ---: | ---: | ---: |
| NTSC 60 Hz | 8,000 | 3,823 | 28.75 | 0 |
| PAL 50 Hz | 800 | 354 | 22.74 | 0 |

The NTSC run exceeded the 3,600-pair presentation qualification. The PAL
run used a shorter window to measure cadence separately. Physical Saturn
testing remains open.

## Scene compositor

The matched captures preserve foreground samples on the pillar, island,
stair wall and front rim while changing the water-covered floor. The HUD
bands are byte-for-byte identical where their text is shared. The camera
script exercises the near plane, follows the player, and returns to the
orbit with moving actors and particles. The renderer drains the effect
before switching back to the native pool.

| Ymir scene sample | RGB readback / indexed upload | Readback / conversion and upload |
| --- | ---: | ---: |
| Full transfer, follow camera | 35,840 / 17,920 bytes | 465 / 1,727 ticks |
| Bounded transfer, same camera sequence | 23,040 / 11,520 bytes | 298 / 1,158 ticks |

The row bounds reduced transfer bytes by 36% in this sample. The same
full and bounded samples took 1,308 and 1,309 ticks from draw submission to
confirmed END. Fewer DMA bytes also reduce CPU conversion and list setup.
The two bitmaps still occupy 256 KiB of VDP2 VRAM and one CRAM bank.

| Moving orbit, fields 380–534 | Changed composed fields | Changes/s |
| --- | ---: | ---: |
| NTSC native RGB half, 8×8 | 77 / 155 | 29.8 |
| NTSC multipass, 8×8 | 101 / 155 | 39.1 |
| NTSC multipass, 4×4 | 123 / 155 | 47.6 |
| PAL multipass, 8×8 | 78 / 155 | 25.2 |

These observations count changes to the 160×70 water-area crop during
movement. The screenshots at paused ticks separately check image parity
where the layers should agree. The 4×4 trial produced coarser occlusion
around the front ledge, so the default stays at 8×8. DMA and VDP1 ownership
stay on the Master during the effect.

To reproduce the moving window, run the Ymir wrapper for 540 fields with
`--pad-script harness/scripts/water_lab_fx.pad --video-observations fields.csv
--video-crop 80:90:160:70`. Count rows with nonzero `changed_pixels`
from field 380 through 534. Add `--pal` for the 50 Hz run. For the native
match, omit pad entries 110, 116, 130 and 150 so the sequence stays in RGB
half. In the 4×4 trial, insert two Z+B presses between fields 26 and 62 to
cycle the grid from 8×8 through 16×16 to 4×4.

## Water Lake

`examples/water_lake` replays one 390-tick camera route over a 60×60-unit lake
in two ways and records each 30-tick stretch (`harness/run-water-lake.sh`; run
`python3 harness/tests/test_water_lake.py` to check it). **NAIVE** models the
water as four large mesh quads (a 6×6 grid of textured tiles for the textured
variant) and sorts everything by depth, as a level would. **FULL** draws the
camera-centred water plane (4 rings of 4×4 cells), the level prepared against
the water region in two passes with a vertex block per side, the actor cut at
the waterline, and the underwater view. Both draw the same ~300-face level,
bridge and a 96-face sphere. The textured variants use an 8×8 LUT4 ripple
texture with the water colour as the fallback for a face behind the near plane.

Ymir only (immediate division, no SH-2 cache emulation): not a hardware
measurement. Preparation is the CPU time to build the frame; flush is the
scene flush. Face counts follow the route by tick and are pinned; times are
not. The `tex` columns count clipped faces in the textured runs.

| Section (ticks) | Route | NAIVE prep / flush ms | FULL prep / flush ms | NAIVE clipped | FULL clipped | NAIVE tex clipped | FULL tex clipped |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 (0–29) | orbit | 13.8 / 11.6 | 20.4 / 12.6 | 16 | 16 | 17 | 17 |
| 1 (30–59) | orbit | 13.8 / 11.4 | 19.8 / 12.5 | 17 | 17 | 18 | 18 |
| 2 (60–89) | orbit | 13.9 / 8.6 | 24.8 / 11.2 | 6 | 6 | 6 | 6 |
| 3 (90–119) | approach | 13.6 / 12.4 | 26.0 / 15.1 | 32 | 34 | 33 | 34 |
| 4 (120–149) | approach | 10.8 / 10.8 | 21.7 / 13.7 | 40 | 48 | 45 | 48 |
| 5 (150–179) | skim over water | 7.7 / 8.4 | 17.8 / 11.4 | 40 | 49 | 44 | 49 |
| 6 (180–209) | to the bridge | 10.4 / 9.7 | 21.2 / 14.4 | 32 | 43 | 40 | 44 |
| 7 (210–239) | on the bridge | 12.5 / 20.3 | 24.9 / 27.2 | 103 | 122 | 110 | 122 |
| 8 (240–269) | on the bridge | 7.3 / 9.1 | 17.7 / 11.4 | 38 | 46 | 42 | 46 |
| 9 (270–299) | to the water | 11.3 / 21.5 | 25.0 / 18.9 | 53 | 63 | 57 | 65 |
| 10 (300–329) | dive | 9.7 / 18.7 | 21.6 / 22.1 | 96 | 114 | 102 | 116 |
| 11 (330–359) | under water | 11.7 / 25.8 | 25.4 / 35.6 | 131 | 168 | 142 | 169 |
| 12 (360–389) | surfacing | 11.2 / 21.6 | 25.0 / 17.1 | 50 | 60 | 55 | 62 |

| Stage (last frame) | NAIVE ms | FULL ms |
| --- | ---: | ---: |
| Level submit | 8.1 | 9.5 |
| Water (build for FULL) | 0.2 | 1.7 |
| Water submit | 0.0 | 1.7 |
| Actor split | 0.0 | 1.1 |
| Actor submit | 2.9 | 2.8 |

FULL costs about 6–12 ms more preparation per frame than NAIVE: the plane is 52
faces, the actor is cut, and the level's two instances now project a vertex
block each (13.2 ms before the blocks, 9.5 ms after; NAIVE's single instance is
8.1 ms). The numbers do **not** show the plane reducing clipped or fallback
faces, for flat or for textured water: no run skips a face, every clipped face
is drawn as a fallback, and the plane clips as many or slightly more. For
textured water the plane keeps finer cells near the camera, so less of the
texture turns into the flat fallback colour, but that is not measured in
pixels. What FULL buys is correctness, shown by the matched captures at ticks
120, 240 and 330 that the harness pins: the bed no longer paints over the
water, the waterline cuts the actor and the posts, wet geometry is tinted by
depth, and the camera under the surface gets the colour-offset fade instead of
an untreated view.

[Water Lab](../examples/water_lab/README.md) · [Compositor probe](FX_COMPOSITOR.md) · [Implementation plan](WATER_LAB_PLAN.md)
