# End-to-end installed-package test.
#
#   cmake -DSOURCE_DIR=<repo> -DWORK_DIR=<scratch> \
#         -DTOOLCHAIN_FILE=<repo>/cmake/toolchains/sh2eb-elf.cmake \
#         [-DGENERATOR=Ninja] [-DPREFIX=<existing install prefix>] \
#         -P tests/package_consumer/run.cmake
#
# 1. configure + build LibSaturn with the SH-2 toolchain, install it to a
#    temporary prefix (skipped when PREFIX is given);
# 2. configure tests/package_consumer using ONLY that prefix, including a
#    version-aware find_package(LibSaturn <version> CONFIG);
# 3. build a minimal Saturn ELF and run check.cmake, the contract checks that
#    the Conan test_package shares:
#      - no source-tree include path leaks into the consumer;
#      - the linker script, headers and archives come from the prefix;
#      - startup symbols are in the ELF, entry point at 0x06004000;
#      - the post-link constructor guard passes, and rejects a bad program.

cmake_minimum_required(VERSION 3.24)
include("${CMAKE_CURRENT_LIST_DIR}/helpers.cmake")

foreach(_v SOURCE_DIR WORK_DIR TOOLCHAIN_FILE)
    if(NOT ${_v})
        message(FATAL_ERROR "run.cmake: -D${_v}=... is required")
    endif()
endforeach()
if(NOT GENERATOR)
    set(GENERATOR Ninja)
endif()
file(TO_CMAKE_PATH "${SOURCE_DIR}" SOURCE_DIR)
file(TO_CMAKE_PATH "${WORK_DIR}" WORK_DIR)
file(TO_CMAKE_PATH "${TOOLCHAIN_FILE}" TOOLCHAIN_FILE)

file(STRINGS "${SOURCE_DIR}/VERSION" LIBSATURN_VERSION LIMIT_COUNT 1)
string(STRIP "${LIBSATURN_VERSION}" LIBSATURN_VERSION)

# -- 1. build + install the package ----------------------------------------
if(PREFIX)
    file(TO_CMAKE_PATH "${PREFIX}" PREFIX)
    message(STATUS "[package-consumer] using existing prefix ${PREFIX}")
else()
    set(PREFIX "${WORK_DIR}/prefix")
    file(REMOVE_RECURSE "${WORK_DIR}")
    step_run("configure LibSaturn" COMMAND "${CMAKE_COMMAND}"
        -S "${SOURCE_DIR}" -B "${WORK_DIR}/lib" -G "${GENERATOR}"
        "-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}" -DCMAKE_BUILD_TYPE=Release)
    step_run("build LibSaturn" COMMAND "${CMAKE_COMMAND}" --build "${WORK_DIR}/lib")
    step_run("install LibSaturn to a temporary prefix" COMMAND "${CMAKE_COMMAND}"
        --install "${WORK_DIR}/lib" --prefix "${PREFIX}")
endif()

# Install layout from the plan.
foreach(_f
        include/saturn/saturn.h include/saturn/version.h
        lib/libsaturn.a lib/libsaturn_startup.a
        share/libsaturn/linker/saturn.ld
        share/libsaturn/toolchains/sh2eb-elf.cmake
        share/libsaturn/cmake/LibSaturnFunctions.cmake)
    if(NOT EXISTS "${PREFIX}/${_f}")
        message(FATAL_ERROR "[package-consumer] install tree is missing ${_f}")
    endif()
endforeach()
# The config lives in lib/cmake/LibSaturn for cmake --install; package managers
# may relocate it (vcpkg moves it to share/<port>), which is allowed.
foreach(_f LibSaturnConfig.cmake LibSaturnConfigVersion.cmake LibSaturnTargets.cmake)
    if(NOT EXISTS "${PREFIX}/lib/cmake/LibSaturn/${_f}"
       AND NOT EXISTS "${PREFIX}/share/libsaturn/${_f}"
       AND NOT EXISTS "${PREFIX}/share/LibSaturn/${_f}")
        message(FATAL_ERROR "[package-consumer] install tree is missing ${_f}")
    endif()
endforeach()
if(EXISTS "${PREFIX}/include/saturn/example_util.h")
    message(FATAL_ERROR "[package-consumer] example-only header leaked into the install tree")
endif()

# -- 2. configure the consumer from the prefix only ------------------------
set(_consumer "${WORK_DIR}/consumer")
file(REMOVE_RECURSE "${_consumer}")
# A game configures with the toolchain file the package ships.
set(_installed_toolchain "${PREFIX}/share/libsaturn/toolchains/sh2eb-elf.cmake")
if(NOT EXISTS "${_installed_toolchain}")
    set(_installed_toolchain "${TOOLCHAIN_FILE}")
endif()
set(_consumer_args
    -S "${SOURCE_DIR}/tests/package_consumer" -B "${_consumer}" -G "${GENERATOR}"
    "-DCMAKE_TOOLCHAIN_FILE=${_installed_toolchain}"
    "-DCMAKE_PREFIX_PATH=${PREFIX}"
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

step_run("consumer rejects an incompatible version request" EXPECT_FAILURE 1
    COMMAND "${CMAKE_COMMAND}" ${_consumer_args} -DLIBSATURN_REQUIRED_VERSION=99.0.0)
file(REMOVE_RECURSE "${_consumer}")
step_run("configure consumer with find_package(LibSaturn ${LIBSATURN_VERSION} CONFIG)"
    OUTPUT_VAR _configure_out
    COMMAND "${CMAKE_COMMAND}" ${_consumer_args}
        -DLIBSATURN_REQUIRED_VERSION=${LIBSATURN_VERSION})
expect_contains("${_configure_out}" "libsaturn-consumer: version=${LIBSATURN_VERSION}"
    "package version")
expect_contains("${_configure_out}"
    "linker-script=${PREFIX}/share/libsaturn/linker/saturn.ld"
    "linker script must come from the installed package")

# -- 3. build the consumer and check the contract --------------------------
step_run("build consumer ELF" OUTPUT_VAR _build_out
    COMMAND "${CMAKE_COMMAND}" --build "${_consumer}" --verbose)
set(_build_log "${WORK_DIR}/consumer-build.log")
file(WRITE "${_build_log}" "${_build_out}")

step_run("verify the consumer against the package contract"
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${SOURCE_DIR}" "-DPREFIX=${PREFIX}"
        "-DCONSUMER_DIR=${_consumer}" "-DBUILD_LOG=${_build_log}"
        -P "${CMAKE_CURRENT_LIST_DIR}/check.cmake")

message(STATUS "[package-consumer] OK: LibSaturn ${LIBSATURN_VERSION} prefix ${PREFIX}")
