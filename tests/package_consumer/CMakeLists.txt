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
