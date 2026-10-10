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
# Optional runtimes stay out of programs that never call them (~297 KB music
# slots, ~269 KB asset cache blocks in work RAM Low).
expect_missing("${_symbols}" "g_musicE" "music runtime without sat_music_*")
expect_missing("${_symbols}" "g_asset_cache_blocks" "asset cache blocks without the asset API")

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
# 10b. the linker script keeps room for the Master stack above .bss.
step_run("a program without room for the Master stack is rejected" EXPECT_FAILURE 1
    OUTPUT_VAR _stack_out
    COMMAND "${CMAKE_COMMAND}" --build "${_consumer}" --target stack_trap)
expect_contains("${_stack_out}" "leaves less than __stack_reserve bytes" "rejection must come from the stack check")
# 10c. the cart helpers split an overlay program: one flat file per member, the resident cart
# section on its own, and none of them in the app image.
foreach(_f OVLA.BIN:12:AAAAAAAAAAAA OVLB.BIN:20:BBBBBBBBBBBBBBBBBBBB overlay_consumer.res.bin:8:RRRRRRRR)
    string(REPLACE ":" ";" _p "${_f}")
    list(GET _p 0 _name)
    list(GET _p 1 _size)
    list(GET _p 2 _text)
    if(NOT EXISTS "${_consumer}/${_name}")
        message(FATAL_ERROR "[package-consumer] ${_name} was not produced by libsaturn_split_cart_image")
    endif()
    file(SIZE "${_consumer}/${_name}" _got)
    if(NOT _got EQUAL _size)
        message(FATAL_ERROR "[package-consumer] ${_name} is ${_got} bytes, expected ${_size}")
    endif()
    file(READ "${_consumer}/${_name}" _hex HEX)
    string(HEX "${_text}" _want)
    if(NOT _hex STREQUAL _want)
        message(FATAL_ERROR "[package-consumer] ${_name} holds ${_hex}, expected ${_want}")
    endif()
endforeach()
file(SIZE "${_consumer}/overlay_consumer.app.bin" _ovl_app)
if(_ovl_app GREATER 983040)
    message(FATAL_ERROR "[package-consumer] overlay_consumer.app.bin carries the cart sections (${_ovl_app} bytes)")
endif()
# 11. libsaturn_add_disc() built a bootable disc from the installed tools.
set(_disc "${_consumer}/disc")
foreach(_ext iso bin cue)
    if(NOT EXISTS "${_disc}/consumer.${_ext}")
        message(FATAL_ERROR "[package-consumer] disc output consumer.${_ext} was not produced")
    endif()
endforeach()
file(READ "${_disc}/consumer.cue" _cue)
expect_contains("${_cue}" "consumer.bin" "CUE must name the raw image")
expect_contains("${_cue}" "MODE1/2352" "CUE must declare a Mode 1 track")
file(READ "${_disc}/consumer.iso" _iso_head LIMIT 16 HEX)
expect_contains("${_iso_head}" "53454741205345474153415455524e20" "ISO must start with the Saturn IP header")
file(SIZE "${_consumer}/consumer.app.bin" _app_size)
file(SIZE "${_disc}/iso_root/0.BIN" _boot_size)
if(NOT _app_size EQUAL _boot_size)
    message(FATAL_ERROR "[package-consumer] disc 0.BIN (${_boot_size}) != app image (${_app_size})")
endif()
if(NOT EXISTS "${_disc}/iso_root/CONSUMER.TXT")
    message(FATAL_ERROR "[package-consumer] ROOT_DIRS content missing from the disc root")
endif()
foreach(_f ONE.TXT TWO.TXT)
    if(NOT EXISTS "${_disc}/iso_root/${_f}")
        message(FATAL_ERROR "[package-consumer] ROOT_FILES entry ${_f} missing from the disc root")
    endif()
endforeach()
message(STATUS "[package-consumer] contract checks passed")
