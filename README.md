<div align="center">

# LibSaturn

### A hardware-native SDK for Sega Saturn game development.

**Modern C APIs. Real Saturn hardware. Your level of control.**

A native foundation for building Sega Saturn games — from direct VDP1/VDP2 work to 3D scenes, animation, physics, audio, asset streaming, and parallel SH-2 execution.

**Bare metal · No SGL dependency · No libyaul dependency · No mandatory game engine**

[Get started](#get-started) · [Explore the SDK](#what-you-can-build) · [Use in your game](#use-libsaturn-in-your-own-game) · [Examples](#examples) · [Documentation](#documentation)

</div>

---

## Built for the Saturn, not around it

The Sega Saturn is not a conventional single-CPU, single-GPU machine. It combines two SH-2 processors with specialized graphics, sound, I/O, and storage hardware. Getting the most out of it means understanding those capabilities rather than abstracting them away.

**LibSaturn is a bare-metal SDK with a public C API and a C++ implementation.** It supplies the runtime, hardware-facing interfaces, reusable gameplay systems, and build tools needed to create native Saturn software. It does **not** run on top of Sega Game Library (SGL) or libyaul.

**High-level convenience and low-level control are choices, not consecutive layers your game must pass through.** Use a scene API for a 3D world, issue VDP1 primitives directly, configure VDP2 effects, manage the Slave SH-2, or combine those approaches in a single project. Each API has its own ownership and hardware constraints; the SDK does not pretend they disappear.

| Choose an approach | Work with | Examples of public interfaces |
| --- | --- | --- |
| **Game-oriented** | Scenes, characters, physics, cameras, sprites, asset handles | `saturn/scene.h`, `saturn/character2.h`, `saturn/physics3_world.h` |
| **Subsystem-oriented** | VDP1 commands, VDP2 layers, audio, CD, input, parallel jobs | `saturn/vdp1.h`, `saturn/vdp2_layers.h`, `saturn/audio.h`, `saturn/parallel.h` |
| **Hardware-facing** | SH-2 state, SCU DMA/DSP, interrupts, peripheral and storage services | `saturn/sh2.h`, `saturn/dma.h`, `saturn/scu_dsp.h`, `saturn/irq.h`, `saturn/cd_block.h` |

There is no separate desktop-style rendering engine between your application and the console. The library's rendering and runtime code is specifically implemented for Saturn hardware, with bounded resources, explicit ownership, and host-side preprocessing where it saves work on the machine.

## What you can build

LibSaturn is more than a set of register definitions. Its subsystems are designed to work together in actual game projects.

### Graphics, rendering, and presentation

- **VDP1 2D:** textured sprites, scaling and distortion, polygons, polylines, lines, indexed textures, LUT4, palettes, Gouraud shading, color calculation, sprite atlases, and batched command submission.
- **3D rendering:** fixed-point math, models, cameras, transforms, scene submission, materials, clipping, culling, lighting helpers, and global painter ordering for VDP1's no-Z-buffer architecture.
- **Animation and characters:** skeletal clips, interpolation, crossfades, root motion, attachments, CPU skinning, character assets, and dual SH-2-assisted workloads.
- **VDP2 composition:** normal scroll layers (NBG0–NBG3), rotation backgrounds (RBG0/RBG1), VRAM cycle-aware configuration, windows, mosaic, line scrolling, per-line effects, color calculation, priority, and sprite-shadow facilities.
- **Saturn-specific environments:** rotation-ground helpers, panoramic backgrounds, TATE-oriented rendering, and experimental/targeted voxel-terrain examples.

### Gameplay systems

- **2D gameplay:** deterministic collision and physics, tile terrain, character movement, paths, camera following, stage maps, entity streaming, and sprite clips.
- **3D gameplay:** collision queries, spatial acceleration, physics-world stepping, moving/tilting geometry, and scene-integrated transforms.
- **Input:** digital pads, analog/3D pads, multitap, mouse, keyboard, device events, and asynchronous polling through SMPC-backed services.
- **Resource control:** caller-owned storage, bounded queues and caches, logical asset handles, and predictable runtime budgets.

### Audio, storage, and system hardware

- **SCSP audio:** PCM voices, playback controls, streamed music, and DSP-based echo/reverb.
- **68000 sound driver:** a resident program for scheduled, timestamped SCSP register writes, independent of the SH-2 game loop's frame length.
- **Disc and file access:** CD Block, ISO9660/CDFS, logical file APIs, cooperative prefetch, and a bounded disc scheduler coordinating data and music streams.
- **Save and expansion:** internal Backup RAM via Boot ROM services, save inspection and verification, and 1 MiB / 4 MiB RAM-cartridge support.
- **Parallelism and coprocessors:** low-level Master/Slave SH-2 APIs, a bounded task executor with explicit cache/ownership rules, SCU DMA, and SCU DSP access and transform support.

### Tools that belong in the workflow

Import and preprocess content **before** it reaches the console. LibSaturn includes host tools for OBJ/MTL and animated GLB assets, VDP1-compatible textures and palettes, skeletal/pose data, sprite atlases, stage data, disc images, and runtime validation.

The library is also installable: a game can consume its C headers, startup objects, linker configuration, and runtime through a **CMake package**, without vendoring LibSaturn's implementation. Conan 2 is supported as a distribution route for that package.

> **Coverage is specific, not universal.** These are implemented facilities, not a claim that every Saturn mode or peripheral is supported. Some subsystems, workloads, and devices remain partial or experimental. See the [coverage audit](docs/LIBSATURN_CURRENT_COVERAGE.md) and [hardware acceptance checklist](docs/HARDWARE_ACCEPTANCE_CHECKLIST.md).

## A first Saturn program

Here is a small native C program using the frame helpers and a direct VDP1 rectangle draw. It is intentionally simple: no engine setup, scene graph, or external graphics library.

```c
#include <saturn/app.h>
#include <saturn/color.h>
#include <saturn/vdp1.h>

int main(void) {
    if (sat_app_init_default() != SAT_OK) return 1;

    for (;;) {
        sat_pad_state_t pad = {0};

        if (sat_app_frame_begin(SAT_COLOR_BLUE, SAT_COLOR_BLACK, &pad) != SAT_OK)
            return 1;

        if (sat_draw_rect_screen(116, 68, 88, 88, SAT_COLOR_RED) != SAT_OK)
            return 1;

        if (sat_app_frame_end() != SAT_OK) return 1;
        if (pad.pressed & SAT_PAD_START) break;
    }

    return 0;
}
```

The public API is C-compatible. Internally, the library implements Saturn-specific services in C++ with a freestanding SH-2 toolchain; game projects need not use C++ or adopt its internal architecture.

For a complete example with assets and build configuration, start with [`examples/hello_world`](examples/hello_world). To explore the canonical high-level 3D frame contract, see [`saturn/scene.h`](include/saturn/scene.h).

## Get started

### Build an example from this repository

You need the `sh2eb-elf` cross-toolchain and the host dependencies used by the build and asset tools. The Makefile builds the library and the selected example:

```sh
git clone https://github.com/celsowm/libsaturn.git
cd libsaturn
make EXAMPLE=hello_world
```

The build produces a Saturn ELF, executable image, and disc artifacts (including BIN/CUE) under `build/`. Open the matching `.cue` in your emulator or use the repository's launcher. The `.iso` is an intermediate image; **the distributed disc track is the `.bin` plus `.cue` pair**.

### Windows setup

On Windows 10/11, the PowerShell bootstrap configures the MSYS2-based development environment. It can also prepare the optional emulator tools:

```powershell
.\scripts\bootstrap-dev.ps1
.\build-example.ps1 -Example hello_world
.\run-example.ps1 hello_world -Emulator mednafen -BiosProfile auto
```

To install only the build environment, without the emulator download stage:

```powershell
.\scripts\bootstrap-dev.ps1 -SkipEmulators
```

Linux and macOS can run the native host tests; building Saturn binaries on those hosts requires an installed, working SH-2 cross-toolchain and the corresponding host build tools. The Windows bootstrap is not a universal installer for other platforms.

### Test the project

```sh
make test
```

The host suite tests portable runtime algorithms and tooling. Hardware-facing acceptance is handled separately through emulator harnesses; see [Validation](#validation-and-hardware-status).

## Use LibSaturn in your own game

LibSaturn is designed to be **linked as an installed SDK**, not copied into every game's source tree. A standalone game can consume the provided CMake target:

```cmake
cmake_minimum_required(VERSION 3.24)
project(my_saturn_game LANGUAGES C)

find_package(LibSaturn 0.1 CONFIG REQUIRED)

add_executable(my_saturn_game src/main.c)
target_link_libraries(my_saturn_game PRIVATE LibSaturn::Saturn)
libsaturn_configure_executable(my_saturn_game)
libsaturn_add_disc(my_saturn_game)
```

Build and install a local SDK prefix from a LibSaturn checkout:

```sh
cmake --preset saturn-release
cmake --build --preset saturn-release
cmake --install build/saturn-release --prefix /path/to/libsaturn-sdk
```

Then configure your game with the SH-2 toolchain and installation prefix:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/libsaturn-sdk/share/libsaturn/toolchains/sh2eb-elf.cmake \
  -DCMAKE_PREFIX_PATH=/path/to/libsaturn-sdk
cmake --build build --target my_saturn_game_disc
```

**Conan 2** is also available for managing the installed package and SH-2 profile. See [Packaging and distribution](docs/PACKAGING.md) for the exact install layout, variants, host-side `LibSaturn::Sim2D` target, Conan configuration, and disc-building options.

### Independent game projects

- [ikemen-saturn](https://github.com/celsowm/ikemen-saturn) — an external consumer of the installed `LibSaturn::Saturn` package, with a disc build managed through CMake.
- [high-speed-platformer-saturn](https://github.com/celsowm/high-speed-platformer-saturn) — an acceptance project for the reusable 2D runtime, including host-simulated gameplay through `LibSaturn::Sim2D`.

These are separate games/projects, not required components of the SDK.

## Examples

The repository includes focused hardware examples, system tests, and larger demonstrations. Good entry points include:

| Example | Explore |
| --- | --- |
| [`hello_world`](examples/hello_world) | Basic app lifecycle and text rendering |
| [`runtime_2d`](examples/runtime_2d) | Reusable 2D frame/runtime systems |
| [`runtime_3d`](examples/runtime_3d) | Scene-based 3D rendering |
| [`dino_demo`](examples/dino_demo) | Imported 3D model and rendering pipeline |
| [`skeletal_animation_3d`](examples/skeletal_animation_3d) | Skeletons, clips, and skinning |
| [`killer_clown_demo`](examples/killer_clown_demo) | Two-character 3D gameplay and multiple subsystems in concert |
| [`bullet_hell_stress`](examples/bullet_hell_stress) | Sprite batching, TATE, fixed-step simulation, and disc streaming |
| [`vdp2_effects_demo`](examples/vdp2_effects_demo) | VDP2 effects and composition |
| [`parallel_runtime`](examples/parallel_runtime) | Master/Slave execution and bounded jobs |
| [`cd_streaming_jukebox`](examples/cd_streaming_jukebox) | Audio streamed from disc |
| [`scsp_dsp_demo`](examples/scsp_dsp_demo) | SCSP DSP effects |
| [`scu_dsp_demo`](examples/scu_dsp_demo) | SCU DSP operation |

Build a different example by changing the Makefile selection, for example `make EXAMPLE=bullet_hell_stress`. See the [`examples/`](examples/) directory for the complete collection and individual requirements.

## Engineering for the actual machine

LibSaturn's goal is to make sophisticated Saturn software easier to write **without inventing a fictional hardware model**:

- **VDP1 has no Z-buffer.** Its rendering path deals explicitly with face ordering, clipping, command budgets, and texture constraints.
- **VDP2 is a specialized compositor.** Its layers, VRAM fetch cycles, priorities, color operations, and rotation modes have real constraints; invalid configurations can be rejected rather than silently approximated.
- **Two SH-2s do not mean arbitrary shared-memory threads.** Parallel jobs have explicit ownership, cache synchronization, bounded work, and failure/fallback behavior.
- **The SCSP, SCU, CD Block, and SMPC are separate devices.** Their services are exposed with device-specific contracts instead of a generic desktop I/O facade.
- **Memory and bandwidth are budgets.** Static or caller-provided buffers, predictable capacity, offline baking, and measured hot paths are core design decisions.

A project can deliberately bypass higher-level helpers when a lower-level route fits its requirements. Conversely, using a high-level feature does not require each game to redo hardware startup, renderer sorting, CD scheduling, or asset adaptation.

For deeper implementation details, see the [parallel runtime architecture](docs/PARALLEL_RUNTIME_ARCHITECTURE.md), [renderer pipeline study](docs/RENDERER_PIPELINE_STUDY.md), and [VDP2 subsystem coverage](docs/LIBSATURN_CURRENT_COVERAGE.md).

## Validation and hardware status

LibSaturn includes host tests, package-consumer gates, and a modified-Ymir integration harness to exercise Saturn programs and inspect runtime behavior. Selected cases are also evaluated with Mednafen. Build output, game logic, emulator behavior, and physical console acceptance are **different levels of evidence**.

Several complex 3D workloads have measured performance limits; emulator submissions or frame counters must not be presented as universally achieved display frame rates. Physical Saturn acceptance remains incomplete for parts of the project. Performance reports document specific builds, scenes, and emulator setups.

- [Coverage by hardware block and SDK area](docs/LIBSATURN_CURRENT_COVERAGE.md)
- [Real-hardware acceptance checklist](docs/HARDWARE_ACCEPTANCE_CHECKLIST.md)
- [Fighting-game readiness and measured limitations](docs/FIGHTING_GAME_READINESS.md)
- [Parallel runtime benchmarks](docs/PARALLEL_RUNTIME_BENCHMARKS.md)
- [Shmup/bullet-hell performance and limitations](docs/SHMUP_BULLET_HELL_SUPPORT.md)

**Current version:** `0.1.0`. The SDK is under active development; public interfaces can evolve before a stable 1.0 release. Incomplete areas include selected advanced VDP2 modes, broader peripherals/expansion support, additional SCU/SCSP DSP programs, and unverified real-hardware behaviors. Refer to the coverage audit for the specific status of each feature.

## Project layout

| Directory | Purpose |
| --- | --- |
| [`include/saturn/`](include/saturn/) | Public C headers and SDK interfaces |
| [`src/`](src/) | Runtime implementation, systems, and hardware-facing code |
| [`examples/`](examples/) | Applications, hardware exercises, and integration demos |
| [`tools/`](tools/) | Host-side conversion, baking, and build utilities |
| [`cmake/`](cmake/) and [`packaging/`](packaging/) | Installed SDK, toolchain, and Conan distribution |
| [`tests/`](tests/) | Host-side regression and package tests |
| [`harness/`](harness/) | Emulator-driven Saturn runtime acceptance |
| [`docs/`](docs/) | Architecture, subsystem guides, hardware notes, and engineering studies |

## Documentation

**Start here**

- [Using and packaging LibSaturn](docs/PACKAGING.md)
- [Public API ownership](docs/PUBLIC_API_OWNERSHIP.md)
- [Current feature and hardware coverage](docs/LIBSATURN_CURRENT_COVERAGE.md)
- [Hardware acceptance checklist](docs/HARDWARE_ACCEPTANCE_CHECKLIST.md)

**Graphics and animation**

- [Skeletal animation and character runtime](docs/SKELETAL_ANIMATION_3D.md)
- [3D renderer and scene pipeline](docs/RENDERER_PIPELINE_STUDY.md)
- [VDP1 transparency and alpha](docs/TRANSPARENCY_ALPHA.md)
- [3D physics world](docs/PHYSICS3_WORLD.md)
- [Voxel terrain](docs/VOXEL_TERRAIN_PLAN.md)

**Systems and hardware**

- [Parallel SH-2 runtime](docs/PARALLEL_RUNTIME_ARCHITECTURE.md)
- [SCSP audio streaming](docs/SCSP_AUDIO_STREAMING_GUIDE.md)
- [RAM expansion cartridges](docs/RAM_EXPANSION_CARTRIDGE.md)
- [CD Block hardware notes](docs/sega_saturn_hardware/hard/cdblock/index.md)
- [Saturn hardware reference material](docs/sega_saturn_hardware/)

## Community and credits

LibSaturn draws on Sega Saturn hardware documentation and the work of the homebrew development community. Thanks to the **SegaXtreme Discord community** for testing, feedback, and shared knowledge.

Related projects consulted as references include [Jo Engine](https://github.com/Johannes-Fetz/JoEngine), [libyaul](https://github.com/yaul-org/libyaul), [SaturnMathPP](https://github.com/robertoduarte/SaturnMathPP), and [SaturnRingLib](https://github.com/ReyeMe/SaturnRingLib). They are **not** linked as dependencies of the LibSaturn runtime. [SGDK](https://github.com/Stephane-D/SGDK) has also been consulted for sound-driver design ideas.

The separate integration harness uses a pinned, modified [Ymir](https://github.com/StrikerX3/Ymir) emulator core under its own GPL-3.0 license boundary. Optional testing launchers support [Mednafen](https://mednafen.github.io/), [Kronos](https://github.com/FCare/yabause), and [Yaba Sanshiro](https://github.com/devmiyax/yabause); see [emulator setup](emulators/README.md).

LibSaturn itself is licensed under the **[MIT License](LICENSE)**. Third-party references, tools, and emulator components retain their own licenses.