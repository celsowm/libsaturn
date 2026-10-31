# Dino PS1 cadence reference

The active Saturn geometry preserves the original source OBJ, without decimation.
The archived reconstruction authored its clips over 40 frames at 30 Hz:
1.333 seconds. The active asset now replaces Walk with the recovered original
PS1 deformation on the original surface; Roar remains reconstructed.
The calibrated GLB timeline is still 1.333 seconds. The source mesh, texture
pages and original Walk come from Sony's PlayStation Developer's Demo CD.
See the example's asset notice and the recovery section below.

## Original demo observation

The local disc contains `DINO/DINO.EXE`, `DINOBASE.TMD` and `DINOBASE.VDF`.
The observed executable has SHA-256
`8bb65e609a3605656a78930776dd324028e19dca952b16d6d94bce8649d7b824`.
The disc and executable are private local inputs, not repository artifacts.
The downloaded CUE referred to `DEVELOPER'S DEMO.BIN`; its local working copy
was corrected to the actual BIN filename before running the emulator.

PCSX-Redux ran with its interpreter, software GPU, OpenBIOS and no controller
input. A Vsync observer recorded the original program's state after boot:

- `0x80044438`: main-loop frame counter, incremented at `0x8001149c`.
- `0x80043e90`: default animation speed, 32.
- `0x8003ddc8`: loaded VDF descriptor, containing nine deformation channels.
- `0x8003ddd0`: pointer to the nine live VDF weights (observed `0x80050dd4`).

At `0x80011270` onward, the demo multiplies the low 16 bits of its frame
counter by speed, shifts right by six, and selects one of 16 table entries.
The default speed therefore gives a 32-update walking cycle. This is distinct
from the longer jaw animation and from internal CD command queue counters.
The observer records the changing VDF weights along with that counter.

A 480-field window, fields 3600 through 4079, yielded complete cycle intervals
of **57, 57, 58, 57, 57, 58, 57, 57 fields**: mean 57.25 fields, about
**0.955 seconds** assuming 59.94 NTSC fields per second. These are emulator
observations, not physical PlayStation timing measurements. Camera, rendering
load, emulator implementation and scene state can affect the observed rate.

The Saturn viewer defaults to `DINO_WALK_SPEED_PERCENT=140`, making its
calibrated GLB cycle about 0.952 seconds. This matches the observed
cadence approximately. The initial version used reconstructed poses; the
current Walk uses recovered original deformation. Roar retains its
source timing. The importer also normalizes float32 endpoints close to its
sample grid, so this 40/30-second loop bakes to 20 frames at 15 Hz rather
than gaining an extra held frame. Set the parameter to `100` for the original
reconstruction's speed.

## Repeatable observer

`harness/scripts/ps1_dino_reference.lua` observes guest state without writing
guest RAM, changing input, loading savestates or patching the program. It pins
four relevant instruction words, verifies that the VDF is loaded, records
480 warmed fields and pauses automatically. Run from the repository root with
local paths to PCSX-Redux, OpenBIOS, the corrected CUE and extracted DINO.EXE:

```sh
SDL_AUDIODRIVER=dummy "$REDUX" \
  -no-ui -interpreter -debugger -softgpu \
  -bios "$OPENBIOS" -iso "$DINO_CUE" -exe "$DINO_EXE" \
  -run -webserver -webserver-port 8083 \
  -dofile "$PWD/harness/scripts/ps1_dino_reference.lua"
python3 harness/scripts/ps1_dino_cadence.py \
  harness/build/ps1_dino_reference.jsonl
```

The analyzer requires consecutive fields, progressing guest frames, the
default speed and animated VDF weights. It reports complete cycle intervals
and writes `ps1_dino_reference.analysis.json` next to the capture. Intermediate
RAM scans used to locate the counters can be discarded after recording the
result; the observer only retains small JSONL records.

## Saturn validation

The prepared Blender mesh contains 1799 triangles. VDP1 conversion produces
1698 faces and 1191 welded vertices, using 278680 bytes of texture VRAM and
285840 bytes for both pose streams. The import profile passes with 283
commands of estimated headroom before runtime culling.

The rebuilt ISO opened in Mednafen on stock RAM. A 1200-field Ymir sequence
exercised pause/resume, camera, zoom, light, reset, Roar and return to Walk.
It completed 509 frames with zero task failures, draw overflow, music errors
or panic; the Slave executed 1018 tasks. The existing animated-import suite
(20 tests) and the float32 endpoint regressions (2 tests) passed.
Runtime captures and analysis are in `harness/build/dino_runtime*`; the
small PS1 cadence summary is retained in
[`harness/studies/dino-ps1-cadence.json`](../harness/studies/dino-ps1-cadence.json).

## First lateral visual comparison

`harness/scripts/ps1_dino_visual.lua` uses controller Square to turn the
original model for 32 guest updates and releases the button. It then fixes
the view to exactly 90 degrees and captures fields 800 through 919 at
four-field intervals. The first version held Square for only 20 display
fields, producing a three-quarter view; its silhouette comparisons are
withdrawn. PadRead swaps the controller bytes, so physical Square maps to
the handler's 0x0080 yaw input, while physical D-pad Right translates X.

Unlike the cadence observer, this visual harness deliberately writes view
state. It sets the yaw halfword at `0x80043eda` to 1024 (4096 units per turn),
and the view position at `0x8004855c` / `0x80048564` to X=6000 / Z=-50000
to keep the whole tail inside the viewport. Pitch and roll remain zero.
Animation data, weights, speed and executable code are untouched. Every
captured record includes the view state, and the decoder rejects captures
whose rotation differs from `[0, 1024, 0]`. Create
`harness/build/dino_comparison` and use the same Redux invocation above with
`-dofile harness/scripts/ps1_dino_visual.lua`. The script pauses automatically.
The output records guest phase and speed alongside raw VRAM snapshots.

The Saturn sequence was executed with the current ISO and Ymir, stock RAM,
540 fields, Z at field 150 (disable the red light), release at 170, Down
from 180 through 189 to lower its default ten-degree pitch, and screenshots
every four fields from 400 through 520. Camera RAM is also dumped to verify
the final yaw and pitch.
Both emulator commands completed. The first comparison artifacts are local
under `harness/build/dino_comparison`.

Comparison limitations and controls:

- The old claim about a more compact original silhouette was based on a
  three-quarter view and cannot establish a model or rig difference.
- The initial Saturn sequence had an orange/red band moving across the body.
  Inspection identified the deliberate VDP2 red-light sweep in `light_update`.
  The final sequence disables it with Z before comparing source colors.
- Matching the walking period does not establish matching foot trajectories,
  support duration, body bob or tail motion. These still need phase alignment.

These are visual observations, not measured anatomical errors. Perspective,
lighting and animation phase are not yet matched. Raw Saturn
screenshots are 640 by 224 and PS1 framebuffer crops are 640 by 240; displaying
them as square pixels produces misleading proportions. The comparison also
provides a version with each full viewport normalized to 4:3. No geometry
or rig correction has been made based on this first pass.

Decode and assemble the captured sequences with Pillow available:

```sh
python3 harness/scripts/dino_visual_compare.py harness/build/dino_comparison
```

`comparison-profile.png` shows the two display views and `walk-profile.gif` shows the sampled
sequences with independent phases. The raw upper-buffer sequence can include
an incomplete rendering; it is not a measurement of visible frame pacing.

The PS1 decoder uses the upper framebuffer at VRAM (0, 0), not the emulator's
GUI screenshot API. The other buffer begins at row 240. The recorded guest
phase is sampled at Vsync and is not guaranteed to identify the exact rendered
pose in that buffer; a synchronized pose comparison must account for buffering.

## Recovered original walking deformation

The read-only observer `harness/scripts/ps1_dino_motion.lua` stops at
`0x80012650`, after the original program resets the mesh and applies VDF
deformation, before its view matrices are used for drawing. It captures
64 consecutive guest updates, frames 64 through 127, without controller
input, view patches or guest memory writes. Its pinned instructions cover
the VDF entry and GTE deformation operation. The local artifacts are one
2 MiB RAM snapshot, 64 sets of 1376 SVECTOR positions, and JSONL weights.
Game executable, disc and RAM captures remain private local inputs.

The loaded TMD descriptor at `0x8003ddb8` exposes the original vertex backup
and an object table with live vertex pointers. The VDF descriptor at
`0x8003ddc8` supplies nine displacement spans and their current weights.
Each channel adds signed displacement times weight, shifted right by 12,
with GTE IR saturation and SVECTOR storage. Replaying all nine channels
matches every captured integer coordinate across the 64 updates exactly.

All 1376 TMD points match the 2332 OBJ entries (which include duplicate
positions), after axis conversion, scale and the OBJ export's translation.
Maximum correspondence error is 0.00000413 OBJ units. The first four VDF
channels repeat exactly every 32 updates. These provide the transferred Walk;
the independent eye deformation and other interactive channels are excluded.

`tools/bake_ps1_dino_walk.py` finds the closest original surface point for
each prepared-mesh point and transfers displacements using barycentric
weights, retaining its static surface offset. The prepared mesh already lies
on the source surface for most points: its 95th percentile rest-surface
distance is below 0.00515 scene units. The maximum is 0.0661, including
reconstructed mouth details and simplified extremities. That distance is a
rest-surface correspondence diagnostic, not an animation error bound.

The emitted standard GLB uses 987 synthetic translation joints as an offline
interchange representation. Both clips are checked after loading the emitted
file again; maximum coordinate roundtrip error is below 0.00000041 scene units.
Saturn still uses the existing quantized vertex decoder. No PS1 animation
runtime or extra skeletal work is added to the console.

To repeat with the same local Redux paths as above:

```sh
mkdir -p harness/build/dino_motion
SDL_AUDIODRIVER=dummy "$REDUX" \
  -no-ui -interpreter -debugger -softgpu \
  -bios "$OPENBIOS" -iso "$DINO_CUE" -exe "$DINO_EXE" \
  -run -dofile "$PWD/harness/scripts/ps1_dino_motion.lua"
python3 tools/bake_ps1_dino_walk.py \
  --capture harness/build/dino_motion --obj "$DINO_SOURCE_OBJ" \
  --mesh examples/dino_demo/assets/ps1_trex_saturn.glb \
  --output examples/dino_demo/assets/ps1_trex_saturn_original_walk.glb \
  --report harness/build/dino_motion/transfer.json
```

The observer pauses the emulator after capture; exit that process before the
following command in a sequential shell. Two independent capture runs produced
the same transferred GLB SHA-256. The compact validation report is retained
in `harness/studies/dino-ps1-motion-transfer.json`.

## Aligned geometry studies and updated runtime

`tools/compare_ps1_dino_poses.py` produces side, front and back sheets at eight
phases, with one orthographic camera, scale and ground line. Columns show
the original source deformation, the previous reconstructed Walk, and the
transferred Walk. The old reconstruction gets its best fixed cyclic phase
offset, minimizing lower-leg surface point distance against the new reference.
These are host geometry studies, not emulator screenshots or texture checks.

```sh
python3 tools/compare_ps1_dino_poses.py \
  --capture harness/build/dino_motion --obj "$DINO_SOURCE_OBJ" \
  --before examples/dino_demo/assets/ps1_trex_saturn.glb \
  --after examples/dino_demo/assets/ps1_trex_saturn_original_walk.glb \
  --output harness/build/dino_original_walk/phases.png
```

The updated default build retains 32 Walk poses at 24 Hz on the calibrated
timeline and 20 Roar poses at 15 Hz. Both streams occupy 307944 bytes, with
maximum quantization error below 0.000049 scene units. Using 32 poses for
both clips exceeded High WRAM by 9152 bytes; preserving the existing Roar
sampling instead passes the stock-RAM link and runtime gates.

The model has 1798 triangles, converted to 1776 VDP1 faces and 987 animated
vertices, with 310592 bytes of texture VRAM and 205 commands of estimated
headroom. The corrected build completed 508 frames, 508 pose jobs and 1016 Slave
tasks in the same 1200-field Ymir control sequence (the preceding build
completed 509 frames). Panic, task failures, draw overflow and music errors remain zero.
The existing 20 animated-import tests and eight sampling/transfer regressions
passed. Mednafen also opened the updated stock-RAM disc successfully.


## Texture-coordinate and seam correction

The initial motion exporter wrote SourceModel UVs directly into glTF.
SourceModel uses a bottom-left origin, whereas glTF uses a top-left origin.
Reloading flipped the vertical coordinate again, mapping dark/transparent atlas
regions onto the neck and back. The exporter now converts `v` to `1-v` when
writing glTF; the roundtrip regression checks UVs as well as both pose streams.

Blender preparation also welds coincident UV/normal-split vertices with matching
skin weights before decimation. Per-corner UVs remain independent; upper/lower
lip vertices with different weights remain separate. This reduced geometric
boundary edges after positional welding from 685 to 142 (the source has 195,
including intentional mouth/detail borders). This count diagnoses topology;
it is not a claim that every remaining border is a defect.

Ymir confirms the display changes from the boot's 320x224 to 640x224 with dot
clock multiplier 2. This is horizontal hi-res with 224 vertical lines, not a
448-line interlaced display. Raw captures are 640x224 and require 4:3 pixel
aspect correction for normal viewing. The HUD intentionally doubles glyphs
horizontally, so its font alone does not reveal the horizontal pixel density.

Corrected neutral screenshots are under `harness/build/dino_seams/fixed_*.png`.
An A/B run with backface culling disabled retained the dark bands; correcting
UV export removed them. The shipping build retains backface culling.


## Original surface and frontal entrance

The active GLB now uses `--original-topology`: all 2724 source OBJ triangles,
1376 original TMD points and original corner UVs are retained. There is no
Blender reduction or reconstructed mouth/teeth geometry in the active surface.
Walk is evaluated directly on the source points, rather than transferred to
a decimated mesh. The editable reconstruction and reduced mesh remain archived.
Reconstructed Roar displacements are sampled on the original surface and remain
approximate; its correspondence error is below 0.00000033 scene units.

The importer previously missed quad candidates across normal-split vertices.
It now matches corners by full motion and UVs before looking for adjacent
triangles. UV seams and differing motion remain separate, covered by regression
tests. This permits 825 safe pairs: 2724 triangles become 1899 VDP1 faces,
with 82 commands of estimated headroom and 429312 bytes of animation streams.
The original mesh's Walk/UV roundtrip errors are below 0.00000061 scene units
and 0.000000024 UV units respectively. Console position quantization adds its
separately reported error; this is not a claim of identical rendered pixels.

The entrance starts at yaw 0 and pitch 0, looking directly at the approaching
model, and B restores those same angles. This supersedes the authored 25-degree
three-quarter entrance. The six-second timing and reveal remain authored
presentation, not recovered PS1 intro code.

The original source remains textured with a shared hi-res palette on Saturn.
PS1 lighting, rasterization and color precision still differ; preserving the
original surface does not by itself reproduce the PS1 Gouraud lighting.


The current texture density of 540 uses 241088 bytes of texture VRAM and passes
the stock-RAM link. Density 960 failed the VRAM gate; density 720 passed VRAM
but overflowed High WRAM. Both 32-pose Walk and 20-pose Roar are retained.
The 1200-field control scenario completes 467 frames with zero panic, draw
overflow, music errors or failed tasks. The previous reduced three-quarter
build completed 520 frames; geometry workload and the view both changed.
Neutral capture guest RAM confirms camera yaw/pitch `(0, 0)`.
Artifacts are under `harness/build/dino_quality/`.
