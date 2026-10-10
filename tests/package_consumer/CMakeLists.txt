# Standalone consumer of an installed LibSaturn package.
#
# Deliberately knows nothing about the LibSaturn source tree: it finds the
# package through CMAKE_PREFIX_PATH only. Configure it with the SH-2 toolchain
# file (tests/package_consumer/run.cmake does this end to end).
cmake_minimum_required(VERSION 3.24)

# C only on purpose: find_package(LibSaturn) must enable what the runtime
# needs by itself, as a game that never mentions C++ would rely on.
project(libsaturn_package_consumer LANGUAGES C)

set(LIBSATURN_REQUIRED_VERSION "" CACHE STRING "Version to request from find_package")
find_package(LibSaturn ${LIBSATURN_REQUIRED_VERSION} CONFIG REQUIRED)

add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE LibSaturn::Saturn)
libsaturn_configure_executable(consumer)
libsaturn_add_binary(consumer MAX_BYTES 983040)
# Bootable disc through the package's own tooling; disc_data/ and both files land in the disc root
# (more than one ROOT_FILES entry: the list must survive the trip into the disc script).
libsaturn_add_disc(consumer ROOT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/disc_data"
    ROOT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/disc_files/ONE.TXT" "${CMAKE_CURRENT_SOURCE_DIR}/disc_files/TWO.TXT")

# Facts the driver verifies about the resolved package.
foreach(_t LibSaturn::Saturn LibSaturn::Core LibSaturn::Startup)
    if(NOT TARGET ${_t})
        message(FATAL_ERROR "package does not provide ${_t}")
    endif()
endforeach()
get_target_property(_core_includes LibSaturn::Core INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "libsaturn-consumer: version=${LibSaturn_VERSION}")
message(STATUS "libsaturn-consumer: include-dirs=${_core_includes}")
message(STATUS "libsaturn-consumer: linker-script=${LIBSATURN_LINKER_SCRIPT}")

# A program whose global needs a static constructor. crt0 would never run it,
# so linking it must fail the post-link check. The driver builds this target
# and expects the build to be rejected.
add_executable(ctor_trap EXCLUDE_FROM_ALL ctor_trap.cpp)
target_link_libraries(ctor_trap PRIVATE LibSaturn::Saturn)
libsaturn_configure_executable(ctor_trap)

# The consumer program asking for a Master stack reserve larger than Work RAM High: no .bss can
# leave that much room, so the linker script's stack check must reject the link.
add_executable(stack_trap EXCLUDE_FROM_ALL main.c)
target_link_libraries(stack_trap PRIVATE LibSaturn::Saturn)
libsaturn_configure_executable(stack_trap)
target_link_options(stack_trap PRIVATE "-Wl,--defsym=__stack_reserve=0x100000")

# A program whose tables share a cart slot: two overlay members at one address plus a resident
# cart section, split into the app image and flat files by the cart helpers.
set(_ovl_saved_ld "${LIBSATURN_LINKER_SCRIPT}")
libsaturn_cart_overlay_script("${CMAKE_BINARY_DIR}/gen/overlay.ld"
    SLOT_ADDRESS 0x02400000 SLOT_BYTES 0x40 PHDR slot
    MEMBERS ovl_a:.rodata.ovl_a_table ovl_b:.rodata.ovl_b_table)
file(WRITE "${CMAKE_BINARY_DIR}/gen/resident.ld"
"    .cartres 0x02400040 : AT(0x02400040) SUBALIGN(4) {
        *overlay_data*.obj(.rodata.resident_table)
        . = ALIGN(4);
    } :cartres
")
libsaturn_inject_link_script("${CMAKE_BINARY_DIR}/gen/overlay_consumer.ld"
    BLOCKS "${CMAKE_BINARY_DIR}/gen/overlay.ld" "${CMAKE_BINARY_DIR}/gen/resident.ld"
    PHDRS slot cartres)
set(LIBSATURN_LINKER_SCRIPT "${CMAKE_BINARY_DIR}/gen/overlay_consumer.ld")
add_executable(overlay_consumer overlay_main.c overlay_data.c)
target_compile_options(overlay_consumer PRIVATE -ffunction-sections -fdata-sections)
target_link_libraries(overlay_consumer PRIVATE LibSaturn::Saturn)
libsaturn_configure_executable(overlay_consumer)
libsaturn_split_cart_image(overlay_consumer
    APP_BIN "${CMAKE_BINARY_DIR}/overlay_consumer.app.bin" MAX_BYTES 983040
    DATA_SECTION .cartres DATA_BIN "${CMAKE_BINARY_DIR}/overlay_consumer.res.bin"
    OVERLAYS ovl_a=${CMAKE_BINARY_DIR}/OVLA.BIN ovl_b=${CMAKE_BINARY_DIR}/OVLB.BIN)
set(LIBSATURN_LINKER_SCRIPT "${_ovl_saved_ld}")
