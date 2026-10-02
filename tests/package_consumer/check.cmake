# Contract checks on a built consumer. Shared by run.cmake (plain CMake) and
# the Conan test_package, so both delivery paths are held to the same rules.
#
#   cmake -DSOURCE_DIR=<repo> -DPREFIX=<installed LibSaturn prefix>
#         -DCONSUMER_DIR=<consumer build dir> -DBUILD_LOG=<verbose link log>
#         -P tests/package_consumer/check.cmake
#
# BUILD_LOG is the output of `cmake --build <CONSUMER_DIR> --verbose` for a
# fresh link, so the exact link line can be inspected.

cmake_minimum_required(VERSION 3.24)
include("${CMAKE_CURRENT_LIST_DIR}/helpers.cmake")

foreach(_v SOURCE_DIR PREFIX CONSUMER_DIR BUILD_LOG)
    if(NOT ${_v})
        message(FATAL_ERROR "check.cmake: -D${_v}=... is required")
    endif()
endforeach()
file(TO_CMAKE_PATH "${SOURCE_DIR}" SOURCE_DIR)
file(TO_CMAKE_PATH "${PREFIX}" PREFIX)
file(TO_CMAKE_PATH "${CONSUMER_DIR}" _consumer)
file(READ "${BUILD_LOG}" _build_out)
set(_elf "${_consumer}/consumer.elf")
if(NOT EXISTS "${_elf}")
    message(FATAL_ERROR "[package-consumer] consumer.elf was not produced")
endif()
if(NOT EXISTS "${_consumer}/consumer.app.bin")
    message(FATAL_ERROR "[package-consumer] consumer.app.bin was not produced")
endif()
if(NOT EXISTS "${_elf}.map")
    message(FATAL_ERROR "[package-consumer] link map was not produced")
endif()

# 7. no source-tree include path leaked into the consumer.
file(READ "${_consumer}/compile_commands.json" _ccdb)
expect_contains("${_ccdb}" "${PREFIX}/include" "consumer compile uses the installed headers")
expect_missing("${_ccdb}" "${SOURCE_DIR}/include" "consumer compile commands")
expect_missing("${_ccdb}" "-I${SOURCE_DIR}\"" "consumer compile commands")
expect_missing("${_ccdb}" "${SOURCE_DIR}/src" "consumer compile commands")

# 8. linker script, archives and startup from the installed package.
expect_contains("${_build_out}" "-Wl,-T,${PREFIX}/share/libsaturn/linker/saturn.ld"
    "linker script must come from the installed package")
expect_missing("${_build_out}" "${SOURCE_DIR}/src" "consumer link line")
expect_contains("${_build_out}" "--whole-archive" "startup must be linked whole-archive")
expect_contains("${_build_out}" "${PREFIX}/lib/libsaturn_startup.a"
    "startup archive must come from the installed package")
expect_contains("${_build_out}" "${PREFIX}/lib/libsaturn.a"
    "runtime archive must come from the installed package")

# 9. startup symbols are present and _start sits at the BIOS load address.
find_program(_nm NAMES sh2eb-elf-nm HINTS ENV LIBSATURN_TOOLCHAIN_ROOT)
find_program(_objdump NAMES sh2eb-elf-objdump HINTS ENV LIBSATURN_TOOLCHAIN_ROOT)
if(NOT _nm OR NOT _objdump)
    message(FATAL_ERROR "[package-consumer] sh2eb-elf-nm/objdump not found on PATH")
endif()
execute_process(COMMAND "${_nm}" "${_elf}" OUTPUT_VARIABLE _symbols RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "[package-consumer] nm failed on ${_elf}")
endif()
if(NOT _symbols MATCHES "06004000 [Tt] _start")
    message(FATAL_ERROR "[package-consumer] _start is missing or not at 0x06004000:\n${_symbols}")
endif()
expect_contains("${_symbols}" " _saturn_slave_entry" "slave startup entry")
expect_contains("${_symbols}" " _main" "program entry")

# 10. the post-link constructor guard ran, and also rejects a bad program.
expect_contains("${_build_out}" "LibSaturnCheckNoInitArray.cmake"
    "post-link constructor check must run")
step_run("explicit constructor check on the consumer ELF"
    COMMAND "${CMAKE_COMMAND}" "-DELF=${_elf}"
        "-DOBJDUMP=${_objdump}"
        -P "${PREFIX}/share/libsaturn/cmake/LibSaturnCheckNoInitArray.cmake")
step_run("a program with a static constructor is rejected" EXPECT_FAILURE 1
    OUTPUT_VAR _trap_out
    COMMAND "${CMAKE_COMMAND}" --build "${_consumer}" --target ctor_trap)
expect_contains("${_trap_out}" "[ctors] FAIL" "rejection must come from the constructor guard")
message(STATUS "[package-consumer] contract checks passed")
