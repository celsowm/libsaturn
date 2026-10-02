# Single authoritative project version.
#
# The repository VERSION file is the only place the version is edited. This
# module feeds it to project(), the installed LibSaturnConfigVersion.cmake,
# the Conan recipe (which parses the same file) and <saturn/version.h>.
#
# include/saturn/version.h is checked in so a plain source checkout (the
# Makefile workflow) has a working public header without running CMake.
# libsaturn_check_version_header() fails the configure step when it drifts
# from VERSION; `cmake -P cmake/SyncVersion.cmake` regenerates it.

set(LIBSATURN_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." CACHE INTERNAL "")
get_filename_component(LIBSATURN_ROOT "${LIBSATURN_ROOT}" ABSOLUTE)

# Reads VERSION into LIBSATURN_VERSION / _MAJOR / _MINOR / _PATCH in the
# caller's scope. Only plain MAJOR.MINOR.PATCH is accepted so the same string
# is valid for CMake's project(VERSION) and for Conan.
macro(libsaturn_read_version)
    file(STRINGS "${LIBSATURN_ROOT}/VERSION" _libsaturn_version_line LIMIT_COUNT 1)
    string(STRIP "${_libsaturn_version_line}" LIBSATURN_VERSION)
    if(NOT LIBSATURN_VERSION MATCHES "^([0-9]+)\.([0-9]+)\.([0-9]+)$")
        message(FATAL_ERROR
            "VERSION must contain MAJOR.MINOR.PATCH, found '${LIBSATURN_VERSION}'")
    endif()
    set(LIBSATURN_VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(LIBSATURN_VERSION_MINOR "${CMAKE_MATCH_2}")
    set(LIBSATURN_VERSION_PATCH "${CMAKE_MATCH_3}")
endmacro()

# Renders the header template to <out_file>.
function(libsaturn_render_version_header out_file)
    libsaturn_read_version()
    configure_file("${LIBSATURN_ROOT}/cmake/version.h.in" "${out_file}" @ONLY)
endfunction()

# Fails when the checked-in header is not what VERSION would generate.
function(libsaturn_check_version_header)
    set(_expected "${CMAKE_CURRENT_BINARY_DIR}/libsaturn_version_expected.h")
    libsaturn_render_version_header("${_expected}")
    file(READ "${_expected}" _expected_text)
    set(_checked_in "${LIBSATURN_ROOT}/include/saturn/version.h")
    if(NOT EXISTS "${_checked_in}")
        message(FATAL_ERROR
            "include/saturn/version.h is missing; run: cmake -P cmake/SyncVersion.cmake")
    endif()
    file(READ "${_checked_in}" _actual_text)
    # A Windows checkout with autocrlf stores the header with CRLF.
    string(REPLACE "
" "
" _actual_text "${_actual_text}")
    if(NOT _actual_text STREQUAL _expected_text)
        message(FATAL_ERROR
            "include/saturn/version.h is out of date with VERSION; "
            "run: cmake -P cmake/SyncVersion.cmake")
    endif()
endfunction()
