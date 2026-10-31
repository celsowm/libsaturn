# Controlled PS1 and Saturn Dino comparison

This study compares the current 640x448 Saturn renderer with the original
PS1 program at matched Walk poses and camera projection. It supersedes the
earlier independent-phase profile images. It does not modify the production
demo. Its restored application SHA-256 is
`702f795634f76f3ef1a059b58ae36758a6ff5dbed6fbaad48babb284a33bc5c3`.

## Results

The controlled profile views have closely corresponding outlines and limb
positions. The current Saturn mesh retains the original 1376 moving points
and 2724 source triangles. The previous comparison's different perspective
and pose exaggerated apparent anatomical differences.

The Saturn capture has finer vertical edges and more readable small texture
features at this framing. The original PS1 render has stronger highlights,
darker shaded regions and clearer lighting across the shoulder, thigh and
neck. The Saturn material looks more uniformly brown and flatter. Higher
framebuffer resolution has not reproduced the original lighting. The PS1
background also contributes to its presentation; it remains visible here.
The front view still differs in the visible upper tail and mouth appearance.
Those are renderer/material observations; the matched profile does not prove
that every visibility and shading detail is equivalent.

| Observation | Original PS1 | Current Saturn |
| --- | --- | --- |
| Source raster | 640x240 | 640x448, double-density interlace |
| Walk cycle | 0.95512 s, measured | 0.95238 s, configured; about 0.951 s estimated from field samples |
| Program updates in the timing samples | About 33.50/s | About 25.14/s |
| Lighting | Stronger light/shadow variation and volume | Flatter, less contrast |
| Detail in frozen captures | Coarser vertical stepping | Finer edges and small features |

The configured walking period is 0.29% shorter on Saturn. This is a small
cadence difference. Program update rate is a separate quantity: the Saturn
sample completed 88 render loops over 210 NTSC fields in the unmodified
front view. The PS1 rate comes from its 32 guest updates per measured walk
cycle. These are emulator observations under the stated scenes, not physical
hardware benchmarks or measurements of monitor presentation. Frozen pose
images cannot assess interlace combing, motion judder or displayed frame pacing.

The next visual priorities supported by this comparison are stronger
lighting/shading, material contrast and smoother scene updates. Increasing
polygon count is not supported by the outline comparison.

## Capture controls

PCSX-Redux used its interpreter, software GPU and OpenBIOS to run the original
local DINO.EXE with the complete multitrack CD image. The visual-study Lua
script holds recovered Walk poses 0, 8, 16 and 24 after the original VDF
evaluation and before rendering. Its pose data comes from the verified
integer replay in `tools/bake_ps1_dino_walk.py`, whose 64 captured deformation
updates reproduce the original coordinates exactly. Independent jaw/roar
channels are excluded on both sides. This is a controlled render of original
poses, not an untouched gameplay capture.

The PS1 texture data, lighting routines, renderer and executable instructions
are retained. Both PS1 framebuffer regions are pixel-identical in all eight
saved captures, verifying that the held pose has settled across buffering.
The screenshots therefore avoid the incomplete upper-buffer issue in the
earlier moving capture.

The Ymir harness renders the existing Saturn asset and renderer with stock
RAM and NTSC timing. The temporary
[`dino-fair-capture.patch`](../harness/studies/dino-fair-capture.patch) holds the
same four poses, disables the entrance and red sweep through
`DINO_BENCH_PAUSED=1`, and sets the comparison camera. The patch was removed
and the normal demo rebuilt after capture. The controlled application hash is
`410335409ed29da760f7fc41a039c68300a68aa6ad17454a4f4719aca215c9c8`.

Camera matching uses the original GTE rotation, translation, H and screen
offset registers observed at its projection instructions. The PS1 uses a
different coordinate convention. The study uses PS1 yaw 270 for the profile
and yaw 0 for the front, then reflects the complete PS1 viewport horizontally
to correspond to the Saturn views. Reflection preserves every source pixel.
The observed horizontal/depth matrix scale is 4098/4096, vertical scale 1/2,
projection distance 1000, and depth translation 52024. The profile's X
translation is 6002; the front's is zero.

The recovered source-to-Saturn mapping is
`world = original * [1,-1,1] / 4000 + [0,1.8390055,1.5776986]`.
The Saturn comparison projection uses `Pxx=3.125` and approximately
`Pyy=4.164633`. Comparing the actual captured Saturn camera matrices with
the GTE camera across all 1376 points and all 32 Walk poses gives a maximum
ideal projection deviation below 0.021 display pixels. This checks camera
equivalence before raster rounding and pose quantization; it is not an image
error score or a claim that the two rasterizers draw identical pixels.

Complete viewports are mapped to 640x480 with nearest sampling. There is no
blur, sharpening, color correction, independent silhouette resizing or
background removal. Detail panels use the same screen rectangles on both
sides and enlarge them three times with nearest sampling.

## Local artifacts and reproduction

Artifacts are in `harness/build/dino_fair_comparison/`:

- `comparison.png`: profile and front at pose 0, PS1 left and Saturn right.
- `profile-four-poses.png` and `front-four-poses.png`: all controlled poses.
- `details.png`: matching head and torso screen crops.
- `comparison.json`: hashes, buffer checks, camera checks and timing results.
- `ps1-projection.jsonl`, `ps1.jsonl`: original camera and pose observations.
- `capture.map`, `saturn-command.json`, `saturn*.bin`: controlled build evidence.
- `cadence-samples.json`: eight fresh samples of the unmodified application.

A persistent copy of the checks, hashes and measurements is retained in
`harness/studies/dino-fair-comparison.json`.

The PS1 capture entry point is `harness/scripts/ps1_dino_fair.lua`. Before
running it, generate its ignored `walk-fixed.bin` by calling `decode_capture`
from `tools/bake_ps1_dino_walk.py`, placing the 32 returned Walk poses into a
little-endian int16 array shaped `(32,1376,4)`, with the fourth component zero.
The original archive and sources stay local; the extracted scratch disc is
removed after the study.

For the Saturn capture, apply the study patch, build with
`DINO_BENCH_PAUSED=1`, and resolve diagnostic addresses from that build's ELF.
The recorded probe command captures each pose after 35 completed render loops
within its 40-loop slot, plus four settling fields. Save the map and hashes
before restoring and rebuilding the production source. Capture the profile
camera matrix at field 400 of the same controlled build.

After the raw captures and timing samples exist, assemble and verify them:

```sh
python harness/scripts/dino_fair_compare.py harness/build/dino_fair_comparison
```

The assembler rejects unsettled PS1 buffers, unexpected dimensions, missing
pose combinations, camera mismatch and an incorrect Saturn animation clock
rate. The native Mednafen launcher was also executed with the restored normal
build and explicit `-nothrottle 0`.
