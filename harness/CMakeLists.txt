# harness/CMakeLists.txt — GPL-3.0 (see harness/LICENSE and harness/README.md
# for why this directory is licensed separately from the rest of the repo).
#
# Builds a headless probe that links Ymir's emulator core (libs/ymir-core) to
# boot a real libsaturn-built .iso and dump VDP1/VDP2 register/VRAM state to
# JSON for assertion by harness/tests/*.py.
#
# Ymir's source is fetched, not vendored: this repo never commits a copy of
# it. Pinned to a specific commit rather than `main` because Ymir is under
# active development (commits daily as of writing) and an untracked upstream
# change should not silently change what this harness asserts.
cmake_minimum_required(VERSION 3.28)
project(libsaturn_harness CXX)

include(FetchContent)

# Pin: StrikerX3/Ymir @ main, captured 2026-07-31. Bump deliberately, not by
# accident — re-verify harness/tests still pass after moving this.
set(YMIR_PIN_COMMIT "5c571f680e3606de08813af4de43dc00e90cc6e3")

# ymir-core is a library-only consumer of Ymir: no SDL3 frontend, no ImGui
# debugger UI, no tests/tools app. Ymir's own CMakeLists.txt defaults
# Ymir_LIBRARY_ONLY to ON automatically whenever it is NOT the top-level
# CMake project (i.e. whenever pulled in via FetchContent/add_subdirectory,
# as here) — this is set explicitly anyway so the intent survives even if
# that default ever changes upstream.
set(Ymir_LIBRARY_ONLY ON CACHE BOOL "" FORCE)
set(Ymir_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(Ymir_ENABLE_SANDBOX OFF CACHE BOOL "" FORCE)
set(Ymir_ENABLE_YMDASM OFF CACHE BOOL "" FORCE)
set(Ymir_ENABLE_YMIR_HEADLESS OFF CACHE BOOL "" FORCE)
set(Ymir_ENABLE_YMIR_DBG OFF CACHE BOOL "" FORCE)
set(Ymir_ENABLE_UPDATE_CHECKS OFF CACHE BOOL "" FORCE)

# LTO is disabled: GCC 15.2.0 (MSYS2 UCRT64, the toolchain this repo already
# uses for the SH2 cross build) hits a codegen bug under -flto when linking
# ymir-core — "two or more sections for .gnu.lto_..." on a moodycamel
# ConcurrentQueue template instantiation, a known class of GCC/MinGW LTO
# duplicate-section bug. This only affects optimization, not correctness.
set(Ymir_ENABLE_IPO OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    ymir
    GIT_REPOSITORY https://github.com/StrikerX3/Ymir.git
    GIT_TAG ${YMIR_PIN_COMMIT}
    GIT_SUBMODULES_RECURSE TRUE
)
FetchContent_MakeAvailable(ymir)

# The previous three overlapping patches could not be reverse-checked once
# the whole stack was applied. Keep one atomic diff against the pinned source:
# write counters, write watch, misaligned accesses and per-step CPU costs.
# These are harness diagnostics, outside the public runtime API.
foreach(YMIR_DEBUG_PATCH_NAME ymir_harness_debug)
    set(YMIR_DEBUG_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/${YMIR_DEBUG_PATCH_NAME}.patch")
    execute_process(
        COMMAND git -C "${ymir_SOURCE_DIR}" apply --check --reverse "${YMIR_DEBUG_PATCH}"
        RESULT_VARIABLE YMIR_DEBUG_PATCH_ALREADY_APPLIED
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT YMIR_DEBUG_PATCH_ALREADY_APPLIED EQUAL 0)
        execute_process(
            COMMAND git -C "${ymir_SOURCE_DIR}" apply --check "${YMIR_DEBUG_PATCH}"
            RESULT_VARIABLE YMIR_DEBUG_PATCH_CHECK)
        if(NOT YMIR_DEBUG_PATCH_CHECK EQUAL 0)
            message(FATAL_ERROR "${YMIR_DEBUG_PATCH_NAME}.patch no longer applies cleanly")
        endif()
        execute_process(
            COMMAND git -C "${ymir_SOURCE_DIR}" apply "${YMIR_DEBUG_PATCH}"
            RESULT_VARIABLE YMIR_DEBUG_PATCH_APPLY)
        if(NOT YMIR_DEBUG_PATCH_APPLY EQUAL 0)
            message(FATAL_ERROR "Failed to apply ${YMIR_DEBUG_PATCH_NAME}.patch")
        endif()
    endif()
endforeach()

add_executable(probe src/probe_main.cpp)
target_link_libraries(probe PRIVATE ymir::ymir-core)
target_compile_features(probe PRIVATE cxx_std_20)

enable_testing()
add_executable(test_video_observation tests/video_observation_test.cpp)
target_include_directories(test_video_observation PRIVATE src)
target_compile_features(test_video_observation PRIVATE cxx_std_20)
add_test(NAME composited_video_observation COMMAND test_video_observation)
add_executable(test_cpu_profile tests/test_cpu_profile.cpp)
target_include_directories(test_cpu_profile PRIVATE src)
target_compile_features(test_cpu_profile PRIVATE cxx_std_20)
add_test(NAME cpu_profile_accounting COMMAND test_cpu_profile)
add_executable(test_cpu_profile_emulator tests/test_cpu_profile_emulator.cpp)
target_include_directories(test_cpu_profile_emulator PRIVATE src)
target_link_libraries(test_cpu_profile_emulator PRIVATE ymir::ymir-core)
target_compile_features(test_cpu_profile_emulator PRIVATE cxx_std_20)
add_test(NAME cpu_profile_emulator COMMAND test_cpu_profile_emulator)

# Statically link the MinGW runtime so probe.exe runs standalone (no
# libstdc++-6.dll / libgcc_s_seh-1.dll / libwinpthread-1.dll on PATH needed) —
# it gets invoked from run-harness.ps1, which does not run inside the MSYS2
# shell that build-example.ps1 uses for the SH2 cross build.
if(MINGW)
    target_link_options(probe PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
