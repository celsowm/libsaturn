# city_walk

A first-person walk through a low-poly city that does not fit in memory. The
city streams from the CD into a **4 MB RAM cartridge** once at boot, then pages
from the cartridge into work RAM while you walk. The streets are not polygons
at all: they are a VDP2 rotation plane.

The city model is **"Low Poly City Game-Ready" by costoWRLD**, licensed
CC-BY-4.0. See [assets/LICENSE.txt](assets/LICENSE.txt) for the attribution and
the note on what was derived from it. The credit is also drawn on screen for the
first few seconds.

## Requirements

**A 4 MB RAM cartridge is mandatory.** Without one the program says so and
stops. A 1 MB cartridge is refused too, not loaded halfway, because the archive
relies on two 2 MiB banks (`sat_ram_cart_alloc` never spans a bank).

The honest reason is placement, not volume: the archive is ~1.75 MB. Nothing
here needs 4 MB of *data*; it needs a cartridge whose banks are big enough that
every blob sits contiguously inside one.

## Controls

| | |
|---|---|
| D-pad up / down | walk forward / back |
| D-pad left / right | turn |
| L / R | strafe |
| B (held) | run |
| X | jump to the next street viewpoint (seven of them) |

Speeds are chosen for the demo, not for realism. At a believable walking pace
in a city this size the player would never leave the first chunk in a whole
session, and nothing would page. Walking is 8 units/s, running 24; a chunk is
32 units. Running can outpace the page-in queue, which is deliberate: the
renderer then falls back to a coarser level of detail instead of waiting.

## How it works

```
  low_poly_city.glb (6.6 MB, Draco)
        |  tools/city_chunker.py   (build time, ~1 min)
        v
     CITY.BIN  (1.75 MB, on the disc, never in the executable)
        |  CD -> WRAM-L staging (64 KiB spans) -> 32-bit stores
        v
   RAM cartridge  (2 x 2 MiB banks, volatile, data only)
        |  one 4 KiB page-in per frame, 32-bit loads
        v
   work RAM rings: 3x3 LOD0 + 5x5 LOD1 + 7x7 LOD2 = 122.5 KiB
        |  decode into player-relative coordinates, project, sort
        v
   VDP1 faces (buildings)   +   VDP2 RBG0 plane (streets)   +   VDP2 NBG0 sky
```

* **Chunks.** The city is cut into a 16 x 16 grid of 32-unit chunks (107 of the
  256 hold buildings).
* **Buildings are blocks wearing the source's facades.** A chunk averages
  ~1,700 source triangles and LOD0 holds 176 faces, and one tower alone has
  7,900. No simplified mesh gets close to the model at that budget: decimating
  whole chunks tore buildings into shards, simplifying each building on its
  own still dropped the loose slabs and frames it is made of. What the Saturn
  games of the time did, and what this does, is simple shapes with the detail
  in textures:
  - the chunker samples the city into a 0.5-unit top-down surface model and
    extrudes it into closed blocks (walls where a neighbour is lower, a top
    only below the 2-unit eye: a roof above the eye is never seen);
  - every block face is then **baked from the source**: the geometry within a
    thin slab around the face (windows, frames, balconies, signs) is projected
    onto it, supersampled 2x2, and quantised to one 255-colour palette. The
    bake runs in the face's own A->B / A->D frame, which is how a VDP1
    distorted sprite maps its texels, so no wall comes out mirrored (a test
    places a red plaque on one corner and finds it there);
  - facade pieces are cut in **both axes** near the player. LOD0 first tries
    4x4-unit patches and LOD1 starts at 8x4; dense chunks progressively fall
    back through larger/one-axis cuts until the hard face/vertex caps fit.
    This is not just texture-warp control: a close tall distorted sprite can
    project a remote corner beyond VDP1's safe off-screen range, and without
    vertical cuts the renderer must replace that whole facade with its solid
    fallback colour.
* **Three levels of detail** per chunk: the finest block level (coarser levels
  join close neighbours, then drop slivers) whose faces fit 176 / 64 / 24.
  Every facade face keeps a texture: the baker starts at 4 texels/unit for
  LOD0 (down to 1 at LOD2) and lowers texel density as far as necessary before
  it will sacrifice coverage. Thus moving closer cannot select a richer mesh
  whose walls are suddenly solid. Fixed VDP1 texture slots are 16 / 5 / 1.5
  KiB. A level may not lose
  more than half of a chunk's buildings at LOD0 (a quarter at LOD1); thin props
  may vanish at LOD2, 80+ units away. Rings of 3x3 / 5x5 / 7x7
  chunks around the player hold them, in fixed, direct-mapped slots: no
  allocator, no fragmentation, eviction is "this slot now belongs to another
  cell".
* **Textures stream with the geometry.** 350 KiB of VDP1 VRAM is reserved once
  (`sat_vdp1_vram_reserve`), one fixed span per ring slot. A page-in copies the
  blob to work RAM and its texture block straight from the cart to the slot's
  VRAM (`sat_vdp1_vram_write`), just before `sat_end_frame`: the VDP1 may still
  be drawing a list that reads what the slot held before, and that call waits
  for it anyway. The slot is drawable only after the copy. A textured wall
  that crosses the near plane (a sprite cannot be clipped) is drawn as a
  clipped polygon of its solid colour instead of disappearing at your shoulder.
* **Coordinates never leave a small range.** 16.16 fixed point silently flips
  sign above ~181 units and the city is 488 wide, so nothing holds a
  city-wide coordinate. The player is a (chunk, local) pair and every vertex
  is decoded relative to the player's chunk. The worst magnitude is 144 units;
  a host test proves it for every cell of every ring.
* **Streets on VDP2.** Every flat triangle within 0.6 units of the street
  (road, kerb, lawn, lane dashes, crosswalk stripes) goes into a 512x512 bitmap,
  one unit per dot, that becomes an RBG0
  Mode-7 plane under the polygons. Painted marks are coplanar with the road
  and thinner than a dot, so the rasteriser draws big before small at equal
  height and takes 4x4 samples per dot, letting a mark that covers a quarter
  of a dot own it. That removes the flat ground from the VDP1
  face budget and from the painter's ordering problems (there is no Z-buffer).
  The eye is exactly 2.0 units high because the plane's perspective is
  calibrated to it.
* **The face budget is enforced, not hoped for.** `city_plan_frame` limits a
  frame to `CITY_FACE_CAP` (500) faces by coarsening the farthest cells first
  (never more than one level below what their distance calls for) and only then
  dropping them, so the street keeps its detail and the horizon thins out. The
  budget counts the faces in the blobs, before backface culling; about half of
  them reach the VDP1.
* **Distance fade.** As in `skybridge_3d`, the far rings dissolve into the sky
  and ground instead of appearing whole at the edge of the paged area: eight VDP2
  colour-calculation slots through `sat_fade3d_slot`, opaque up to 64 units and
  fully transparent at 128, with a 2-unit hysteresis band per chunk so a cell on
  a boundary does not shimmer.
* **Two SH-2s.** The Slave prepares the farthest cells as a batch, up to half
  of the frame's faces, while the Master decodes and submits the rest. Splitting
  by weight matters: giving the Slave a fixed count of outer cells left the
  Master idle for ~15 ms a frame. If the Slave is missing or fails, the Master
  does the same work: a frame is never short of faces.
* **The residency invariant.** `chunks_loaded - evictions` is always the number
  of resident slots. The harness checks it on a real walk.

## Numbers, and what they do and do not mean

Measured on the Ymir emulator (harness), NTSC, 320x224:

| | |
|---|---|
| CD -> cart load of CITY.BIN | ~6.8 s for 1.75 MB (limited by the drive: ~27 ms per read call plus 6.5 ms per sector) |
| cart -> work RAM, one 4 KiB page | 0.34 ms (32-bit loads; 0.53 ms with 16-bit) |
| frame rate | 30 fps (2 VBlanks) for the large majority of frames in every viewpoint tried; the rest are 20 fps |
| residency in work RAM | 122.5 KiB of raw big-endian blobs |
| facade textures | 350 KiB of VDP1 VRAM in 83 fixed slots; a page-in's block (4 KiB on average) goes cart -> VRAM in well under a millisecond (SCU DMA) |

**These are emulator numbers, and one of them is a lower bound.** Ymir does not
model VDP1 fill rate: the same number of faces costs the same whether they
cover a tenth of the screen or four times it. What was measured is the CPU cost
of projecting, sorting and emitting faces, which is real, plus zero per-pixel
cost. On hardware the fill rate adds on top, so treat the frame rate as an
**upper bound**, not a prediction. The cartridge timings likewise assume the
A-Bus wait states Ymir models, which have not been checked against real
hardware. The CD figure is bounded by physics: a 2x drive delivers at most
~307 KB/s, so expect 5-8 s on a console.

## Known limits

* The source model has **no street under half of the city**; only a white base
  slab far below. The ground plane fills the slab's rectangle with that colour
  and real street, kerb, crosswalk and lawn data paint over it where it
  exists. The lots of the western blocks really are near-white in the source.
* The 1208 triangles of the model's alpha-masked tree-sprite cards were dropped:
  as opaque faces they became solid green squares.
* Blocks are boxes under their textures: pitched roofs and overhangs are
  flat pictures on the wall, and lamp posts, traffic-light poles and thin trees
  (under 1 unit wide) are gone. Distant small houses may merge into one block.
* `tools/city_fidelity.py` renders the source GLB and any CITY.BIN from the
  same viewpoints (z-buffered reference, VDP1-style affine texturing for the
  archive) and scores them; `tools/city_preview.py` renders street views.
* Beyond the city edge the bitmap wraps, so a copy of the ground can show past
  ~25 units outside the footprint. The player cannot leave the grid.
* The horizon is fixed (the camera does not pitch): the ground plane's
  perspective is exact for a level view only.

## Building and testing

```powershell
.\build-example.ps1 city_walk      # first build: ~1 min (Draco decode, blocks, facade bake)
.\harness\run-city-walk-checks.ps1 -Bios .\bios\saturn_bios_us.bin
```

The first build needs Node.js (`npx`) to decode the Draco-compressed source,
and `numpy` and `Pillow` (`pip install -r tools/requirements-city.txt`). The
decoded GLB and a per-chunk cache live under `build/generated/city_walk/`.

Tests, all runnable without hardware except the harness scenarios:

* `tests/host/test_city_walk.cpp`: grid, rings, the residency state machine,
  frame planning, blob decoding against corrupt input, the coordinate bound and
  walking collision (`make test`).
* `tests/test_city_chunker.py`: the chunker on a synthetic city, including the
  archive layout read back by an independent parser and by the C reader.
* `harness/tests/test_city_walk.py`: four emulator scenarios (a walk that must
  page, a walk into a wall, no cartridge, a 1 MB cartridge).

To see a view without a console: `python tools/city_preview.py
build/generated/city_walk/iso/CITY.BIN --eye 3,7 --local 8,8 --yaw 0 --out x.png`.

## Files

| | |
|---|---|
| `main.c` | start-up order, refusal screens, frame loop |
| `loader.c` | CD -> cart -> VDP2, header/TOC/CRC checks |
| `residency.c` | the rings: cart -> work RAM, one slot per frame |
| `render.c` | plan, decode, Master/Slave split, submit |
| `materials.c` | the archive's shade table -> one solid-colour pool |
| `ground.c`, `background.c` | RBG0 streets, NBG0 sky |
| `player.c`, `hud.c`, `telemetry.c` | walking and collision, text, harness readout |
| `city_grid.h`, `city_format.h` | pure logic and the archive format (host-tested) |
