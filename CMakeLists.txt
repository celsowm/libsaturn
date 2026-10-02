# LibSaturn - bare-metal game library for the Sega Saturn.
#
# This project is the canonical description of an *installed* LibSaturn:
# headers, the Core/Startup archives, the linker script and the CMake package
# a game consumes with
#
#     find_package(LibSaturn CONFIG REQUIRED)
#     target_link_libraries(my_game PRIVATE LibSaturn::Saturn)
#     libsaturn_configure_executable(my_game)
#
# Package managers (Conan, ...) only transport what this file installs. The
# repository Makefile still owns examples, asset baking and disc images; it
# reads the same source manifest (cmake/LibSaturnSources.cmake).
#
# Build for the console:
#     cmake --preset saturn-release && cmake --build --preset saturn-release
#     cmake --install build/saturn-release --prefix <prefix>

cmake_minimum_required(VERSION 3.24)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(LibSaturnVersion)
libsaturn_read_version()

option(LIBSATURN_BUILD_RUNTIME
    "Build and install the Saturn runtime (needs the SH-2 toolchain)" ON)

if(LIBSATURN_BUILD_RUNTIME)
    project(LibSaturn VERSION ${LIBSATURN_VERSION} LANGUAGES C CXX ASM
        DESCRIPTION "Bare-metal game library for the Sega Saturn")
else()
    # Host-only configuration: header smoke tests with the native compiler.
    project(LibSaturn VERSION ${LIBSATURN_VERSION} LANGUAGES C CXX
        DESCRIPTION "Bare-metal game library for the Sega Saturn")
endif()

# Consumers get the same defaults whether LibSaturn is the top-level project
# or reached through add_subdirectory()/FetchContent.
set(_libsaturn_is_top_level OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_libsaturn_is_top_level ON)
endif()

option(LIBSATURN_BUILD_TESTS
    "Build the header smoke tests and package-consumer test" OFF)
option(LIBSATURN_BUILD_EXAMPLES
    "Reserved: examples are built by the Makefile (needs asset tooling)" OFF)
option(LIBSATURN_BUILD_HOST_TOOLS
    "Reserved: host asset tools are not part of the runtime package" OFF)
option(LIBSATURN_ENABLE_PROFILE_METRICS
    "Compile frame/parallel profiling counters into the runtime" OFF)
set(LIBSATURN_PARALLEL_TEST_FAULT "0" CACHE STRING
    "Parallel executor fault injection mode (0 = off, test builds only)")
option(LIBSATURN_INSTALL "Generate install rules" ${_libsaturn_is_top_level})

if(LIBSATURN_BUILD_EXAMPLES OR LIBSATURN_BUILD_HOST_TOOLS)
    message(WARNING "LIBSATURN_BUILD_EXAMPLES / LIBSATURN_BUILD_HOST_TOOLS are "
        "reserved; build examples and asset tools with the Makefile for now.")
endif()

libsaturn_check_version_header()

# Never fall back to an unoptimized default build.
get_property(_libsaturn_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _libsaturn_multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(LIBSATURN_BUILD_TESTS)
    # tests/tools/test_cmake_flag_parity.py compares these compile commands
    # with the Makefile's.
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

if(LIBSATURN_BUILD_RUNTIME)
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^sh2")
        message(FATAL_ERROR
            "LibSaturn's runtime is SH-2 code. Configure with\n"
            "  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/sh2eb-elf.cmake\n"
            "(CMAKE_SYSTEM_PROCESSOR is '${CMAKE_SYSTEM_PROCESSOR}'). To test "
            "headers on the host only, use -DLIBSATURN_BUILD_RUNTIME=OFF.")
    endif()

    include(LibSaturnSources)

    set(LIBSATURN_PREFIXED_SOURCES "")
    foreach(_group CORE GRAPHICS AUDIO HAL INPUT PHYSICS RESOURCES STORAGE)
        foreach(_src IN LISTS LIBSATURN_${_group}_SOURCES)
            list(APPEND LIBSATURN_PREFIXED_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
        endforeach()
    endforeach()
    set(LIBSATURN_PREFIXED_STARTUP "")
    foreach(_src IN LISTS LIBSATURN_STARTUP_SOURCES)
        list(APPEND LIBSATURN_PREFIXED_STARTUP "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    endforeach()

    # -- LibSaturn::Core ------------------------------------------------
    add_library(libsaturn_core STATIC ${LIBSATURN_PREFIXED_SOURCES})
    add_library(LibSaturn::Core ALIAS libsaturn_core)
    set_target_properties(libsaturn_core PROPERTIES
        OUTPUT_NAME saturn
        EXPORT_NAME Core
        C_EXTENSIONS ON
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

    target_include_directories(libsaturn_core
        PUBLIC
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
            "$<INSTALL_INTERFACE:include>"
        PRIVATE
            # Runtime sources include each other as "src/<subsystem>/...".
            "${CMAKE_CURRENT_SOURCE_DIR}")

    # The ABI/runtime model every translation unit linked with the runtime
    # has to share, so consumers inherit it from the target.
    target_compile_options(libsaturn_core
        PUBLIC
            -m2 -mb -ffreestanding -fomit-frame-pointer
            "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti;-fno-threadsafe-statics;-fno-use-cxa-atexit>"
        PRIVATE
            # Always -O2, whatever the build type: the runtime relies on GCC
            # constant-folding MMIO references (tools/check_no_static_ctors.py)
            # and the Makefile has always built it this way. Later flags win
            # over CMake's per-config -O3/-O0.
            -O2 -Wall -Wextra)
    # The compiler runtime (software division, 64-bit shifts, ...) must follow
    # libsaturn.a on every link line, so it is part of the Core link interface.
    target_link_libraries(libsaturn_core PUBLIC gcc)

    if(LIBSATURN_ENABLE_PROFILE_METRICS)
        set(_libsaturn_profile 1)
    else()
        set(_libsaturn_profile 0)
    endif()
    target_compile_definitions(libsaturn_core PRIVATE
        SAT_PROFILE_METRICS=${_libsaturn_profile}
        SAT_PARALLEL_TEST_FAULT=${LIBSATURN_PARALLEL_TEST_FAULT})

    # -- LibSaturn::Startup ---------------------------------------------
    add_library(libsaturn_startup STATIC ${LIBSATURN_PREFIXED_STARTUP})
    add_library(LibSaturn::Startup ALIAS libsaturn_startup)
    set_target_properties(libsaturn_startup PROPERTIES
        OUTPUT_NAME saturn_startup
        EXPORT_NAME Startup
        LINKER_LANGUAGE C)
    target_compile_options(libsaturn_startup PUBLIC -m2 -mb)

    # -- LibSaturn::Saturn ----------------------------------------------
    # The complete game runtime. crt0/ip_stub/slave_entry are only reached
    # from the linker script and the BIOS, never from C, so a normal archive
    # link would drop them; the startup archive is therefore linked
    # whole-archive. CMake's $<LINK_LIBRARY:WHOLE_ARCHIVE> feature is not
    # defined for a bare-metal system, so the option is spelled out. The
    # startup archive stays an ordinary link dependency as well, which gives
    # build ordering and keeps it behind the objects that need its symbols.
    add_library(libsaturn_runtime INTERFACE)
    add_library(LibSaturn::Saturn ALIAS libsaturn_runtime)
    set_target_properties(libsaturn_runtime PROPERTIES EXPORT_NAME Saturn)
    target_link_libraries(libsaturn_runtime INTERFACE libsaturn_startup libsaturn_core)
    target_link_options(libsaturn_runtime INTERFACE
        "LINKER:--whole-archive,$<TARGET_FILE:LibSaturn::Startup>,--no-whole-archive")

    # In-tree builds (add_subdirectory) use the checked-out linker script and
    # helpers exactly as an installed prefix would provide them.
    set(LIBSATURN_LINKER_SCRIPT
        "${CMAKE_CURRENT_SOURCE_DIR}/src/core/startup/saturn.ld")
    set(LIBSATURN_CMAKE_SCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    include(LibSaturnFunctions)

    if(LIBSATURN_INSTALL)
        include(cmake/LibSaturnInstall.cmake)
    endif()
endif()

if(LIBSATURN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/cmake)
endif()
