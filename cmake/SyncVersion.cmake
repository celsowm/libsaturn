# Regenerates include/saturn/version.h from the VERSION file.
#   cmake -P cmake/SyncVersion.cmake
cmake_minimum_required(VERSION 3.21)
set(CMAKE_CURRENT_BINARY_DIR "${CMAKE_CURRENT_LIST_DIR}/../build")
include("${CMAKE_CURRENT_LIST_DIR}/LibSaturnVersion.cmake")
libsaturn_render_version_header("${LIBSATURN_ROOT}/include/saturn/version.h")
file(STRINGS "${LIBSATURN_ROOT}/VERSION" _v LIMIT_COUNT 1)
message(STATUS "Wrote include/saturn/version.h for ${_v}")
