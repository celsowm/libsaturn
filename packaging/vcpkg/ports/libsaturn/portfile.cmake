# EXPERIMENTAL spike. Builds the repository checkout named by LIBSATURN_SOURCE_DIR
# (a published port would use vcpkg_from_github with a release tag instead).
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

if(NOT DEFINED ENV{LIBSATURN_SOURCE_DIR})
    message(FATAL_ERROR "set LIBSATURN_SOURCE_DIR to a LibSaturn checkout for this spike")
endif()
set(SOURCE_PATH "$ENV{LIBSATURN_SOURCE_DIR}")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DLIBSATURN_BUILD_TESTS=OFF
        -DLIBSATURN_BUILD_EXAMPLES=OFF
        -DLIBSATURN_BUILD_HOST_TOOLS=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME LibSaturn CONFIG_PATH lib/cmake/LibSaturn)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
