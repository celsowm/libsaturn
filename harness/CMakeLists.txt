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

add_executable(probe src/probe_main.cpp)
target_link_libraries(probe PRIVATE ymir::ymir-core)
target_compile_features(probe PRIVATE cxx_std_20)

# Statically link the MinGW runtime so probe.exe runs standalone (no
# libstdc++-6.dll / libgcc_s_seh-1.dll / libwinpthread-1.dll on PATH needed) —
# it gets invoked from run-harness.ps1, which does not run inside the MSYS2
# shell that build-example.ps1 uses for the SH2 cross build.
if(MINGW)
    target_link_options(probe PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
