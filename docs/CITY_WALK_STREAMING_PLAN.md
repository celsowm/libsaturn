# city_walk: streaming a city that does not fit in memory

Design record for `examples/city_walk`: a first-person walk through the
"Low Poly City Game-Ready" model (costoWRLD, CC-BY-4.0). The example is the
first consumer of three things the library did not have: geometry that is
not resident, a residency manager for it, and a tool that turns an ordinary
GLB into a paged archive. This document records what was decided, what was
measured, and which measurements turned out to be wrong.

## Source asset

| | |
|---|---|
| Geometry | 969 meshes, 1874 nodes, **296,901 triangles** (median 54 per node, max 7,916) |
| Materials | 37, of which 6 are textured (a palette atlas carries most of the buildings) |
| Extent | 488 x 52.6 x 488 units, essentially flat |
| Compression | `KHR_draco_mesh_compression` + `EXT_texture_webp`, 6.6 MB; decodes to 23.7 MB |
| Street | 12,699 flat triangles at y = -0.125; **no street at all under half the city** |

Consequences that shaped everything:

* 16.16 fixed point silently flips sign above ~181 units. A 488-unit city
  cannot hold a city-wide coordinate anywhere.
* The city occupies 119 of 256 chunks of a 16 x 16 grid; the densest chunk has
  13,762 triangles.
* Only 296,901 triangles total, but the VDP1 painter with no Z-buffer manages a
  few hundred faces per frame. Almost all detail must go.

## Decisions

1. **Vendor the compressed GLB, decode Draco at build time** (`npx
   @gltf-transform/cli`, 3.9 s, cached by content hash).
2. **Two-tier streaming.** CD -> cart once at boot, with a progress screen;
   cart -> work RAM while walking.
3. **A 4 MB RAM cartridge is mandatory**, with a clear refusal screen otherwise.
   The archive is ~1.75 MB, so this is a placement contract, not a volume
   requirement: `sat_ram_cart_alloc` never spans a bank, and every blob must be
   contiguous inside one 2 MiB bank so it can be copied with one loop.
4. **The street is VDP2, not VDP1** (from the user, during implementation). It
   is flat, so it costs nothing on the painter and cannot mis-sort against
   buildings. A 512x512 8 bpp bitmap (one unit per dot) shown as an RBG0
   Mode-7 plane; it fills VDP2 banks A0 and A1, so the rotation and
   coefficient tables live in B0 (which needed KTAOF, see below).
6. **As close to the GLB as possible** (from the user, after the block
   version). Blocks alone were solid but did not look like the model; the
   buildings now wear facade textures baked from it, streamed into VDP1 VRAM.
5. **New tool, not an extension of `import_model.py`.** That importer merges all
   primitives into one soup and requires a skin; a chunker needs the opposite.
6. **Distance fade** through the VDP2 colour-calc slots, as skybridge does.

## Measurements (Ymir emulator, NTSC, 320x224)

| | result | note |
|---|---|---|
| R1: cost of a solid face | ~110 us/face all-in (project + sort + emit) | the probe alone said 600 faces = 3 VBlanks; the whole system costs ~40% more, and the budget counts blob faces, ~2x the faces that reach the VDP1 |
| R1 caveat | **Ymir does not model VDP1 fill rate** | 900 faces at 1x, 2.5x and 4x coverage cost exactly the same |
| R2: Draco decode | 3.9 s | 6.6 -> 23.7 MB |
| R3: CD -> WRAM-L, 1 MiB | 3.83 s, drive-bound | ~27 ms per call + 6.5 ms per sector (~2x drive) |
| R3: WRAM-L -> cart | 11.9 MB/s with 32-bit stores, 7.6 with 16-bit | verified read-back, 0 mismatches |
| R3: cart -> WRAM-L, 4 KiB | 0.34 ms (32-bit), 0.53 ms (16-bit) | |
| Real load of CITY.BIN | 154 VBlanks = 2.6 s | |
| Frame cost split (per frame, cap 500) | Master submit ~10 ms, flush ~9 ms, decode ~1.5 ms, plan ~1.5 ms, Slave wait ~2 ms | Slave and Master balanced by face weight |
| Face cap | **500 blob faces** | 400 gave 30 fps almost always, 600 gave 30 fps in ~84% of frames; 500 leaves room for fill rate the emulator does not see |

Two measurement traps worth remembering:

* `sat_frame_count()` floors per call, so summing many short intervals gives 0.
  Use raw FRT ticks for anything under a few frames.
* Sample the frame cost at the *same point of consecutive iterations* (the top
  of the loop). Sampling between `begin_frame` and `end_frame` measures only
  the render window, which always fits in one VBlank.
* A span of 64 sectors read ~3.5x faster than a 2x drive can deliver. That is
  an emulator artefact; the loader uses 32.

## Architecture

```
tools/city_chunker.py --> CITY.BIN (disc) --> RAM cart --> ring slots (WRAM-L)
                                      |                          |
                       ground bitmap -> VDP2 VRAM        decode -> submit
```

* **Grid.** 16 x 16 chunks of 32 units, origin (-128, -320).
* **Geometry = extruded blocks** (`tools/model_pipeline/blocks.py`). The first
  version simplified each chunk's triangles (QEM, ~1,700 -> 176 faces) and the
  result was unacceptable: hollow building shells tore into shards, roof slabs
  floated, walls went missing. The chunker now samples every triangle into a
  0.5-unit top-down surface model (max height, top colour, mean wall colour),
  sends the low cells (<= 0.6 u: road, kerb, lawn) to the ground bitmap and
  extrudes the rest as flat-topped regions: walls where a neighbour is lower,
  tops only below the 2-unit eye. Nine block levels, finest first (coarser
  ones close gaps, then drop slivers); each chunk LOD takes the finest that
  fits its caps and keeps enough of the chunk's buildings (50% at LOD0, 25% at
  LOD1). 107 chunks.
* **Facades.** Every block face carries a texture baked from the source
  (`tools/model_pipeline/facades.py`), in the face's A->B / A->D frame so the
  VDP1's distorted-sprite mapping lands every texel where it was. Archive
  format 2: a blob's texture block follows it in the cart (TOC entries grew to
  16 bytes), faces name a texture in their spare byte, one 255-colour palette
  goes to CRAM bank 5. 1.75 MB archive, ~1 min build, 11,322 textures.
  Measured against a z-buffered render of the GLB (`tools/city_fidelity.py`):
  per-chunk QEM 33%, per-object QEM 33%, plain blocks 31%, textured blocks 26-28%
  of 4x4 blocks visibly different; the metric punishes a one-pixel shift of a
  window pattern, so the pictures are the real judge. Facade subdivision is
  two-dimensional near the camera: LOD0 first tries 4x4-unit independently
  baked patches and LOD1 starts at 8x4, then each falls back through coarser
  candidates when the chunk caps demand it. This prevents one tall close wall
  from losing its whole texture merely because one projected corner exceeds
  the renderer's safe distorted-sprite window.
* **Library changes this needed.** `sat_vdp1_vram_reserve` / `sat_vdp1_vram_write`
  (fixed VDP1 texture spans a program streams into, write waits for the last
  list); a textured face with a corner behind the near plane falls back to its
  material's `rgb555` as a clipped polygon instead of being dropped; the VDP2
  HAL marks every bank an RBG0 bitmap spans (512x512 needs A0 and A1) and the
  ground environment sets KTAOF for a coefficient table past 256 KiB.
* **LODs.** Three per chunk, at most 176 / 64 / 24 faces and 384 / 144 / 48
  vertices, in rings of 3x3 / 5x5 / 7x7. Slots are direct-mapped and fixed-size
  (4096 / 1536 / 1024 bytes), 122.5 KiB total, holding *raw big-endian blobs*:
  the decoded form is 1.5-2x larger and would not fit.
* **Coordinates.** Player = (chunk, local). Vertices are decoded relative to the
  player's chunk with `x = origin_rel + (int16 << 10)`. Worst magnitude
  `(3 + 1) * 32 + 16 = 144` units, under the 176 hard limit. `city_grid.h`
  computes it from the constants and a host test pins it.
* **Archive.** `city_format.h` is the specification. Header 128 B, material
  table, TOC of 768 x 12 B (chunk-major, LOD-minor, with a `bank` field), the
  ground bitmap and palette, then 32-byte-aligned blobs that never straddle a
  2 MiB boundary. Everything big-endian, pointer-free. The TOC's `bytes` field
  is u16, so a blob is at most 65,535 B.
* **Residency.** A pure state machine (`city_grid.h`): `recenter`, `next`,
  `complete`, `lookup`. A cell is drawable only if its slot holds exactly that
  cell in READY state, so a slot reassigned mid-frame cannot be drawn. One
  page-in per frame; the renderer falls back to a coarser resident LOD and counts
  a pop-in instead of ever blocking.
* **Frame planning.** `city_plan_frame` picks each cell's LOD, culls by the
  XZ footprint against the view wedge, and enforces the face budget **far to
  near**: the farthest cells coarsen first (at most one LOD below what their
  distance calls for), then the farthest are dropped. Coarsening onto an EMPTY
  slot (a chunk whose props vanish at LOD2) drops the cell rather than planning
  a zero-byte item. The budget counts *blob*
  faces, before backface culling; about half reach the VDP1.
* **Two SH-2s.** The Slave prepares the *farthest* cells as a batch, up to half
  of the frame's blob faces; the Master decodes and submits the rest through one
  scratch mesh. Each batched item owns its decoded mesh, so at most ten go to the
  Slave. A fixed count of outer cells left the Master waiting ~15 ms for the Slave
  once the far-to-near budget had shrunk its own share. Failure falls back to the
  Master.
* **Ground.** RBG0 with a per-scanline coefficient table. The eye is exactly
  `focal` = 2 units high, because the plane's perspective is calibrated to it.
  Position and yaw go into the rotation matrix and Mx/My **with their 10-bit
  fraction**; an integer camera would snap the ground 1-2 units against smoothly
  moving buildings. Rows near the horizon are transparent to avoid the bitmap
  repeating at hundreds of units.
* **Fade.** Eight colour-calc slots, opaque up to 64 units, fully transparent at
  128, with a 2-unit hysteresis band via `sat_fade3d_slot`, one state byte per
  chunk. Enabled once with `sat_vdp2_sprite_color_calc_configure_alpha(7)`; ordinary
  sprites keep priority 7, faded ones take 6, RBG0 is 5 and NBG0 is 2. This was
  in the original plan and was missed until the user asked whether it was in.

## Findings that corrected the plan

* The first plan said pass 0 = nearest. The painter paints **higher passes
  last**, so the nearest chunk gets the highest pass.
* `pass` is per instance, not per face, so a per-face ground bias was
  impossible; moving the ground to VDP2 removed the need.
* The library camera is **right-handed** (as glTF): looking along +Z, screen-right
  is -X. The first preview was mirrored until this was fixed; the runtime winding
  (`A,B,C,D` = reverse of CCW, so `cross(D - A, B - A)` points out) was right all
  along.
* `sat_hud_init` returns `INVALID_ARG` for a zero character spacing, and the
  error had been swallowed: the HUD was silently absent for half the session.
  Telemetry now records the first HUD failure.
* The nearest-first budget let the street eat the whole allowance and thinned
  the horizon unpredictably; far-to-near keeps the street intact.
* A material's `baseColorFactor` is linear and must be encoded to sRGB once.

## Verification

| | where |
|---|---|
| grid, rings, residency, planning, decoder vs corrupt input, coordinate bound, collision | `tests/host/test_city_walk.cpp` (`make test`) |
| chunker on a synthetic city; archive read back by an independent parser and by the C reader | `tests/test_city_chunker.py` |
| four emulator scenarios: walk (must page), wall, no cart, 1 MB cart | `harness/run-city-walk-checks.ps1` |
| host preview of any view, ground and buildings | `tools/city_preview.py` |

The harness test asserts invariants (`loaded - evicted == resident`, no decode
failures, Slave used with zero fallbacks, refusal screens) and a frame-rate
budget read from a VBlank-cost histogram, never an average.

## What is not known

* **VDP1 fill rate on hardware.** The emulator does not model it, so every
  frame-rate figure is an upper bound.
* **A-Bus wait states** of the cartridge. The 0.34 ms page-in is the emulator's.
* Whether the fade's colour-calc interacts with the RBG0 ground exactly as in
  skybridge on real hardware.
* Behaviour beyond the 4 MB cartridge: nothing here needs more, and the plan's
  optional "use all 4 MB" tier was dropped because the renderer cannot draw more
  geometry than the rings already hold.
