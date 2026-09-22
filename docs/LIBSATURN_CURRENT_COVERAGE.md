# LibSaturn Current Coverage

This document is a snapshot of the hardware and runtime coverage currently exposed by LibSaturn.

**Snapshot date:** 2026-09-19
**Baseline:** `main` at `9d53694`

The goal is not to measure a percentage of the Sega Saturn hardware. Instead, this document answers a more useful question:

> Which Saturn subsystems already have a usable LibSaturn abstraction, which are only partially exposed, and which are still essentially unexplored by the library?

Bundled Sega hardware documentation does **not** count as implementation coverage. A subsystem is considered covered only when there is runtime code and, preferably, a public API, test, example, or tool that exercises it.

## Status legend

| Status | Meaning |
|---|---|
| **SUBSTANTIAL** | Usable public abstraction exists and the subsystem is already a meaningful part of LibSaturn. |
| **PARTIAL** | Useful functionality exists, but only a subset of the hardware or expected runtime abstraction is covered. |
| **MINIMAL** | Only enough support exists for bootstrap/basic operation. |
| **NOT EXPOSED** | No meaningful public runtime API was found in the snapshot. |
| **DOCS ONLY** | Hardware documentation exists in the repository, but there is no corresponding supported runtime subsystem. |

## Executive summary

LibSaturn is already strongest in the areas needed to render and run small-to-medium bare-metal games:

- bare-metal startup and frame lifecycle;
- VDP1 2D primitives and textured rendering;
- a growing VDP1-oriented 3D stack;
- VDP2 NBG0 and RBG0 support;
- sprite/VDP2 color-calculation support used by distance fading;
- fixed-point math, models, animation, collision, spatial and physics helpers;
- basic PCM playback through the SCSP;
- digital pad input;
- host-side asset conversion and model baking;
- ELF -> BIN -> ISO build flow.

The largest Saturn hardware areas still missing as first-class LibSaturn subsystems are:

- Backup RAM/save support;
- generic cartridge/A-Bus access beyond the volatile RAM-expansion driver;
- 3D Control Pad analog input and the broader Saturn peripheral family;
- higher-level Slave SH-2 scheduling/job execution;
- SCU DMA;
- SCU DSP;
- advanced SCSP DSP/effects/synthesis and a resident 68000 sound driver;
- broader SMPC system services;
- the remaining VDP2 layers and raster/line effects;
- NetLink/communications and optional MPEG hardware.

## Public API surface

The current umbrella header exposes the following subsystem headers:

```text
core.h
geometry2d.h
color.h
memory.h
time.h
surface.h
texture.h
render2d.h
math3d.h
mesh3d.h
model3d.h
anim3d.h
render3d.h
fade3d.h
video.h
input.h
app.h
audio.h
cd.h
cdfs.h
file.h
asset.h
cd_block.h
scene3d.h
fmt.h
font.h
grid.h
collide2d.h
spatial.h
physics.h
collide3d.h
vdp1.h
vdp1_color_calc.h
vdp2.h
vdp2_color_calc.h
```

This is a useful high-level picture of what LibSaturn currently treats as supported public runtime functionality.

## Coverage matrix

| Area | Status | Current coverage | Major gaps / next territory | Evidence |
|---|---|---|---|---|
| Bare-metal startup/runtime | **SUBSTANTIAL** | Startup, linker/runtime integration, initialization and game-oriented frame lifecycle. | More system services and richer platform introspection. | `src/core/startup`, `src/core/runtime`, `include/saturn/core.h`, `include/saturn/app.h` |
| Frame timing / VBlank | **SUBSTANTIAL** | Begin/end frame, VBlank wait, display-frame counter calibrated from SH-2 timing. | Broader interrupt/timer abstraction. | `include/saturn/video.h`, `src/hal/scu/scu.*` |
| VDP1 sprites | **SUBSTANTIAL** | Normal, scaled and distorted sprites; screen/native coordinate helpers. | Additional command/state abstractions and broader hardware modes. | `include/saturn/vdp1.h` |
| VDP1 primitives | **SUBSTANTIAL** | Polygon, polyline, line and rectangle helpers. | More generalized render-state control. | `include/saturn/vdp1.h` |
| VDP1 textures/palettes | **SUBSTANTIAL** | Indexed-8 texture upload, palette upload, texture VRAM management paths. | More source formats and runtime streaming/upload strategies. | `include/saturn/vdp1.h` |
| VDP1 Gouraud | **SUBSTANTIAL** | Managed Gouraud tables and Gouraud polygon/polyline/line drawing. | Broader integration with material/render abstractions. | `include/saturn/vdp1.h` |
| Sprite color calculation | **PARTIAL** | VDP1/VDP2 coordination for sprite priority/color-calculation ratios; distance-fade infrastructure. | Generalized blend/color-calc composition across more render paths. | `include/saturn/vdp1_color_calc.h`, `include/saturn/vdp2_color_calc.h`, `include/saturn/fade3d.h` |
| 3D math | **SUBSTANTIAL** | Fixed-point game/3D math abstractions used by renderer and gameplay systems. | SIMD-like/parallel acceleration paths where useful. | `include/saturn/math3d.h` |
| Mesh/model pipeline | **SUBSTANTIAL** | Mesh/model runtime plus offline OBJ/MTL/image baking for VDP1-compatible faces, with a caller-owned camera/model scene facade. | Runtime asset streaming, broader import formats and richer material model. | `include/saturn/mesh3d.h`, `include/saturn/model3d.h`, `include/saturn/scene3d.h`, `tools/import_model.py` |
| 3D renderer | **SUBSTANTIAL** | Dedicated render layer on top of VDP1-oriented geometry. | Further clipping, sorting, batching, material and multiprocessor work. | `include/saturn/render3d.h` |
| 3D animation | **SUBSTANTIAL** | Public animation subsystem integrated with the 3D stack. | More advanced animation tooling/runtime features as needed. | `include/saturn/anim3d.h` |
| Distance fade | **SUBSTANTIAL** | Public 3D distance-fade abstraction backed by Saturn sprite color calculation. | Additional policies and integration with future LOD/streaming systems. | `include/saturn/fade3d.h`, `include/saturn/vdp2_color_calc.h` |
| Collision / spatial / physics | **PARTIAL** | 2D and 3D collision helpers plus spatial/physics public modules. | Broader solver/body/scene features and optional future use of the low-level Slave API. | `include/saturn/collide2d.h`, `include/saturn/collide3d.h`, `include/saturn/spatial.h`, `include/saturn/physics.h` |
| VDP2 NBG0 | **PARTIAL** | NBG0 initialization, scroll, priority, enable/disable, indexed-8 tiled upload and map writes. | NBG1/NBG2/NBG3 and more generalized map/cell configuration. | `include/saturn/vdp2.h` |
| VDP2 RBG0 | **PARTIAL** | Bitmap configuration, rotation parameters, coefficient control, matrix/viewpoint/center/scaling helpers and Mode-7-style setup. | More complete rotation modes, coefficient workflows and advanced compositing. | `include/saturn/vdp2.h` |
| VDP2 advanced raster effects | **NOT EXPOSED** | No broad public abstraction for the full family of per-line/raster effects. | Line scroll, vertical cell scroll, line color, windows, mosaic and richer per-line effects. | No corresponding first-class public module in the current umbrella API. |
| VDP2 remaining normal backgrounds | **NOT EXPOSED** | Public normal-background API is centered on NBG0. | NBG1, NBG2, NBG3 and generalized layer configuration. | `include/saturn/vdp2.h` |
| Digital pad | **PARTIAL** | Held/pressed/released state and digital Saturn pad buttons. | Device discovery, multiple controller types, analog axes and richer port model. | `include/saturn/input.h` |
| 3D Control Pad / analog | **NOT EXPOSED** | No public analog-axis/device-type API. | Analog stick, analog triggers and automatic device identification. | Current `input.h` is digital-pad-oriented. |
| Multitap / peripheral family | **NOT EXPOSED** | No general peripheral framework. | Multitap, mouse, wheel, Mission Stick, Twin Stick, Virtua Gun and other SMPC peripherals. | No corresponding public module. |
| SMPC system services | **MINIMAL** | Current HAL covers digital pad access and sound CPU on/off operations. | RTC/time, region/language/system status, peripheral enumeration and other SMPC commands. | `src/hal/smpc.*` |
| SCU interrupts/frame support | **MINIMAL** | Interrupt initialization and frame/VBlank timing support. | General interrupt-controller API and broader SCU services. | `src/hal/scu.*` |
| SCU DMA | **NOT EXPOSED** | No first-class LibSaturn DMA API. | DMA channels, queued transfers, RAM<->VRAM/CRAM/SCSP workflows and asynchronous completion. | Sega SCU docs are present; no corresponding public runtime subsystem. |
| SCU DSP | **DOCS ONLY** | SCU DSP manuals are present in the repository. | Program loading, assembler/tooling integration, dispatch, synchronization and useful DSP kernels. | `docs/sega_saturn_hardware/hard/scu_` |
| SCSP PCM playback | **PARTIAL** | PCM S8/S16 sounds, resident sound data, bounded music streams, looping, voices, volume, pan, pitch and voice statistics/stealing. | Richer envelopes/modulation, effects, synthesis and long-run/CD hardware validation. | `include/saturn/audio.h`, `src/hal/scsp.*`, `src/audio/playback/music.cpp` |
| SCSP DSP / effects | **NOT EXPOSED** | Internal SCSP initialization touches DSP state, but there is no supported public DSP effects API. | Reverb, chorus/delay-style effects, mixer programs, DSP program loading and routing. | `src/hal/scsp.*`; Sega SCSP docs under `docs/sega_saturn_hardware`. |
| SCSP synthesis / envelopes / LFO | **NOT EXPOSED** | Public audio API is centered on PCM voices. | Hardware envelope generators, LFO/modulation and richer slot synthesis. | `include/saturn/audio.h` |
| 68000 sound CPU | **NOT EXPOSED** | Sound CPU can be switched by the SMPC HAL, but there is no resident sound-driver framework. | 68k driver loading, command queues, independent music/SFX scheduling and streaming coordination. | `src/hal/smpc.*` |
| CD Block runtime I/O | **PARTIAL** | Hardware-specific synchronous 2048-byte sector reads, bounded command polling, LBA/FAD conversion, a `sat_cd_device_t` adapter, plus modified-Ymir BIOS-harness validation of both a PVD probe and CD-resident PCM playback are exposed. | Authentication policy, asynchronous I/O, seek/read scheduling and richer error/status reporting. | `include/saturn/cd_block.h`, `src/hal/cd/block.cpp`, `examples/cd_block_probe`, `examples/cd_streaming_jukebox` |
| CDFS / VFS | **PARTIAL** | Read-only logical paths, bounded handles, caller-backed blobs, caller-owned `read_at` backends, ISO9660 PVD/directory lookup, CDFS-to-VFS file adapters, non-resident music refill and cooperative asset prefetch/cache through the VFS path are public. `cd_streaming_jukebox` stages public-domain PCM as ISO files and joins this complete route. | Transparent manifest-driven CD registration and a genuinely non-blocking storage backend remain outside the current synchronous CD contract. | `include/saturn/cd.h`, `include/saturn/cdfs.h`, `include/saturn/file.h`, `include/saturn/asset.h`, `src/storage/cd/filesystem.cpp`, `src/resources/assets.cpp`, `examples/cd_streaming_jukebox` |
| Asset streaming | **PARTIAL** | File reads support partial backend transfers; typed resident loading, bounded embedded/non-resident logical music streams, fixed-block cache, cache diagnostics and cooperative prefetch require no whole-file allocation. | Texture/model/map streaming and general typed non-resident loaders. | `include/saturn/file.h`, `include/saturn/asset.h`, `src/resources/assets.cpp`, `src/audio/playback/music.cpp` |
| Backup RAM / save data | **PARTIAL** | Public internal-Backup-RAM save API over the Boot ROM BUP library: init, status/free-space, directory listing, read, write/overwrite, verify, delete and explicit format. Mutating BIOS calls are protected by SMPC reset-disable/reset-enable critical sections. | Emulator persistence acceptance, Backup Memory cartridge support, RTC convenience metadata and optional versioned/CRC payload helpers. | `include/saturn/save.h`, `src/storage/save/api.cpp`, `src/hal/storage/backup.*`, `tests/host/test_save_api.cpp` |
| RAM cartridge | **PARTIAL** | Volatile 1 MiB / 4 MiB ID detection, two-bank arenas and offset-based logical buffers, optional cart-backed asset cache/VFS bridge, demo and host tests. | Emulator / physical-hardware acceptance; multi-bank cache and DMA tuning. | `include/saturn/ram_cart.h`, `src/hal/storage/ram_cart.cpp`, `src/storage/cartridge/api.cpp`, `docs/RAM_EXPANSION_CARTRIDGE.md` |
| Generic cartridge / A-Bus | **NOT EXPOSED** | No general A-Bus/cartridge framework. | ROM carts, expansion hardware, bus probing and safe mapped access abstractions. | No corresponding public module. |
| Slave SH-2 | **PARTIAL** | Public low-level lifecycle, dedicated Slave entry, FRT signaling, cache-through shared control block, directional one-slot mailbox, timeout/restart state, example and host protocol tests. | Interrupt-driven reception, bulk-buffer ownership helpers, physical Saturn validation and any higher-level scheduling remain outside this layer. | `include/saturn/dual_sh2.h`, `src/hal/dual_sh2/`, `src/hal/sh2/`, `examples/dual_sh2`, `docs/DUAL_SH2_LOW_LEVEL.md` |
| Runtime filesystem-independent asset API | **PARTIAL** | Bounded logical asset handles, metadata, caller-owned partial reads, typed texture/font/sound loaders, bounded non-resident music refill, fixed cache/prefetch service, and generated C registration for embedded/physical manifest entries are independent of the current storage representation. | Concrete RAM-cart backend and non-resident typed texture/model/map loaders. | `include/saturn/asset.h`, `src/resources/assets.cpp`, `src/audio/playback/music.cpp`, `tools/generate_asset_manifest.py`, `tools/generate_asset_registry.py` |
| Formatting / fonts / utility drawing | **PARTIAL** | Formatting, font and grid helpers are public. | Broader UI/text layout and asset-backed fonts if desired. | `include/saturn/fmt.h`, `include/saturn/font.h`, `include/saturn/grid.h` |
| NetLink / modem / communications | **NOT EXPOSED** | No runtime communication subsystem. | NetLink/modem/serial-style communication abstractions. | No corresponding public module. |
| MPEG Card | **NOT EXPOSED** | No runtime MPEG-card subsystem. | Detection, decode/control APIs and optional video playback pipeline. | No corresponding public module. |

## Hardware coverage by major Saturn block

### Master SH-2

The Master SH-2 is where essentially all current game/runtime code executes. LibSaturn already provides a useful C API over an internal C++ implementation, a fixed-point 3D/game stack, rendering, collision, animation and general frame control.

The main missing CPU-side architecture feature is not basic Master SH-2 execution, but **using the second SH-2 as a managed resource**.

### Slave SH-2

Current status: **PARTIAL — low-level infrastructure exposed; no scheduler or job system**.

A future LibSaturn abstraction should avoid forcing games to build their own cache/synchronization protocol. A useful target would be a small job system with explicit data ownership:

```c
sat_slave_init();
sat_job_submit(job_fn, data);
sat_jobs_wait();
```

Likely first workloads:

- animation transforms;
- collision broad/narrow phase;
- decompression;
- visibility/culling;
- world update batches;
- asset preparation that does not touch VDP registers directly.

### SCU

Current status: **MINIMAL** for interrupts/frame timing, otherwise largely unexplored.

The two major opportunities are:

1. **SCU DMA** — remove bulk memory transfer work from the SH-2s.
2. **SCU DSP** — expose the Saturn's dedicated calculation unit through a safe runtime/tooling layer.

The repository already contains Sega SCU documentation for DMA, interrupts and DSP, so the missing piece is implementation rather than reference material.

### VDP1

Current status: **SUBSTANTIAL**.

This is one of LibSaturn's strongest hardware areas. The public API already covers the primitives needed for 2D and a meaningful VDP1-based 3D renderer:

- normal sprites;
- scaled sprites;
- distorted sprites;
- polygons;
- polylines;
- lines;
- indexed textures and palettes;
- Gouraud shading;
- screen/native coordinate helpers;
- color-calculation integration;
- model/mesh/render abstractions built above the raw VDP1 layer.

The current 3D asset pipeline also handles one of the Saturn's important limitations: source UV-mapped faces are baked offline into VDP1-compatible rectangular face textures instead of pretending VDP1 supports arbitrary per-vertex UV coordinates.

### VDP2

Current status: **PARTIAL but increasingly capable**.

The strongest covered areas are NBG0, RBG0 and sprite color-calculation coordination. RBG0 already has a meaningful high-level path for infinite/Mode-7-style ground rendering.

The largest VDP2 territory still open is not basic background drawing, but the console's more unusual raster/composition features:

- NBG1/NBG2/NBG3;
- line scroll;
- vertical cell scroll;
- line color tables;
- windows;
- mosaic;
- more generalized priority/color-calculation composition;
- reusable per-scanline effects.

These are especially attractive because they can create Saturn-specific visual effects without spending VDP1 polygon budget.

### SCSP + Motorola 68000

Current status: **PARTIAL**.

LibSaturn already has a useful PCM-oriented API and direct SCSP slot support. That is enough for straightforward SFX playback, looping voices, pan, pitch and volume.

However, the Saturn sound subsystem is much larger than a PCM voice player. The main missing layers are:

- SCSP DSP effects;
- advanced slot envelopes and modulation;
- synthesis-oriented APIs;
- a resident Motorola 68000 sound driver;
- command queues between game CPU and sound CPU;
- streamed music/audio coordination.

A 68k-backed sound service would also let audio continue with less Master SH-2 involvement.

### SMPC and peripherals

Current status: **MINIMAL**.

Digital pad polling exists, but the current public input model is not yet a general Saturn peripheral model. There is no public concept of device type or analog axes.

Important unexposed devices/features include:

- 3D Control Pad;
- analog triggers;
- Multitap;
- mouse;
- Racing Wheel;
- Mission Stick;
- Twin Stick;
- Virtua Gun and other light-gun-style devices;
- broader SMPC status/system commands.

A generic device API here would also make future SDL/raylib compatibility layers much cleaner.

### CD Block

Current status: **PARTIAL**.

LibSaturn can build an ISO, but building an ISO and using the Saturn CD Block at runtime are separate capabilities.

The runtime now has a hardware-specific synchronous CD Block reader with a
storage-neutral `sat_cd_device_t` adapter, plus the bounded ISO9660/CDFS parser
and VFS `read_at` callback. The reader intentionally leaves disc
authentication to the BIOS/platform startup path. Non-resident music can use
that storage-neutral path once the relevant backend is mounted. The modified
Ymir BIOS harness now validates the CD Block transport by reading LBA 16 and
checking the ISO9660 `CD001` signature. Logical non-resident assets also have
a fixed four-block cache and a cooperative prefetch queue serviced by
`sat_asset_prefetch_update()`; no worker thread is implied.

A complete storage path could grow in layers:

```text
CD Block sector I/O
        |
        v
ISO9660 / CDFS
        |
        v
VFS / logical asset paths
        |
        v
cooperative read + prefetch + cache
        |
        v
texture / model / map / audio streaming
```

This would remove the assumption that important game assets need to be compiled into the executable or permanently resident in RAM.

### Backup RAM and cartridge expansion

Current status: **PARTIAL** for internal Backup RAM and for volatile RAM expansion. External persistent Backup Memory cartridge support remains separate and is not yet implemented.

The internal save path now wraps the Saturn Boot ROM BUP library rather than
reimplementing Sega's on-media allocation format. The public API can inspect
capacity, enumerate records, read/write/verify/delete them and explicitly
format the device. Initialization never formats automatically.

Two distinct areas remain separate in the architecture:

- **Backup RAM/save storage**: persistent game data, directory records, free-space handling and robust versioned saves.
- **RAM cartridge**: volatile expansion memory usable for caches/assets/game-specific data.

Backup Memory cartridge support belongs to the save subsystem but is deliberately
deferred until device detection and emulator/hardware acceptance are proven.
The volatile 1 MiB / 4 MiB RAM-expansion driver now provides detection, two-bank
allocations, bounded asset-cache backing and a VFS bridge. A later generic
cartridge/A-Bus layer may support other hardware without coupling it to saves.

## Host-side tooling coverage

LibSaturn's host tooling is already an important part of its effective hardware abstraction.

Current useful areas include:

- SH-2 toolchain/bootstrap scripts;
- ELF -> BIN -> ISO build flow;
- IP.BIN generation/build support;
- indexed-8 image conversion;
- OBJ/MTL/image import and VDP1-oriented texture baking;
- emulator launch/acceptance flows.

The most important tooling gaps are tied to the runtime gaps above. Examples:

- pack an asset tree into the ISO while preserving logical paths;
- generate streaming-friendly level chunks;
- bake Saturn audio formats and stream metadata;
- build/load SCU DSP programs;
- build/load a 68000 sound driver;
- generate VDP2 line/raster tables offline when appropriate.

## Biggest unexplored areas

From the current snapshot, the largest new capability domains are:

1. **CD Block + CDFS/VFS + streaming**
2. **Full input/peripheral framework**
3. **Higher-level use of the Slave SH-2**
4. **SCU DMA**
5. **Advanced SCSP + 68000 sound runtime**
6. **SCU DSP**
7. **Advanced VDP2 raster/layer features**
8. **External Backup Memory cartridge and RAM-cart DMA tuning**
9. **NetLink/communications and optional MPEG hardware**

This ordering is a practical engineering sequence, not a statement that later hardware is less interesting. The first items unlock capabilities that can be reused broadly by many games and by compatibility layers.

## Suggested architectural boundaries

Future work should avoid exposing raw Saturn hardware quirks directly through high-level game APIs when a reusable boundary is possible.

A useful layering model is:

```text
Game / compatibility layer
        |
        v
High-level LibSaturn subsystem
        |
        v
Runtime service / scheduler / allocator
        |
        v
HAL
        |
        v
Saturn registers / BIOS service / bus
```

Examples:

- `sat_asset_*` should not care whether bytes came from embedded data, CD or RAM cart.
- `sat_input_*` should expose devices/axes/buttons without making game code speak raw SMPC protocol.
- Future application subsystems may build on `saturn/dual_sh2.h`; this low-level layer intentionally does not define job semantics.
- `sat_dma_*` should own SCU DMA channel state and completion rules.
- `sat_audio_*` should be able to evolve from direct PCM voices toward 68k/DSP-backed services without invalidating game code.

## What does not count as coverage

For this document, none of the following alone marks a subsystem as implemented:

- a Sega manual copied into `docs/`;
- a register definition that is never part of a supported flow;
- a one-off register write during initialization;
- host tooling with no corresponding Saturn runtime path;
- an example-specific hack that has not become a reusable subsystem.

This distinction is important for SCU DSP, advanced SCSP functionality and several optional Saturn devices: the repository may already contain the documentation needed to implement them, but that is different from LibSaturn supporting them.

## Updating this document

When a subsystem changes meaningfully:

1. update its status in the coverage matrix;
2. add the relevant public header/HAL/example as evidence;
3. update the snapshot baseline SHA;
4. move the subsystem out of the unexplored list when it becomes a supported reusable path;
5. prefer describing the actual usable abstraction rather than individual register-level features.

The intent is for this file to remain a lightweight hardware/runtime coverage audit as LibSaturn grows from a bare-metal rendering/game library into a broader Sega Saturn development platform.
