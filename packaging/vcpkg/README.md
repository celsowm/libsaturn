# vcpkg (experimental)

An overlay port and triplet that deliver the same installed CMake package as
every other path (`find_package(LibSaturn CONFIG)` → `LibSaturn::Saturn`).
It is **not** in the vcpkg registry and is not a supported distribution
channel yet; read [docs/VCPKG_FEASIBILITY.md](../../docs/VCPKG_FEASIBILITY.md)
first.

```text
ports/libsaturn/{vcpkg.json,portfile.cmake}
triplets/sh2eb-saturn.cmake
```

```sh
export LIBSATURN_SOURCE_DIR=/path/to/libsaturn      # until a release tarball hash is pinned
export LIBSATURN_TOOLCHAIN_ROOT=/path/to/saturn-tools/bin   # or have sh2eb-elf-gcc on PATH
vcpkg install libsaturn --triplet sh2eb-saturn \
    --overlay-ports=packaging/vcpkg/ports \
    --overlay-triplets=packaging/vcpkg/triplets
```

In a manifest project, put the overlays in `vcpkg-configuration.json` instead
of the command line:

```json
{
  "overlay-ports": ["path/to/libsaturn/packaging/vcpkg/ports"],
  "overlay-triplets": ["path/to/libsaturn/packaging/vcpkg/triplets"]
}
```

Verify an installed tree with the shared consumer gate:

```sh
cmake -DSOURCE_DIR=<libsaturn> -DWORK_DIR=<scratch> \
      -DPREFIX=<vcpkg_installed>/sh2eb-saturn \
      -DTOOLCHAIN_FILE=<libsaturn>/cmake/toolchains/sh2eb-elf.cmake \
      -P tests/package_consumer/run.cmake
```
