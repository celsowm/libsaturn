# `vdp2_dynamic_tile_stream` — bounded map streaming with a tear-free ring flip

A top-down race road scrolls continuously. The 64x64-cell map is a torus of
16x16 metatiles (4x4 cells each) generated from an original road tileset; the
camera advances 2 px per frame. The interesting part is not the art but how
the map reaches VDP2 without a seam or a torn frame.

## Two rings, one visible

| Resource | Address |
| - | - |
| Pattern-name ring A | `0x00000` (4096 words, 64x64 cells) |
| Pattern-name ring B | `0x04000` (4096 words) |
| Character tiles | `0x40000` (16 tiles, 16-colour) |
| CRAM palette 0 | first 16 entries |

The public `include/saturn/stage_map2.h` keeps a bounded resident window of
the map: the 320x224 viewport plus two cells of
margin on each side (45x33 cells). `sat_stage_map2_set_view` stages only the
cells that entered the window into a caller-owned buffer, and
`sat_stage_map2_commit_vdp2` writes them through the validated VDP2 VRAM
writer. No heap is involved; the two staging buffers are fixed arrays whose
sizes come from `sat_stage_map2_requirements`.

Two independent `sat_stage_map2_t` instances target ring A and ring B. The
frame sequence is:

1. Wait for VBlank and run `sat_vdp2_layers_commit()`; the ring base and
   scroll staged last frame become visible together.
2. Flip `active` if a switch was pending.
3. Move the camera, call `sat_stage_map2_set_view` on the **inactive**
   streamer and commit its runs.
4. Call `sat_vdp2_layer_set_pattern_name_base(NBG0, ring)`; this validates and
   stages the base, updating the layer state and cycle planner but writing no
   register. Then stage the wrapped scroll.
5. The next VBlank commit writes MPABN0/MPCDN0 and the scroll; the inactive
   ring is already complete, so no scanline can see a half-written map.

The new public function is:

```c
sat_result_t sat_vdp2_layer_set_pattern_name_base(sat_vdp2_layer_t layer,
                                                  uint32_t byte_address);
```

It supports one-plane cell layers (all four plane_address entries equal). A
bitmap or multi-plane map returns `SAT_ERR_INVALID_ARG`; a base the cycle
planner cannot fetch returns `SAT_ERR_CAPACITY`; nothing changes on failure.

## Verification, not a golden image

Every 60 frames (and once at the freeze) the example reads the active 45x33
resident window back with the new `sat_vdp2_vram_read_words` and computes two
CRC-32s: one over `sat_stage_map2_cell()` (the model) and one over the actual
VDP2 VRAM. It also counts cells still holding the `0xFFFF` poison word. A
missing cell would show palette 15's magenta canary (tile 1023 is filled with
code 15), so the screenshot scan can catch an uninitialised tile without a
reference picture.

The harness passes `--dump-vram 0:4096 --dump-vram 8192:4096`, recomputes the
frozen window's CRC in Python and asserts that it equals both telemetry CRCs.
It watches MPABN0 (`0x05F80040`) and requires both ring base values
(`0x0000`/`0x0202`) to appear. It also checks the SCU-DMA and CPU counters:
long contiguous row runs go through SCU DMA, while short runs use CPU stores.

## Controls and timeline

The scene runs automatically; `START` exits. `sat_vdp2_layers_commit` is called
once per frame after VBlank. The camera wraps at 16,384 px, which is 32 map
periods (the map period is 512 px): the step across the ring seam is still
2 px, and the streamer rebuilds the inactive window within its advertised
capacity. No visible seam is produced.

The default acceptance run is 2,400 emulated frames. `-Stress` runs 36,000
frames, freezes the scene at 33,000 so the final CRC and the probe's VRAM dump
describe the same picture, and leaves the frozen road on screen while the probe
finishes. The checkpoints 600/1,200/1,800 are normal captures; the 33,000
capture is the frozen one. The stress run passed 8/8: 34,905 guest frames,
32,999 switches, frozen at base 0x04000, model/VRAM CRC both `0x517824CD`,
max pending 1,485 words, 756,900 staged/committed words, 192 SCU-DMA and
33,659 CPU transfers, no sentinel cells and no timeout/illegal interrupt.

## Acceptance

```powershell
.\harness\run-vdp2-dynamic-tile-stream.ps1 -Bios .\bios\saturn_bios_us.bin
.\harness\run-vdp2-dynamic-tile-stream.ps1 -Bios .\bios\saturn_bios_us.bin -Stress
```

The suite checks:

- `g_tile_stream` magic, init/storage, capacity/base/invalid-DMA refusals;
- no runtime capacity or write errors, every frame switched, 45x33 window;
- model CRC == VRAM CRC, zero sentinel cells, the same CRC recomputed from
  the Ymir VRAM dump after the stress freeze;
- MPABN0/MPCDN0 `0x0000` and `0x0202`, MPOFN NBG0 offset 0;
- SCU-DMA and CPU transfers both happened, with no timeout/illegal interrupt;
- centre-asphalt and left-grass pixels, no magenta canary, different early/late
  road frames, and no misaligned master/slave accesses.

## Limitations

- The live base switch is for one-plane cell maps; bitmap and multi-plane
  maps are refused rather than silently mispublished.
- The caller composes the double buffer from two `sat_stage_map2` instances;
  the hardware API only owns the base/scroll publication.
- The example uses a procedural map. It does not demonstrate CD/VFS chunk
  loading; the streaming scheduler itself is independent of the source.
- The evidence is Ymir at the pinned commit plus the harness patches. Physical
  hardware execution is PENDING (checklist row 34).
