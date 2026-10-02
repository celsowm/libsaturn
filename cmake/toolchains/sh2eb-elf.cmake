# CMake toolchain file for the Sega Saturn SH-2 (big-endian, bare metal).
#
#   cmake -S . -B build/saturn -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/sh2eb-elf.cmake
#
# This file owns target and compiler identity only. Target flags (-m2 -mb,
# -ffreestanding, ...) belong to the LibSaturn targets and to
# libsaturn_configure_executable(), never to global CMAKE_*_FLAGS, so a game
# that uses its own toolchain file still gets the right options.
#
# Overrides:
#   LIBSATURN_TOOLCHAIN_PREFIX  triplet prefix, default sh2eb-elf
#   LIBSATURN_TOOLCHAIN_ROOT    directory holding <prefix>-gcc (also read from
#                               the environment); PATH is searched otherwise

if(NOT DEFINED LIBSATURN_TOOLCHAIN_PREFIX)
    if(DEFINED ENV{LIBSATURN_TOOLCHAIN_PREFIX})
        set(LIBSATURN_TOOLCHAIN_PREFIX "$ENV{LIBSATURN_TOOLCHAIN_PREFIX}")
    else()
        set(LIBSATURN_TOOLCHAIN_PREFIX "sh2eb-elf")
    endif()
endif()
if(NOT DEFINED LIBSATURN_TOOLCHAIN_ROOT AND DEFINED ENV{LIBSATURN_TOOLCHAIN_ROOT})
    set(LIBSATURN_TOOLCHAIN_ROOT "$ENV{LIBSATURN_TOOLCHAIN_ROOT}")
endif()

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR sh2)

# A full path needs the executable suffix on Windows; a bare name found on
# PATH does not.
set(_libsaturn_exe "")
if(DEFINED LIBSATURN_TOOLCHAIN_ROOT)
    set(_libsaturn_tool_prefix "${LIBSATURN_TOOLCHAIN_ROOT}/${LIBSATURN_TOOLCHAIN_PREFIX}")
    if(CMAKE_HOST_WIN32)
        set(_libsaturn_exe ".exe")
    endif()
else()
    set(_libsaturn_tool_prefix "${LIBSATURN_TOOLCHAIN_PREFIX}")
endif()

set(CMAKE_C_COMPILER   "${_libsaturn_tool_prefix}-gcc${_libsaturn_exe}")
set(CMAKE_CXX_COMPILER "${_libsaturn_tool_prefix}-g++${_libsaturn_exe}")
set(CMAKE_ASM_COMPILER "${_libsaturn_tool_prefix}-gcc${_libsaturn_exe}")
set(CMAKE_AR           "${_libsaturn_tool_prefix}-ar${_libsaturn_exe}")
set(CMAKE_RANLIB       "${_libsaturn_tool_prefix}-ranlib${_libsaturn_exe}")
set(CMAKE_OBJCOPY      "${_libsaturn_tool_prefix}-objcopy${_libsaturn_exe}")
set(CMAKE_OBJDUMP      "${_libsaturn_tool_prefix}-objdump${_libsaturn_exe}")
set(CMAKE_NM           "${_libsaturn_tool_prefix}-nm${_libsaturn_exe}")
set(CMAKE_SIZE_UTIL    "${_libsaturn_tool_prefix}-size${_libsaturn_exe}")

# There is no hosted OS to run a try-compile executable on, and the Saturn
# link needs a linker script, so compiler checks build a static library.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Never pick up host libraries, headers or packages. Installed LibSaturn
# prefixes are found through CMAKE_PREFIX_PATH / CMAKE_FIND_ROOT_PATH.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
