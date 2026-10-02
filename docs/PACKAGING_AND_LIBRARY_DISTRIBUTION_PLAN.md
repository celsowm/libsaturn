# LibSaturn Packaging and Library Distribution Plan

## Goal

Turn LibSaturn from a repository that builds its own examples into a reusable,
installable Sega Saturn development library that an external project can consume
without copying LibSaturn sources into the game repository.

The end-state should make this the normal consumer experience:

```cmake
find_package(LibSaturn CONFIG REQUIRED)

add_executable(my_game
    src/main.c
)

target_link_libraries(my_game PRIVATE LibSaturn::Saturn)
libsaturn_configure_executable(my_game)
```

A package manager should only be a delivery mechanism for that same installed
CMake package. Conan, a source checkout, a release archive, or another future
package manager must all expose the same headers, targets, linker contract and
runtime behavior.

This is deliberately a packaging/build-system refactor. It must not create a
second runtime architecture and must not duplicate the implementation already
owned by `include/saturn/` and `src/`.

---

## Current state

The repository is already close to a real library at the source-code level:

- public C ABI headers live under `include/saturn/`;
- implementation is organized below `src/`;
- implementation details are C++ while the public API is C-facing;
- the reusable runtime is already archived into `libsaturn.a`;
- application startup and the linker script are separate from the archive;
- examples consume the library rather than being the library implementation.

The largest remaining problem is distribution/build ownership:

- the root `Makefile` owns library compilation, examples, asset generation,
  startup, linking, ISO/BIN/CUE creation, host tests and validation in one file;
- there is no root CMake package;
- there are no install rules;
- there is no `LibSaturnConfig.cmake`;
- there is no package version contract;
- there is no Conan recipe/profile;
- there is no package-consumer test that builds outside the source tree;
- some example-only convenience API is currently inside the public include tree.

The refactor should preserve the current working Makefile while the package
build becomes independently testable.

---

## Distribution contract

The install tree should look approximately like this:

```text
<prefix>/
├── include/
│   └── saturn/
│       ├── saturn.h
│       ├── core.h
│       └── ...
├── lib/
│   ├── libsaturn.a
│   ├── libsaturn_startup.a
│   └── cmake/
│       └── LibSaturn/
│           ├── LibSaturnConfig.cmake
│           ├── LibSaturnConfigVersion.cmake
│           └── LibSaturnTargets.cmake
└── share/
    └── libsaturn/
        ├── linker/
        │   └── saturn.ld
        └── cmake/
            └── LibSaturnFunctions.cmake
```

Host asset tools are not part of the core package contract initially. They can
become a separate component/package after the runtime package is stable.

The installed public include syntax remains:

```c
#include <saturn/saturn.h>
```

Do not rename the public include prefix merely for packaging.

---

## CMake target model

Use a small set of explicit exported targets.

### `LibSaturn::Core`

The actual reusable static library currently produced as `libsaturn.a`.

Responsibilities:

- all reusable C/C++ runtime implementation under `src/`, excluding startup
  assembly that must be linked specially;
- public include directory;
- target compile requirements;
- no example sources;
- no generated game assets;
- no ISO creation policy.

### `LibSaturn::Startup`

A small static archive containing the Saturn startup assembly currently linked
as loose CRT objects:

- `src/core/startup/crt0.s`;
- `src/core/startup/ip_stub.s`;
- `src/core/startup/slave_entry.s`.

The package must ensure these objects are retained when linked. The complete
runtime target/helper must apply whole-archive semantics or another verified
mechanism so entry/startup symbols are never discarded because they reside in
an archive.

### `LibSaturn::Saturn`

An INTERFACE target representing the complete game runtime.

It should carry the consumer-facing link relationship between the core runtime
and startup support, while the executable helper owns executable-specific
linker options.

External games should normally link this target rather than link
`LibSaturn::Core` directly.

---

## Root CMake project

Add a root `CMakeLists.txt` and make it the canonical install/package
description.

Do not immediately delete the Makefile. During migration:

- CMake owns the reusable library and installation contract;
- the existing Makefile remains available for current example/ISO workflows;
- both builds must compile the same implementation sources;
- duplicated source lists must be eliminated or generated from one canonical
  source manifest.

Recommended options:

```text
LIBSATURN_BUILD_TESTS
LIBSATURN_BUILD_EXAMPLES
LIBSATURN_BUILD_HOST_TOOLS
LIBSATURN_ENABLE_PROFILE_METRICS
LIBSATURN_PARALLEL_TEST_FAULT
```

Defaults for a package build:

```text
BUILD_TESTS      OFF
BUILD_EXAMPLES   OFF
BUILD_HOST_TOOLS OFF
```

Use normal CMake install/export support and export the namespace
`LibSaturn::`.

The package must support:

```cmake
find_package(LibSaturn CONFIG REQUIRED)
```

and version-aware lookup through `LibSaturnConfigVersion.cmake`.

---

## Toolchain file

Add:

```text
cmake/toolchains/sh2eb-elf.cmake
```

The toolchain owns target/compiler identity, not project targets.

It should set at least:

```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR sh2)

set(CMAKE_C_COMPILER sh2eb-elf-gcc)
set(CMAKE_CXX_COMPILER sh2eb-elf-g++)
set(CMAKE_ASM_COMPILER sh2eb-elf-gcc)
set(CMAKE_AR sh2eb-elf-ar)
set(CMAKE_OBJCOPY sh2eb-elf-objcopy)
```

Target-specific flags currently embedded in the Makefile must move to target
properties where possible:

```text
-m2
-mb
-ffreestanding
-fomit-frame-pointer
-fno-exceptions
-fno-rtti
-fno-threadsafe-statics
-fno-use-cxa-atexit
```

Avoid global `CMAKE_C_FLAGS`/`CMAKE_CXX_FLAGS` when a target property is
sufficient.

---

## Executable helper

Installing a static archive is not enough for a bare-metal console.

Provide:

```cmake
libsaturn_configure_executable(<target>)
```

This helper must own the Saturn application link contract currently hard-coded
in the Makefile:

- `-m2 -mb`;
- `-nostdlib`;
- installed `saturn.ld`;
- startup retention/link ordering;
- `libgcc`;
- map-file generation;
- post-link `.init_array` validation equivalent to
  `tools/check_no_init_array.py` or a CMake-native replacement;
- optional binary conversion helpers.

The consumer should not need to know the package's physical install paths.

Later, a second helper may own disc creation:

```cmake
libsaturn_add_disc(
    TARGET my_game
    VOLUME_ID MY_GAME
    ...
)
```

Keep executable linking and optical-disc packaging separate.

---

## Source manifest

The package build should not rely on a recursive glob as its final long-term
source contract.

Create a canonical manifest, for example:

```text
cmake/LibSaturnSources.cmake
```

organized by subsystem:

```cmake
set(LIBSATURN_CORE_SOURCES ...)
set(LIBSATURN_GRAPHICS_SOURCES ...)
set(LIBSATURN_AUDIO_SOURCES ...)
set(LIBSATURN_HAL_SOURCES ...)
set(LIBSATURN_INPUT_SOURCES ...)
set(LIBSATURN_PHYSICS_SOURCES ...)
set(LIBSATURN_RESOURCES_SOURCES ...)
set(LIBSATURN_STORAGE_SOURCES ...)
```

CMake consumes that manifest directly.

During transition the Makefile may obtain its library source list from a
generated flat list derived from the same manifest. There must not be two
manually maintained authoritative source inventories.

---

## Public/private include cleanup

`include/saturn/` is the package API surface.

Anything installed from there becomes a compatibility promise.

Immediate cleanup:

- move `include/saturn/example_util.h` to an example-only location such as
  `examples/common/example_util.h`;
- verify that no public header includes files from `src/`;
- keep internal `.hpp` files below `src/`;
- add a public version header, generated or checked in, with SemVer-compatible
  version macros;
- add a header-only package smoke test that includes every public header from C;
- add a second smoke test that includes the umbrella header from C++.

Do not expose HAL implementation headers merely because they are useful inside
the repository. Public hardware APIs must remain explicit `include/saturn/*.h`
contracts.

---

## Versioning

Introduce one authoritative project version.

Recommended first public package line:

```text
0.x.y
```

until the installed API/package contract is considered stable.

Use Git tags:

```text
v0.x.y
```

The same version must drive:

- CMake `project(... VERSION ...)`;
- `LibSaturnConfigVersion.cmake`;
- Conan recipe version;
- release archives;
- public `saturn/version.h`.

Avoid independently edited versions in four files.

---

## Conan 2

Conan should be the first package-manager integration.

Why Conan first:

- it explicitly models separate build and host profiles;
- LibSaturn naturally needs a native host environment plus a cross-compiled
  Saturn target;
- Conan allows extending settings through `settings_user.yml`, which is
  needed because Sega Saturn SH-2 is not a normal desktop target.

Repository layout:

```text
packaging/
└── conan/
    ├── config/
    │   └── settings_user.yml
    └── profiles/
        └── saturn-sh2eb

conanfile.py
```

Suggested custom settings:

```yaml
os:
    baremetal:
arch: [sh2eb]
```

The host profile identifies the Saturn target. The build profile remains the
developer's native machine.

Typical consumer flow should become:

```sh
conan config install <libsaturn-conan-config>
conan install . \
  --profile:host=saturn-sh2eb \
  --profile:build=default \
  --build=missing
```

The recipe should build/install through the same CMake package contract. It
must not reproduce compiler flags or source ownership independently.

The generated CMake dependency name must remain `LibSaturn` and the normal
target must remain `LibSaturn::Saturn`.

---

## vcpkg

Do not make vcpkg the gating build-system requirement for this refactor.

At the time this plan was written, vcpkg's documented
`VCPKG_TARGET_ARCHITECTURE` values do not include SH-2. Overlay ports and
overlay triplets are supported, but that does not by itself create a native
SH-2 architecture identity inside vcpkg.

Therefore:

1. finish the CMake install contract first;
2. finish Conan 2 support second;
3. run a focused vcpkg feasibility spike;
4. only advertise vcpkg as supported if a clean SH-2 package can be consumed
   without lying about the target architecture or maintaining an invasive
   vcpkg fork.

Possible experimental path:

```text
packaging/vcpkg/
├── ports/libsaturn/
│   ├── portfile.cmake
│   └── vcpkg.json
└── triplets/
    └── sh2eb-saturn.cmake
```

If the architecture limitation prevents a correct compiled port, prefer to
document vcpkg as unsupported rather than publishing a fragile fake triplet.

---

## Package-consumer tests

A package is not complete merely because the repository itself builds.

Add a standalone test project outside the source-tree assumptions:

```text
tests/package_consumer/
├── CMakeLists.txt
└── main.c
```

CI must:

1. configure LibSaturn with the SH-2 toolchain;
2. build it;
3. install it to a temporary prefix;
4. configure `tests/package_consumer` using only that prefix;
5. resolve `find_package(LibSaturn CONFIG REQUIRED)`;
6. compile and link a minimal Saturn ELF;
7. verify no source-tree include path leaked into the consumer;
8. verify the linker script comes from the installed package;
9. verify startup symbols are present;
10. run the existing post-link constructor check.

Add a second Conan CI path that creates the Conan package and repeats the same
consumer build through Conan-generated dependency metadata.

No package release should be published without both tests passing.

---

## Repository organization cleanup

Packaging also gives a useful boundary for repository cleanup.

Target top level:

```text
.github/
assets/
cmake/
docs/
examples/
harness/
include/
packaging/
scripts/
src/
tests/
tools/
CMakeLists.txt
CMakePresets.json
LICENSE
Makefile
README.md
VERSION
conanfile.py
```

Audit current root artifacts such as `$tmp`, `ip.bin` and `probe.json`.
Generated/test artifacts must not live at repository root. Move legitimate
fixtures to a named owner under `assets/` or `tests/fixtures/`; otherwise
remove them.

PowerShell convenience scripts may remain as thin root wrappers temporarily,
but implementation should progressively move under `scripts/`.

---

## CMake presets

Add presets so contributors do not need to memorize cross-build command lines.

Suggested names:

```text
saturn-debug
saturn-release
host-tests
package-consumer
```

A package consumer should still be able to use a normal custom toolchain
without presets; presets are contributor convenience, not package API.

---

## README end-state

The README quick start should stop requiring a clone of LibSaturn as the only
path.

Show three equivalent entry points:

### Installed CMake package

```cmake
find_package(LibSaturn CONFIG REQUIRED)
target_link_libraries(my_game PRIVATE LibSaturn::Saturn)
libsaturn_configure_executable(my_game)
```

### Conan

```text
libsaturn/<version>
```

with the Saturn host profile.

### Source checkout

Keep the current repository/example workflow for contributors and for users who
want the complete asset/disc tooling.

---

## Migration phases

### Phase 1 - Package boundary

- add `VERSION`;
- add root CMake project;
- add SH-2 CMake toolchain;
- build `LibSaturn::Core`, `LibSaturn::Startup` and
  `LibSaturn::Saturn`;
- install headers/library/linker script;
- export `LibSaturnConfig.cmake`;
- move example-only public header out of `include/saturn`;
- add standalone installed-package consumer test.

Acceptance:

- current Makefile builds remain green;
- CMake produces the same reusable library functionality;
- a consumer outside the repo can compile/link with `find_package`.

### Phase 2 - Build ownership cleanup

- introduce canonical source manifest;
- remove source-list duplication;
- add CMake presets;
- isolate runtime build from examples/assets/disc generation;
- reduce the monolithic Makefile to orchestration or migrate examples in
  controlled groups.

Acceptance:

- adding a new runtime source requires editing one source manifest;
- library package builds never require example assets, FFmpeg, Pillow,
  mkisofs or emulator tooling.

### Phase 3 - Conan 2

- add `conanfile.py`;
- add shared Conan config with SH-2/bare-metal settings;
- add Saturn host profile;
- add Conan consumer CI;
- document local recipe and remote publishing flow.

Acceptance:

- a clean external consumer resolves `libsaturn/<version>`;
- Conan cross-build uses separate native build and Saturn host contexts;
- the consumer still sees `LibSaturn::Saturn`.

### Phase 4 - Release packaging

- tag-based GitHub release workflow;
- source archive;
- prebuilt package artifact where toolchain compatibility makes that useful;
- checksums;
- package-consumer gate before release;
- changelog/release notes.

### Phase 5 - vcpkg feasibility and optional support

- prototype overlay port/triplet;
- validate whether current vcpkg architecture constraints permit a truthful
  SH-2 target;
- only then add support and CI.

Acceptance:

- no fake x86/arm target identity;
- no requirement to patch each consumer's vcpkg checkout manually;
- package resolves to the same CMake targets as every other installation path.

---

## Non-goals

This refactor must not:

- redesign the LibSaturn public runtime API merely to satisfy a package manager;
- force examples into external package consumers;
- bundle emulator/harness GPL code into the core library package;
- make host asset tools target binaries;
- introduce dynamic libraries on Saturn;
- require users to know internal `src/` paths;
- remove the existing build before equivalent package behavior is proven;
- hide Saturn-specific linker/startup requirements behind undocumented magic.

---

## Recommended implementation order

Start now with Phase 1.

The key architectural rule is:

> CMake defines what an installed LibSaturn is. Package managers only transport
> that definition.

This keeps Conan, future vcpkg support, release archives and direct
`cmake --install` consumers from drifting into different versions of the
library.

---

## Implementation status (2026-10-02)

Operating manual: [PACKAGING.md](PACKAGING.md). vcpkg result:
[VCPKG_FEASIBILITY.md](VCPKG_FEASIBILITY.md).

| Phase | State | Evidence |
|---|---|---|
| 1 - Package boundary | Done | `CMakeLists.txt`, `cmake/`, `VERSION`, `include/saturn/version.h`; `example_util.h` moved to `examples/common/`; `tests/package_consumer` passes (`ctest --preset package-consumer`). |
| 2 - Build ownership | Done for the runtime | One manifest (`cmake/LibSaturnSources.cmake`) read by CMake and, through `tools/library_sources.py`, by the Makefile; presets added; the package build needs no assets, FFmpeg, Pillow, mkisofs or emulators. The example/asset/disc rules stay in the Makefile by design (see below). |
| 3 - Conan 2 | Done | `conanfile.py`, `packaging/conan/`, `test_package/`; `conan create` builds the SH-2 package and runs the consumer contract checks through Conan. |
| 4 - Release packaging | Done (workflow written, first run happens on the first tag) | `.github/workflows/release.yml`, `CHANGELOG.md`, `scripts/release-notes.py`; gates identical to `package.yml`. |
| 5 - vcpkg | Spike done: experimental, not advertised | Overlay port/triplet build and pass the consumer gate; relies on undocumented architecture tolerance. Non-gating CI job. |

### Deviations from the plan text

- **Whole-archive is spelled out.** CMake's `$<LINK_LIBRARY:WHOLE_ARCHIVE>` is not
  defined for a bare-metal system and fails in the consumer's link step, so
  `LibSaturn::Saturn` carries `LINKER:--whole-archive,<startup.a>,--no-whole-archive`
  as an interface link option.
- **The existing constructor guard was toothless and is fixed.** With
  `--gc-sections` and no `KEEP`, the linker discarded `.ctors`/`.init_array`
  before `tools/check_no_static_ctors.py` looked, so the Makefile guard could never
  fire. `saturn.ld` now keeps those (normally empty) sections. All 43 Makefile
  example builds still pass the guard; the package gate proves the guard rejects a
  program with a static constructor.
- **Runtime is always `-O2`.** Compile options override CMake's per-config
  `-O3`/`-O0`; `makefile_flag_parity` compares the effective flags with the Makefile.
- **Toolchain file ships in the package** (`share/libsaturn/toolchains/`), because a
  consumer must name it before `find_package` can run.
- **`libsaturn_add_binary()`** was added next to `libsaturn_configure_executable()`
  for the "optional binary conversion helper".
- **`libsaturn_add_disc()`** was added when the first external consumer (the
  [ikemen-saturn](https://github.com/celsowm/ikemen-saturn) project, now extracted)
  needed a bootable disc without the Makefile.
- **Post-link check is CMake-native** (`LibSaturnCheckNoInitArray.cmake`); the Python
  tool stays for the Makefile.

### Ikemen extraction

`examples/ikemen_saturn` and everything specific to it (converters, host tests,
upstream-oracle tooling, generated-asset rules, CI) moved to
[celsowm/ikemen-saturn](https://github.com/celsowm/ikemen-saturn), an independent
consumer built through `find_package(LibSaturn CONFIG)`, the shipped toolchain file
and `libsaturn_add_disc()`, or through the Conan package. The extraction was
validated by running the old in-tree build and the new external build in the Ymir
probe under the same scripted input: screenshots were byte-identical, and the 63
upstream oracle scenarios give the same result (61 pass, 2 pending) as before.

### Still open

- Moving the Makefile's own disc rules onto `libsaturn_add_disc()` (the function now
  exists and is gated by the package consumer test; the Makefile still has its own
  copy of the steps).
- Migrating examples/asset rules out of the monolithic Makefile (plan: "reduce to
  orchestration or migrate in controlled groups"). The runtime no longer depends on
  it; the example workflow does.
- The 130+ host tests still have their per-test source lists in the Makefile; they
  are not duplicated into CMake.
- First tagged release (`v0.1.0`) and the pinned release-tarball hash a real vcpkg
  port needs.
- `LIBSATURN_BUILD_EXAMPLES` and `LIBSATURN_BUILD_HOST_TOOLS` are reserved options.
