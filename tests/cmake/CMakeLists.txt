# CTest suite for the package boundary. Enabled with -DLIBSATURN_BUILD_TESTS=ON.
#
#  - header_smoke_*  every installed public header compiles on its own as C,
#                    and the umbrella header compiles as C and as C++20.
#                    Built with whatever compiler is configured, so the
#                    `host-tests` preset needs no Saturn toolchain.
#  - python tools    the source-manifest and Makefile flag-parity tests.
#  - package_consumer (SH-2 toolchain only) installs the package into a
#                    temporary prefix and links a Saturn ELF against it.

set(_include_dir "${PROJECT_SOURCE_DIR}/include")
file(GLOB _public_headers RELATIVE "${_include_dir}" CONFIGURE_DEPENDS
    "${_include_dir}/saturn/*.h")
list(SORT _public_headers)

set(_smoke_dir "${CMAKE_CURRENT_BINARY_DIR}/header_smoke")
set(_c_sources "")
foreach(_header IN LISTS _public_headers)
    string(MAKE_C_IDENTIFIER "${_header}" _id)
    set(_src "${_smoke_dir}/${_id}.c")
    file(GENERATE OUTPUT "${_src}" CONTENT
"/* Generated: ${_header} must be self-contained when included first. */
#include <${_header}>
int libsaturn_smoke_${_id}(void) { return 0; }
")
    list(APPEND _c_sources "${_src}")
endforeach()

file(GENERATE OUTPUT "${_smoke_dir}/umbrella.c" CONTENT
"#include <saturn/saturn.h>
#if !defined(SATURN_VERSION_STRING)
#error \"umbrella header must provide the version macros\"
#endif
int libsaturn_smoke_umbrella_c(void) { return 0; }
")
file(GENERATE OUTPUT "${_smoke_dir}/umbrella.cpp" CONTENT
"#include <saturn/saturn.h>
static_assert(SATURN_VERSION_NUMBER >= 0, \"version number\");
int libsaturn_smoke_umbrella_cpp() { return 0; }
")

# Static analysis of the headers only: object files, never linked.
add_library(libsaturn_header_smoke OBJECT EXCLUDE_FROM_ALL
    ${_c_sources}
    "${_smoke_dir}/umbrella.c"
    "${_smoke_dir}/umbrella.cpp")
set_target_properties(libsaturn_header_smoke PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_include_directories(libsaturn_header_smoke PRIVATE "${_include_dir}")
if(LIBSATURN_BUILD_RUNTIME)
    # Same ABI flags a consumer inherits from LibSaturn::Core.
    target_compile_options(libsaturn_header_smoke PRIVATE -m2 -mb -ffreestanding)
endif()
target_compile_options(libsaturn_header_smoke PRIVATE -Wall -Wextra -Werror)

add_test(NAME header_smoke
    COMMAND "${CMAKE_COMMAND}" --build "${PROJECT_BINARY_DIR}"
            --target libsaturn_header_smoke --config $<CONFIG>)

# The public tree must never reach into src/.
add_test(NAME public_headers_do_not_include_src
    COMMAND "${CMAKE_COMMAND}"
            "-DINCLUDE_DIR=${_include_dir}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckPublicIncludes.cmake")

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME library_source_manifest
        COMMAND "${Python3_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/tests/tools/test_library_source_manifest.py"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    if(LIBSATURN_BUILD_RUNTIME)
        add_test(NAME makefile_flag_parity
            COMMAND "${Python3_EXECUTABLE}"
                    "${PROJECT_SOURCE_DIR}/tests/tools/test_cmake_flag_parity.py"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
        set_tests_properties(makefile_flag_parity PROPERTIES
            ENVIRONMENT "LIBSATURN_CMAKE_BUILD_DIR=${PROJECT_BINARY_DIR}")
    endif()
else()
    message(STATUS "Python3 not found: skipping the Python manifest/parity tests")
endif()

if(LIBSATURN_BUILD_RUNTIME)
    # Needs the cross toolchain on PATH exactly like the main build does.
    add_test(NAME package_consumer
        COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
                "-DWORK_DIR=${PROJECT_BINARY_DIR}/package-consumer"
                "-DTOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
                "-DGENERATOR=${CMAKE_GENERATOR}"
                -P "${PROJECT_SOURCE_DIR}/tests/package_consumer/run.cmake")
endif()
