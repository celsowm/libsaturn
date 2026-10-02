# Using and packaging LibSaturn

LibSaturn is an installable library: a game project consumes it without copying
any LibSaturn source. The design and rationale are in
[PACKAGING_AND_LIBRARY_DISTRIBUTION_PLAN.md](PACKAGING_AND_LIBRARY_DISTRIBUTION_PLAN.md);
this page is the operating manual.

> **One rule:** CMake defines what an installed LibSaturn is. Conan, release
> archives and any future package manager only transport that definition.

## The consumer experience

```cmake
cmake_minimum_required(VERSION 3.24)
project(my_game LANGUAGES C)

find_package(LibSaturn 0.1 CONFIG REQUIRED)

add_executable(my_game src/main.c)
target_link_libraries(my_game PRIVATE LibSaturn::Saturn)
libsaturn_configure_executable(my_game)
libsaturn_add_binary(my_game MAX_BYTES 983040)   # optional: flat 0.BIN image
libsaturn_add_disc(my_game ROOT_DIRS ${CMAKE_SOURCE_DIR}/disc)  # optional: ISO/BIN/CUE
```

```c
#include <saturn/saturn.h>
```

Configure with the SH-2 toolchain file and the prefix LibSaturn was installed
to:

```sh
cmake -S . -B build -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=<prefix>/share/libsaturn/toolchains/sh2eb-elf.cmake \
      -DCMAKE_PREFIX_PATH=<prefix>
```

Any toolchain file that selects `sh2eb-elf-gcc/g++`, sets
`CMAKE_SYSTEM_NAME Generic` and `CMAKE_SYSTEM_PROCESSOR sh2` works;
`cmake/toolchains/sh2eb-elf.cmake` in the repository is the reference one.

### Targets

| Target | Contents |
|---|---|
| `LibSaturn::Saturn` | **What games link.** The complete runtime: Core + Startup with the correct link semantics. |
| `LibSaturn::Core` | `libsaturn.a`, the reusable runtime, headers and the ABI/compile options every translation unit must share (`-m2 -mb -ffreestanding ...`). |
| `LibSaturn::Startup` | `libsaturn_startup.a`: `crt0`, `ip_stub`, `slave_entry`. Linked whole-archive because nothing references these objects from C. |

### Functions

`libsaturn_configure_executable(<target> [MAP_FILE <path>] [NO_CTOR_CHECK])`
owns the Saturn link contract: `-m2 -mb -nostdlib`, the installed `saturn.ld`,
`--gc-sections`, a `.map` file, `.elf` suffix, and a post-link check that fails
the build if the program contains static constructors (`crt0` never runs them,
so such a global would silently stay zero). No Python is needed.

`libsaturn_add_binary(<target> [OUTPUT <path>] [MAX_BYTES <n>])` converts the ELF
into the flat image the BIOS loads.

`libsaturn_add_disc(<target> [NAME <base>] [OUTPUT_DIR <dir>] [IP_PROFILE current|safe]
[IP_TEMPLATE <path>] [ROOT_DIRS <dir>...] [ROOT_FILES <file>...] [DEPENDS ...] [ALL])`
adds the custom target `<target>_disc`, producing `<base>.iso`, `.bin` and `.cue`
from the linked program. It runs the same checked scripts the Makefile uses (shipped
in `share/libsaturn/tools`, with the default IP template in `share/libsaturn/boot`),
and needs a Python 3 interpreter plus `mkisofs`, `genisoimage` or `xorrisofs`.
`ROOT_DIRS` contents are copied into the disc root next to `0.BIN` and `IP.BIN`.

### Variables set by the package

`LibSaturn_VERSION`, `LibSaturn_INCLUDE_DIR`, `LIBSATURN_LINKER_SCRIPT`.

### Version compatibility

`find_package(LibSaturn <ver>)` is version-aware. While the major version is `0`,
a request is satisfied only by the same `0.<minor>` line (minor releases may
break the contract). From `1.0` on, any compatible `1.x`.

## Install tree

```text
<prefix>/
├── include/saturn/*.h
├── lib/libsaturn.a  lib/libsaturn_startup.a
├── lib/cmake/LibSaturn/{LibSaturnConfig,LibSaturnConfigVersion,LibSaturnTargets}.cmake
└── share/libsaturn/
    ├── linker/saturn.ld
    ├── cmake/LibSaturnFunctions.cmake  (+ post-link check scripts)
    ├── tools/*.py  tools/stage2d/*.py  (disc scripts and the stage2d asset compiler)
    ├── boot/ip_yaul_template.bin
    ├── toolchains/sh2eb-elf.cmake
    ├── sim/src/...                     (sources of LibSaturn::Sim2D)
    └── LICENSE
```

The prefix is relocatable; the config resolves everything relative to its own
location. The general host asset tools in the repository's `tools/` are not part
of the runtime package; the disc scripts and the generic stage2d compiler are, because
`libsaturn_add_disc()` and games that lay out 2D stages need them.

### Host-side extras

Two things in the package are never linked into firmware:

- `LibSaturn::Sim2D`: an interface target whose sources are the hardware-free 2D
  modules (Terrain2, Character2, Physics2, Path2, Follow Camera2D, entity_stream2, sprite
  clip, the task scheduler, math2d and the spatial grid), installed under
  `share/libsaturn/sim/` with the private headers they include. A game links it into a
  native test executable to run its own logic with the host compiler (the installed
  `libsaturn.a` is SH-2 code and cannot be linked on the host). It needs C++20, and
  compiles with the consumer's flags. The file list is `cmake/LibSaturnSim2D.cmake`;
  `tests/tools/test_package_sim2d.py` keeps it closed and hardware-free.
- `LIBSATURN_STAGE2D_TOOL`: the path of `stage2d_tool.py`, whose `stage2d/` package sits
  next to it in `LIBSATURN_DISC_TOOLS_DIR`. See `docs/STAGE2D_TOOLS.md`.

## Building the package from a checkout

```sh
cmake --preset saturn-release            # needs sh2eb-elf-gcc on PATH
cmake --build --preset saturn-release
cmake --install build/saturn-release --prefix /path/to/prefix
```

Options (all `-D`):

| Option | Default | Meaning |
|---|---|---|
| `LIBSATURN_BUILD_RUNTIME` | `ON` | `OFF` = host-only configure (header tests with the native compiler). |
| `LIBSATURN_BUILD_TESTS` | `OFF` | Header smoke tests, manifest/flag-parity checks, the package-consumer test. |
| `LIBSATURN_ENABLE_PROFILE_METRICS` | `OFF` | Compile profiling counters into the runtime (`SAT_PROFILE_METRICS`). |
| `LIBSATURN_PARALLEL_TEST_FAULT` | `0` | Parallel executor fault injection (test builds only). |
| `LIBSATURN_BUILD_EXAMPLES` / `LIBSATURN_BUILD_HOST_TOOLS` | `OFF` | Reserved. Examples and asset tools are built by the Makefile. |

Presets: `saturn-debug`, `saturn-release`, `host-tests`, `package-consumer`
(contributor convenience; consumers use a plain toolchain file).

The runtime is always compiled with `-O2` regardless of build type: it relies on
GCC constant-folding of MMIO references.

## Conan 2

Conan is the first package-manager integration. It models the Saturn as a
separate *host* machine, so settings are extended once:

```sh
conan config install packaging/conan/config      # adds os=baremetal, arch=sh2eb, profile saturn-sh2eb
conan create . --profile:host=saturn-sh2eb --profile:build=default
```

Consumer `conanfile.txt`:

```ini
[requires]
libsaturn/0.1.0

[generators]
CMakeDeps
CMakeToolchain
```

```sh
conan install . --profile:host=saturn-sh2eb --profile:build=default --build=missing
cmake --preset conan-release
```

The recipe builds and installs the repository's CMake project and republishes the
installed package unchanged (`cmake_find_mode = none`). Your CMake keeps using
`find_package(LibSaturn CONFIG REQUIRED)` and `LibSaturn::Saturn`; Conan only puts
the package folder on `CMAKE_PREFIX_PATH`. The recipe never restates compiler
flags or source lists, and reads its version from the repository `VERSION` file.

`sh2eb-elf-gcc` and Ninja must be on `PATH` of the machine running Conan (the
saturn profile selects the Ninja generator and the cross compiler).

Recipe options: `libsaturn/*:profile_metrics=True`,
`libsaturn/*:parallel_test_fault=<n>`.

### Publishing

```sh
conan create . --profile:host=saturn-sh2eb --profile:build=default   # runs test_package
conan remote add <name> <url>
conan upload "libsaturn/0.1.0" -r <name>
```

`test_package/` builds `tests/package_consumer` through Conan and runs the same
contract checks as the plain-CMake gate, so nothing is uploaded that has not been
consumed once from a clean prefix.

## Release process

1. Edit `VERSION` (single source), run `cmake -P cmake/SyncVersion.cmake`
   to refresh `include/saturn/version.h`.
2. Update `CHANGELOG.md`.
3. `scripts/test-package.ps1 -Conan` (or `scripts/test-package.sh --conan`).
4. Tag `v<VERSION>` and push. The `release` workflow rebuilds the toolchain,
   re-runs the package-consumer and Conan gates, then publishes the source
   archive, a prebuilt prefix archive and `SHA256SUMS`. The tag must equal `VERSION`.

## Gates

| Gate | What it proves |
|---|---|
| `host-tests` preset | every public header compiles standalone (C) and from C++20; headers do not include `src/`; the source manifest matches `src/`. |
| `package-consumer` preset | the SH-2 build, `cmake --install`, then a consumer using **only** the prefix: version-aware `find_package`, no source-tree include leaks, installed linker script, startup symbols (`_start` at `0x06004000`), post-link constructor guard (accepts the consumer, rejects a program with a static constructor). |
| Conan `test_package` | the same consumer and checks through Conan-generated metadata. |
| `makefile_flag_parity` | the Makefile and CMake compile the runtime with the same ABI/language flags. |

## Source of truth

| Thing | Where it is edited |
|---|---|
| Version | `VERSION` (then `cmake -P cmake/SyncVersion.cmake`) |
| Runtime source list | `cmake/LibSaturnSources.cmake` (the Makefile reads it via `tools/library_sources.py`) |
| Public API | `include/saturn/` (anything there is a compatibility promise) |
| Link contract | `cmake/LibSaturnFunctions.cmake`, `src/core/startup/saturn.ld` |

Adding a runtime source file means adding one line to the manifest;
`tests/tools/test_library_source_manifest.py` fails if it and `src/` disagree.

## vcpkg

Not supported. See [VCPKG_FEASIBILITY.md](VCPKG_FEASIBILITY.md).
