# Changelog

All notable changes to LibSaturn are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/). While the major version is `0`, a
minor release may change the installed package contract.

The version is defined once, in the `VERSION` file.

## [0.1.0]

First installable release: LibSaturn can now be consumed as a CMake package
instead of being copied into a game repository.

### Added

- Root CMake project exporting `LibSaturn::Saturn`, `LibSaturn::Core` and
  `LibSaturn::Startup`, with a relocatable, version-aware `LibSaturnConfig.cmake`.
- `libsaturn_configure_executable()` (link contract, link map, post-link
  static-constructor guard) and `libsaturn_add_binary()` for package consumers.
- `libsaturn_add_disc()`: ISO/BIN/CUE from a linked program through the shipped,
  checked disc tools (needs Python 3 and mkisofs/genisoimage/xorrisofs).
- `cmake/toolchains/sh2eb-elf.cmake`, shipped in the install tree.
- Single runtime source manifest (`cmake/LibSaturnSources.cmake`) shared by CMake
  and the Makefile.
- `saturn/version.h` with `SATURN_VERSION_*` macros, generated from `VERSION`.
- Conan 2 recipe, `os=baremetal`/`arch=sh2eb` settings extension and the
  `saturn-sh2eb` host profile; `test_package` consumer.
- CMake presets: `saturn-debug`, `saturn-release`, `host-tests`, `package-consumer`.
- Package gates: header smoke tests, installed-package consumer test,
  Makefile/CMake flag-parity test; `scripts/test-package.{sh,ps1}`.
- CI workflow for the package gates and a tag-driven release workflow producing a
  source archive, a prebuilt prefix, a Conan cache and `SHA256SUMS`.

### Changed

- `saturn.h` includes `saturn/version.h`.
- `example_util.h` moved from the public `include/saturn/` tree to
  `examples/common/`; examples include it as `"example_util.h"` (the Makefile adds
  `-Iexamples/common` to example builds only).
- `saturn.ld` keeps `.preinit_array`, `.init_array` and `.ctors` sections
  (empty, so free) so the post-link constructor guard can see them.

### Fixed

- `assets/boot/ip_yaul_template.bin`, the Makefile's default IP.BIN template, was
  git-ignored (`*.bin`) and never committed, so a clean checkout could not build any
  disc image. It is now tracked.
- The static-constructor guard could never fire: `--gc-sections` discarded the
  constructor tables before the check ran, so a global needing a constructor
  passed silently while staying null on the console.

### Removed

- The `ikemen_saturn` example, its converters, tests and upstream-oracle tooling
  moved to the independent consumer repository
  [celsowm/ikemen-saturn](https://github.com/celsowm/ikemen-saturn), which builds
  against the installed package. LibSaturn keeps no Ikemen-specific build or test
  knowledge.
- Stray repository-root artifacts (`ip.bin`, `probe.json`, `build_and_log.ps1`);
  `$tmp` helper scripts moved to `scripts/dev/`.

### Not supported

- vcpkg. See `docs/VCPKG_FEASIBILITY.md`.
