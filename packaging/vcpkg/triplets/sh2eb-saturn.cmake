# EXPERIMENTAL - see docs/VCPKG_FEASIBILITY.md.
#
# Truthful triplet for the Sega Saturn: the target architecture is declared as
# what it is (sh2eb), not as a stand-in. vcpkg's documentation does not list
# sh2eb among VCPKG_TARGET_ARCHITECTURE values; the vcpkg tool currently
# forwards the string unchanged, which is what makes this work.
#
# Use as an overlay (never patch a vcpkg checkout):
#   vcpkg install libsaturn --triplet sh2eb-saturn \
#       --overlay-ports=<libsaturn>/packaging/vcpkg/ports \
#       --overlay-triplets=<libsaturn>/packaging/vcpkg/triplets
set(VCPKG_TARGET_ARCHITECTURE sh2eb)
set(VCPKG_CMAKE_SYSTEM_NAME Generic)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
# The runtime has no debug-only variant; one configuration is enough.
set(VCPKG_BUILD_TYPE release)

set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../../../cmake/toolchains/sh2eb-elf.cmake")

# vcpkg builds in a scrubbed environment on Windows. Let the toolchain
# location, the checkout used by the port and PATH through (an MSYS2-built
# cross compiler needs its runtime DLLs on PATH).
set(VCPKG_ENV_PASSTHROUGH_UNTRACKED LIBSATURN_TOOLCHAIN_ROOT LIBSATURN_SOURCE_DIR PATH)
