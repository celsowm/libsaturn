# LibSaturn

**A modern bare-metal game-development library for the Sega Saturn.**

LibSaturn is a C-facing, C++-implemented runtime and game framework built directly on the Saturn hardware — without SGL or libyaul. It is no longer just a minimal 2D experiment: the project now spans a high-level 3D scene stack, VDP1/VDP2 rendering, animation, physics, storage and streaming, save data, RAM expansion, audio, asset tooling, and managed use of both SH-2 processors.

The goal is to make serious Saturn development feel like working with a coherent game-development platform while preserving explicit control over the machine.

> **Current direction:** reusable high-level APIs on top of small hardware-specific HALs, deterministic fixed-memory runtime systems, host-side asset baking, and Saturn-specific optimizations rather than hiding the console behind a desktop-style abstraction.

## Highlights

- **Bare-metal Saturn runtime** — startup, linker/runtime support, frame lifecycle, VBlank timing, ELF -> BIN -> ISO build flow.
- **VDP1 2D rendering** — sprites, scaled/distorted sprites, polygons, polylines, lines, rectangles, indexed textures, palettes, Gouraud shading and color calculation.
- **High-level 3D stack** — fixed-point math, meshes, models, cameras, transforms, scene submission, global painter ordering, materials, visibility helpers and VDP1 command generation.
- **3D asset pipeline** — OBJ/MTL/image and animated GLB processing with Saturn-aware texture baking, simplification, palette generation and pose baking.
- **3D animation** — compact baked pose streams, runtime decoding, playback helpers and optional parallel decode.
- **Scene/runtime helpers** — transform hierarchies, orbit/follow cameras, reusable surfaces, view caches, sprite animation, HUD and resource planning.
- **3D physics** — collision queries, spatial acceleration, world stepping, moving/tilting kinematic geometry, rolling-body support and continuous-collision paths.
- **2D physics** — deterministic fixed-step bodies, tile collision, sweeps, contacts, raycasts and uniform-grid broad phase.
- **VDP2 environments** — NBG0, RBG0, rotation/Mode-7-style ground, panoramic/infinite environments and color-calculation integration.
- **Voxel terrain** — reusable Saturn-oriented voxel terrain support.
- **Audio** — SCSP PCM S8/S16 playback, voices, looping, volume/pan/pitch, music streams and bounded streaming support.
- **CD and filesystems** — CD Block sector I/O, ISO9660/CDFS, logical file APIs and storage-neutral read paths.
- **Asset system** — logical asset handles, embedded and non-resident assets, typed loaders, bounded cache and cooperative prefetch.
- **Save data** — internal Backup RAM through the Saturn Boot ROM BUP services, including status, listing, read/write, verify, delete and format.
- **RAM expansion cartridges** — 1 MiB / 4 MiB detection and bounded allocation/cache support.
- **Dual SH-2 support** — low-level Master/Slave infrastructure plus a high-level bounded parallel executor with safe ownership and fallback semantics.
- **Host tooling and validation** — asset converters, model importers, manifest generation, host tests and automated modified-Ymir runtime probes.

## Why LibSaturn?

The Sega Saturn is powerful, but its architecture asks the programmer to coordinate several specialized processors and memory domains:

```text
                         +------------------+
                         |     Game Code    |
                         +---------+--------+
                                   |
                    +--------------v--------------+
                    |       LibSaturn APIs        |
                    | scene / physics / assets /  |
                    | audio / input / storage ... |
                    +--------------+--------------+
                                   |
             +---------------------+---------------------+
             |                     |                     |
      +------v------+       +------v------+       +------v------+
      | Master SH-2 |<----->| Slave SH-2  |       |     SCU     |
      +------+------+       +-------------+       +------+------+
             |                                           |
      +------v-------------------------------------------v------+
      |        VDP1 / VDP2 / SCSP / SMPC / CD / A-Bus         |
      +---------------------------------------------------------+
```

LibSaturn's approach is to expose reusable game-facing systems without pretending the hardware is something it is not. VDP1 remains a command-based quad renderer. VDP2 remains a powerful background processor. Shared SH-2 work has explicit ownership. Runtime systems favor caller-owned or bounded storage. Expensive content transformation is pushed to host tools when that produces a better Saturn runtime.

## Public API

Applications can include the umbrella header:

```c
#include <saturn/saturn.h>
```

or include only the subsystem headers they use.

The public API currently covers, among other areas:

| Area | APIs / capabilities |
|---|---|
| Core | app lifecycle, memory, time, video, formatting |
| 2D | geometry, surfaces, textures, rendering, sprite animation |
| 3D | math, meshes, models, animation, surfaces, renderer, scenes |
| Scene systems | transforms, cameras, material pools, face queues, view cache |
| VDP1 | sprites, primitives, textures, palettes, Gouraud, color calculation |
| VDP2 | backgrounds, RBG0 ground, environment helpers, color calculation |
| Physics | 2D/3D collision, spatial structures, bodies, physics world |
| Content | assets, resource plans, fonts, grids, HUD |
| Audio | PCM voices and streamed music |
| Storage | CD Block, CDFS, files, Backup RAM, RAM cart |
| Multiprocessing | low-level Dual SH-2 and high-level parallel tasks |

The public C ABI is intentionally separated from the internal C++ implementation so game code can remain small and predictable.

## 3D Rendering

LibSaturn provides a game-facing 3D layer above VDP1 rather than requiring every project to rebuild projection, sorting, material handling and command generation.

A typical application submits scene objects and lets the library prepare and globally order their faces before VDP1 submission. This matters on the Saturn because VDP1 has **no depth buffer**: ordering is part of the renderer, not an optional convenience.

The stack includes:

- fixed-point vectors, matrices and transforms;
- camera helpers;
- mesh/model abstractions;
- textured and colored faces;
- scene instances;
- material pooling;
- scene-wide painter ordering;
- clipping/culling helpers;
- Gouraud data;
- distance fading through Saturn color calculation;
- transform hierarchies;
- reusable geometry/surface helpers;
- animation integration;
- optional parallel geometry preparation.

## Saturn-Aware Asset Pipeline

Desktop-style UV meshes cannot be sent directly to VDP1. VDP1 distorted sprites are defined by four screen-space corners and do not provide arbitrary per-vertex UV coordinates.

LibSaturn therefore moves expensive adaptation offline:

```text
OBJ / MTL / PNG                skinned GLB
       |                           |
       +-----------+---------------+
                   |
                   v
          tools/import_model.py
                   |
        bake / simplify / quantize
                   |
                   v
       generated Saturn C/H asset
                   |
                   v
      model + scene + VDP1 runtime
```

The importer can bake source faces into VDP1-compatible rectangular textures, deduplicate textures, generate indexed palettes, enforce Saturn texture limits and produce runtime-ready model data.

For animated GLB assets, skeletal animation is evaluated on the host and emitted as compact baked poses. The Saturn runtime decodes those poses instead of running a general-purpose skeletal animation system every frame.

## Physics and Collision

The physics stack is deterministic and designed around Saturn constraints.

### 2D

- AABB and circle tests;
- contacts and raycasts;
- sweeps;
- fixed-step body movement;
- tile collision;
- caller-owned uniform-grid broad phase.

### 3D

- spheres, AABBs, planes, quads and mesh queries;
- spatial acceleration;
- body/world stepping;
- moving kinematic geometry;
- translation-relative continuous collision;
- bounded tilting kinematic meshes;
- pose-relative contacts;
- scene-renderer integration for physical transforms.

Core math uses 16.16 fixed point, with hardware-aware integer paths where appropriate.

## Dual SH-2 and Parallel Runtime

LibSaturn exposes both levels of Saturn multiprocessing.

### Low-level Dual SH-2 HAL

`saturn/dual_sh2.h` provides explicit Slave lifecycle, signaling, mailbox and shared-state infrastructure for code that needs direct control.

### High-level executor

`saturn/parallel.h` provides a bounded Saturn-specific task runtime:

- fixed caller-provided queue;
- generation-safe handles;
- Master, Slave and AUTO policies;
- cancellation and timeout semantics;
- explicit buffer ownership;
- cache-safe shared-memory contracts;
- Master fallback;
- worker failure handling.

It is intentionally **not** a desktop thread pool. Hardware ownership remains explicit, and tasks that touch VDP/SCU/SMPC/CD/SCSP registers stay Master-owned.

Current integrations include animation decode and 3D geometry preparation. AUTO policy is driven conservatively by measured behavior rather than assumed multiprocessor speedups.

See [`docs/PARALLEL_RUNTIME_ARCHITECTURE.md`](docs/PARALLEL_RUNTIME_ARCHITECTURE.md) and [`docs/PARALLEL_RUNTIME_BENCHMARKS.md`](docs/PARALLEL_RUNTIME_BENCHMARKS.md).

## VDP2 and Saturn-Specific Environments

LibSaturn is not limited to drawing everything as VDP1 polygons.

The VDP2 stack includes NBG0 and RBG0 support, scrolling, priorities, tiled backgrounds, rotation parameters, coefficient control and reusable RBG0 ground/environment helpers. Examples use these capabilities for panoramic skies and effectively infinite Mode-7-style ground.

This allows game scenes to reserve VDP1 command budget for objects while VDP2 handles large backgrounds and ground planes in parallel with the sprite processor.

## Audio

The SCSP runtime supports practical PCM-oriented game audio:

- signed 8-bit and 16-bit PCM;
- multiple voices;
- looping;
- volume;
- pan;
- pitch;
- voice statistics/stealing;
- resident sounds;
- bounded music streams;
- refill from storage-neutral file backends.

See [`docs/SCSP_AUDIO_STREAMING_GUIDE.md`](docs/SCSP_AUDIO_STREAMING_GUIDE.md).

## CD, Files and Asset Streaming

LibSaturn has a layered storage path instead of assuming all content is compiled into the executable:

```text
CD Block
   |
   v
ISO9660 / CDFS
   |
   v
logical file API
   |
   v
asset handles
   |
   +--> bounded cache
   +--> cooperative prefetch
   +--> streamed music
   +--> typed resident loaders
```

The CD Block path provides synchronous 2048-byte sector reads and a storage-neutral device adapter. CDFS resolves ISO9660 content, while the logical file/asset layers allow higher-level code to avoid depending on the physical source of the bytes.

## Save Data and RAM Expansion

### Internal Backup RAM

The save API wraps the Saturn Boot ROM BUP services and exposes:

- initialization/status;
- free-space inspection;
- directory listing;
- read;
- write/overwrite;
- verify;
- delete;
- explicit format.

Initialization never silently formats save memory.

### 1 MiB / 4 MiB RAM cartridges

The RAM-cart layer supports expansion-cartridge detection, two-bank arenas, bounded logical buffers and optional use as asset-cache/VFS backing.

See [`docs/RAM_EXPANSION_CARTRIDGE.md`](docs/RAM_EXPANSION_CARTRIDGE.md).

## Examples

The repository contains focused examples and larger integration demos covering areas such as:

- basic 2D rendering and input;
- textured sprites and fonts;
- VDP1/VDP2 composition;
- textured 3D models;
- animated 3D models;
- physics and collision;
- RBG0 infinite environments;
- CD Block and streamed audio;
- Dual SH-2 communication;
- the high-level parallel runtime;
- larger scene/gameplay experiments used to drive reusable API design.

Build an example with:

```bash
make EXAMPLE=<example_name>
```

On Windows, the helper can launch examples through the configured emulator:

```powershell
.\run-example.ps1 <example_name> -Emulator mednafen -BiosProfile auto
```

## Quick Start on Windows

Windows 10/11 is supported through MSYS2, with PowerShell wrappers so the normal workflow does not require manually living in a Bash shell.

Prepare the development environment:

```powershell
.\scripts\bootstrap-dev.ps1
```

or bootstrap host dependencies and the SH-2 toolchain:

```powershell
.\scripts\bootstrap-msys2.ps1 full
```

Then build:

```bash
make
```

Typical outputs include:

```text
build/mvp.elf
build/mvp.bin
build/mvp.iso
build/mvp.cue
build/libsaturn.a
```

The SH-2 toolchain is based on `sh2eb-elf-gcc`.

## Validation

Host tests:

```bash
make test
```

The repository also uses a modified Ymir harness for automated Saturn runtime validation:

```powershell
.\harness\run-harness.ps1 runtime_2d -Bios .\bios\saturn_bios_us.bin -Frames 120 -BootFrames 90
.\harness\run-harness.ps1 runtime_3d -Bios .\bios\saturn_bios_us.bin -Frames 120 -BootFrames 90
.\harness\run-harness.ps1 cd_streaming_jukebox -Bios .\bios\saturn_bios_us.bin -Frames 300 -BootFrames 90
```

The harness boots the configured Saturn BIOS, injects the built program and probes the running runtime. Direct injection is a runtime validation path and should not be confused with proving a real optical-disc boot.

## Project Structure

```text
include/saturn/   Public C API
src/core/         Runtime, startup and memory
src/graphics/     2D, 3D, VDP1 and VDP2 systems
src/physics/      Collision, spatial structures and physics
src/audio/        Playback and streaming
src/input/        Input runtime
src/storage/      CD, files, saves and cartridge services
src/resources/    Asset/resource management
src/hal/          Hardware-facing implementations
examples/         Focused examples and integration demos
tools/            Asset/model/build-time utilities
scripts/          Bootstrap, build and automation helpers
harness/          Automated Saturn runtime validation
docs/             Architecture, hardware notes and engineering plans
```

The intended layering is:

```text
game / compatibility layer
          |
          v
high-level LibSaturn API
          |
          v
runtime subsystem
          |
          v
hardware abstraction layer
          |
          v
Saturn hardware / BIOS services
```

Hardware register access belongs in the HAL. Reusable algorithms belong in their owning subsystem. Game-facing code should not need to reproduce hardware protocol or renderer plumbing that the library can own once.

## Current Scope

LibSaturn already covers a meaningful portion of the systems required by small and medium-sized Saturn games, but it does **not** claim complete Saturn hardware coverage.

Areas that remain incomplete or future-facing include broader Saturn peripheral support, additional VDP2 layers/raster effects, SCU DMA/DSP, richer SCSP DSP/synthesis, a resident 68000 audio service, generic A-Bus devices, external Backup Memory cartridges, NetLink and MPEG hardware.

For a detailed subsystem-by-subsystem audit, see [`docs/LIBSATURN_CURRENT_COVERAGE.md`](docs/LIBSATURN_CURRENT_COVERAGE.md).

## Design Principles

LibSaturn favors:

- **Saturn-native design** over pretending the console is a PC;
- **high-level reusable APIs** over example-specific hardware code;
- **bounded and caller-owned memory** over hidden allocation;
- **deterministic behavior** over opaque runtime machinery;
- **offline preprocessing** when it saves Saturn CPU/RAM/VRAM;
- **explicit hardware ownership** in multiprocessor code;
- **one canonical implementation path** instead of duplicated example logic;
- **measurement and validation** instead of theoretical performance claims.

## Documentation

Useful starting points:

- [Current hardware/runtime coverage](docs/LIBSATURN_CURRENT_COVERAGE.md)
- [Parallel runtime architecture](docs/PARALLEL_RUNTIME_ARCHITECTURE.md)
- [Parallel runtime benchmarks](docs/PARALLEL_RUNTIME_BENCHMARKS.md)
- [3D physics world](docs/PHYSICS3_WORLD.md)
- [Transform hierarchy](docs/TRANSFORM3D_HIERARCHY.md)
- [RAM expansion cartridge](docs/RAM_EXPANSION_CARTRIDGE.md)
- [SCSP audio streaming](docs/SCSP_AUDIO_STREAMING_GUIDE.md)
- [Transparency and alpha](docs/TRANSPARENCY_ALPHA.md)

The repository also includes Saturn hardware reference material and engineering plans for subsystems that are still evolving.

---

LibSaturn is an active exploration of how far a clean, modern, reusable game-development stack can push the Sega Saturn without giving up the character of the hardware.
