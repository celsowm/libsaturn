# vcpkg feasibility (Phase 5 spike)

**Verdict: experimental, not advertised as supported.** A truthful SH-2 package
*does* build and pass the consumer gate through vcpkg today, but it relies on
behaviour the vcpkg documentation does not promise.

## Question

The packaging plan asked whether vcpkg can deliver LibSaturn without lying about
the target architecture and without a vcpkg fork. The documented values of
`VCPKG_TARGET_ARCHITECTURE` are `x86, x64, arm, arm64, arm64ec, s390x, ppc64le,
riscv32, riscv64, loongarch32, loongarch64, mips64, wasm32`
([triplet variables](https://learn.microsoft.com/en-us/vcpkg/users/triplets)).
SH-2 is not among them.

## What was tried

Date: 2026-10-02. vcpkg tool `2026-09-26-51bf87ca`, vcpkg registry `3aea538b`,
Windows 11, sh2eb-elf GCC 13.2.0 from MSYS2.

`packaging/vcpkg/` holds an overlay port (`libsaturn`) and an overlay triplet
(`sh2eb-saturn`) with `VCPKG_TARGET_ARCHITECTURE sh2eb`,
`VCPKG_CMAKE_SYSTEM_NAME Generic`, static linkage and
`VCPKG_CHAINLOAD_TOOLCHAIN_FILE` pointing at `cmake/toolchains/sh2eb-elf.cmake`.
The port runs `vcpkg_cmake_configure/install` against the repository CMake
project; it adds no flags or source lists of its own.

```sh
vcpkg install libsaturn --triplet sh2eb-saturn \
    --overlay-ports=packaging/vcpkg/ports --overlay-triplets=packaging/vcpkg/triplets
```

## Findings

1. **The architecture string is not validated.** vcpkg passed
   `-DVCPKG_TARGET_ARCHITECTURE=sh2eb` straight to CMake. No fake x86/arm
   identity is needed, and no vcpkg checkout is patched; overlays are the
   supported extension point (`--overlay-*` flags or `vcpkg-configuration.json`).
2. **Compiler detection and the build succeed** with the chainloaded toolchain.
   The package installs to `<root>/installed/sh2eb-saturn`.
3. **The installed package passes the shared consumer gate**
   (`tests/package_consumer/run.cmake -DPREFIX=...`): version-aware
   `find_package`, no source-tree leaks, installed linker script, startup symbols,
   constructor guard. vcpkg relocates the CMake config to `share/libsaturn`;
   the config is relocatable, so nothing breaks.
4. **Windows needs triplet help.** vcpkg builds in a scrubbed environment, so an
   MSYS2-built cross compiler failed with `0xC0000135` (missing runtime DLLs)
   until `VCPKG_ENV_PASSTHROUGH_UNTRACKED` forwarded `PATH`. (The toolchain file also
   needed `.exe` on full compiler paths for `LIBSATURN_TOOLCHAIN_ROOT`; fixed there.)
   Linux/macOS hosts do not need this.
5. vcpkg still builds its *host* helper ports (`vcpkg-cmake`) for the host triplet
   (a Visual Studio toolchain on Windows), which is normal cross-build behaviour.

## Why it is not "supported"

- The documented architecture list does not include `sh2eb`; a future vcpkg release
  may start rejecting unknown values. Supporting a channel that works by accident
  would break the plan's rule against fragile fakes.
- Platform expressions in `"supports"`/`"dependencies"` cannot name SH-2, so the
  port can only exclude hosts, not describe the target.
- The port is not in the vcpkg registry; consumers must add overlays. A real port
  also needs a pinned release tarball hash (`vcpkg_from_github`), which exists only
  after the first tagged release. Until then it builds from `LIBSATURN_SOURCE_DIR`.
- Binary caching ABI hashes know nothing about the GCC/binutils pair the
  toolchain provides beyond the compiler hash.

## Promotion criteria

Advertise vcpkg when all of these hold:

1. A tagged release exists and the port downloads it (`vcpkg_from_github`, pinned `SHA512`).
2. vcpkg documents custom/unknown architectures, or accepts a `sh2eb` value upstream.
3. The `vcpkg` CI job in `.github/workflows/package.yml` (currently non-gating) has
   passed on `main` for a full release cycle.

Until then the supported delivery channels are the installed CMake package,
release archives and Conan 2.
