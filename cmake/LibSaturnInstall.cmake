# Install rules and package export. Included from the root CMakeLists.txt.
#
# Layout contract (see docs/PACKAGING_AND_LIBRARY_DISTRIBUTION_PLAN.md):
#   include/saturn/*.h
#   lib/libsaturn.a  lib/libsaturn_startup.a
#   lib/cmake/LibSaturn/LibSaturn{Config,ConfigVersion,Targets}.cmake
#   share/libsaturn/linker/saturn.ld
#   share/libsaturn/cmake/LibSaturnFunctions.cmake (+ check scripts)
#   share/libsaturn/tools/*.py  share/libsaturn/boot/ip_yaul_template.bin
#   share/libsaturn/toolchains/sh2eb-elf.cmake

include(CMakePackageConfigHelpers)

# Fixed on purpose: the contract must not move to lib64 on some hosts.
set(LIBSATURN_INSTALL_INCLUDEDIR include)
set(LIBSATURN_INSTALL_LIBDIR lib)
set(LIBSATURN_INSTALL_CMAKEDIR lib/cmake/LibSaturn)
set(LIBSATURN_INSTALL_SHAREDIR share/libsaturn)

install(TARGETS libsaturn_core libsaturn_startup libsaturn_runtime
    EXPORT LibSaturnTargets
    ARCHIVE DESTINATION ${LIBSATURN_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${LIBSATURN_INSTALL_INCLUDEDIR})

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/saturn"
    DESTINATION ${LIBSATURN_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h")

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/core/startup/saturn.ld"
    DESTINATION ${LIBSATURN_INSTALL_SHAREDIR}/linker)

install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibSaturnFunctions.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibSaturnCheckNoInitArray.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibSaturnCheckImageSize.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibSaturnBuildDisc.cmake"
    DESTINATION ${LIBSATURN_INSTALL_SHAREDIR}/cmake)

# Disc tooling for libsaturn_add_disc(): the same checked scripts and IP
# template the repository Makefile uses.
install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/memory_layout.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_ip_bin.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_ip_bin.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_iso.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/iso_to_raw.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_cue.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_disc_image.py"
    DESTINATION ${LIBSATURN_INSTALL_SHAREDIR}/tools)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/boot/ip_yaul_template.bin"
    DESTINATION ${LIBSATURN_INSTALL_SHAREDIR}/boot)

# The reference toolchain file ships with the package: a consumer must name it
# at the first configure, before find_package() can run, but it knows the prefix.
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/sh2eb-elf.cmake"
    DESTINATION ${LIBSATURN_INSTALL_SHAREDIR}/toolchains)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    DESTINATION ${LIBSATURN_INSTALL_SHAREDIR})

install(EXPORT LibSaturnTargets
    NAMESPACE LibSaturn::
    DESTINATION ${LIBSATURN_INSTALL_CMAKEDIR})

# Locations the generated config resolves relative to its own position, so an
# installed prefix can be moved or relocated by a package manager.
set(LIBSATURN_INSTALL_LINKER_SCRIPT
    "${CMAKE_INSTALL_PREFIX}/${LIBSATURN_INSTALL_SHAREDIR}/linker/saturn.ld")
set(LIBSATURN_INSTALL_DISC_TOOLS_DIR
    "${CMAKE_INSTALL_PREFIX}/${LIBSATURN_INSTALL_SHAREDIR}/tools")
set(LIBSATURN_INSTALL_DISC_BOOT_DIR
    "${CMAKE_INSTALL_PREFIX}/${LIBSATURN_INSTALL_SHAREDIR}/boot")
set(LIBSATURN_INSTALL_SCRIPT_DIR
    "${CMAKE_INSTALL_PREFIX}/${LIBSATURN_INSTALL_SHAREDIR}/cmake")
set(LIBSATURN_INSTALL_INCLUDEDIR_ABS
    "${CMAKE_INSTALL_PREFIX}/${LIBSATURN_INSTALL_INCLUDEDIR}")

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibSaturnConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/LibSaturnConfig.cmake"
    INSTALL_DESTINATION ${LIBSATURN_INSTALL_CMAKEDIR}
    PATH_VARS LIBSATURN_INSTALL_INCLUDEDIR_ABS LIBSATURN_INSTALL_LINKER_SCRIPT
              LIBSATURN_INSTALL_SCRIPT_DIR LIBSATURN_INSTALL_DISC_TOOLS_DIR
              LIBSATURN_INSTALL_DISC_BOOT_DIR)

# 0.x minor releases may break the installed contract; from 1.0 on only a
# major bump may.
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(_libsaturn_compat SameMinorVersion)
else()
    set(_libsaturn_compat SameMajorVersion)
endif()
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/LibSaturnConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ${_libsaturn_compat}
    ARCH_INDEPENDENT)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/LibSaturnConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/LibSaturnConfigVersion.cmake"
    DESTINATION ${LIBSATURN_INSTALL_CMAKEDIR})
