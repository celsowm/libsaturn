# VDP2 Neon City (E12)

The final integration showcase: a rotating RBG0 neon sign, a streamed wet
street, a light pool built from a colour-calculation window plus the
line-colour screen, animated sign/rain colour offsets, and four NBG layers
for the skyline and foreground.

```powershell
.\build-example.ps1 vdp2_neon_city
.\run-example.ps1 vdp2_neon_city
.\harness\run-vdp2-neon-city.ps1 -Bios bios\saturn_bios_us.bin
.\harness\run-vdp2-neon-city.ps1 -Bios bios\saturn_bios_us.bin -Long
```

| Input | Action |
| --- | --- |
| B | Pause the street stream |
| R | Reset the camera and resume |
| START | Exit |

## Composition

| Layer | Content | Facility |
| --- | --- | --- |
| Back | night gradient | per-line back-screen table |
| NBG3 | distant skyline, lit windows | cell map, priority 1 |
| NBG2 | towers and neon signs | cell map, priority 2, colour offset A |
| RBG0 | rotating neon ring/sign | rotation parameter A, 16.16 matrix, transparent overflow |
| NBG1 | wet street | `sat_stage_map2` double-buffered 64x64 torus, priority 4 |
| NBG0 | rain streaks and side signs | transparent cell map, line scroll, colour offset B |
| Window W0 | moving light pool | colour-calculation window, line-colour screen, add mode |

The street uses two pattern-name rings at 0x40000 and 0x42000. Each field the
next camera view is staged into the inactive ring and committed while the
active one is displayed; the base and scroll are published for the next
VBlank commit. The active ring is never written, so the switch is tear-free.
The model/VRAM CRC is recomputed every 60 fields, and a 0xFFFF sentinel cell
would be visible if a resident cell were missing.

The rotating sign is a 64x64-cell map with a transparent background. Its
parameter set uses a 16.16 rotation matrix about the map centre; the sign is
a **hardware rotation**, not a pre-rotated animation. The light pool is an
**artistic approximation** of a wet reflection: VDP2 adds the line-colour
screen to the street inside the window. It is not a physically based
reflection and does not read the buildings above it.

## Evidence and limits

The harness checks the API verdicts, stream switches, model/VRAM CRC,
staged/committed word counts, sentinel cells, SCU-DMA and CPU transfers, the
NBG/RBG priorities, parameter mode A, line-colour insert, and pixel families
for sky, cyan neon, gold windows/road and the light pool. `-Long` runs the
3,600-field replay and completed 1,753 scene frames with 1,752 ring switches,
31,845 staged/committed words, 7,218 SCU-DMA and 30,759 CPU transfers, zero
sentinel cells and equal model/VRAM CRC. The worst table/streaming update was
27.5 ms per scene field (roughly 30 scene fields per second), and the VDP1
list held only its two mandatory setup commands, with no scene draws. These
are emulator timings, not hardware facts. Mednafen 1.32.1 window captures show
the skyline, both ring phases and the moving light pool. Physical hardware
remains **PENDING**.
