# Executable helpers shipped with the LibSaturn package.
#
# Installed to <prefix>/share/libsaturn/cmake and loaded by
# LibSaturnConfig.cmake, so consumers never need to know physical paths.
# Callers (the in-tree build or the package config) set
#   LIBSATURN_LINKER_SCRIPT      the saturn.ld to link with
#   LIBSATURN_CMAKE_SCRIPT_DIR   directory holding the check scripts

include_guard(GLOBAL)

# libsaturn_configure_executable(<target>
#     [MAP_FILE <path>]      default: <target file>.map
#     [NO_CTOR_CHECK])       skip the post-link .init_array validation
#
# Owns the Saturn application link contract: the SH-2 machine flags,
# -nostdlib, the package's linker script, section garbage collection, a link
# map, and the guard that rejects
# executables whose static constructors crt0 would never run.
function(libsaturn_configure_executable target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "NO_CTOR_CHECK" "MAP_FILE" "")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "libsaturn_configure_executable: unknown arguments ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT TARGET ${target})
        message(FATAL_ERROR "libsaturn_configure_executable: '${target}' is not a target")
    endif()
    if(NOT LIBSATURN_LINKER_SCRIPT OR NOT EXISTS "${LIBSATURN_LINKER_SCRIPT}")
        message(FATAL_ERROR
            "libsaturn_configure_executable: linker script '${LIBSATURN_LINKER_SCRIPT}' not found")
    endif()

    # Linking the runtime here makes the helper correct even if the caller
    # forgot. libgcc comes with LibSaturn::Core so it follows libsaturn.a.
    target_link_libraries(${target} PRIVATE LibSaturn::Saturn)

    set(_map "${ARG_MAP_FILE}")
    if(NOT _map)
        set(_map "$<TARGET_FILE:${target}>.map")
    endif()
    target_link_options(${target} PRIVATE
        -m2 -mb -nostdlib
        "-Wl,-T,${LIBSATURN_LINKER_SCRIPT}"
        "-Wl,-Map,${_map}"
        -Wl,--gc-sections)
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${LIBSATURN_LINKER_SCRIPT}")
    set_property(TARGET ${target} PROPERTY SUFFIX ".elf")

    if(NOT ARG_NO_CTOR_CHECK)
        if(NOT CMAKE_OBJDUMP)
            message(FATAL_ERROR
                "libsaturn_configure_executable: CMAKE_OBJDUMP is not set; use the "
                "sh2eb-elf toolchain file or pass NO_CTOR_CHECK")
        endif()
        # crt0 never runs static constructors, so a global that needs one is
        # silently left null. Fail the link instead of shipping that.
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                    "-DELF=$<TARGET_FILE:${target}>"
                    "-DOBJDUMP=${CMAKE_OBJDUMP}"
                    -P "${LIBSATURN_CMAKE_SCRIPT_DIR}/LibSaturnCheckNoInitArray.cmake"
            COMMENT "Checking ${target} for static constructors"
            VERBATIM)
    endif()
endfunction()

# libsaturn_add_binary(<target>
#     [OUTPUT <path>]        default: <target file without suffix>.app.bin
#     [MAX_BYTES <n>])       fail when the image is larger (Saturn 0.BIN limit)
#
# Converts the linked ELF into the flat image the BIOS loads. Disc creation
# is deliberately separate from linking.
function(libsaturn_add_binary target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "OUTPUT;MAX_BYTES" "")
    if(NOT CMAKE_OBJCOPY)
        message(FATAL_ERROR "libsaturn_add_binary: CMAKE_OBJCOPY is not set")
    endif()
    set(_out "${ARG_OUTPUT}")
    if(NOT _out)
        set(_out "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_BASE_NAME:${target}>.app.bin")
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${target}>" "${_out}"
        COMMENT "Creating Saturn binary image for ${target}"
        VERBATIM)
    if(ARG_MAX_BYTES)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                    "-DFILE=${_out}" "-DMAX_BYTES=${ARG_MAX_BYTES}"
                    -P "${LIBSATURN_CMAKE_SCRIPT_DIR}/LibSaturnCheckImageSize.cmake"
            VERBATIM)
    endif()
endfunction()

# libsaturn_add_disc(<target>
#     [NAME <base>]            file base name, default <target>
#     [OUTPUT_DIR <dir>]       default <binary dir>/disc
#     [IP_PROFILE current|safe]  default current
#     [IP_TEMPLATE <path>]     default: the package's yaul IP template
#     [ROOT_DIRS <dir>...]     directories whose contents land in the disc root
#     [ROOT_FILES <file>...]   individual files placed in the disc root
#     [DEPENDS <item>...]      extra build dependencies of the disc
#     [ALL])                   build it as part of the default target
#
# Creates the custom target <target>_disc, producing <NAME>.iso/.bin/.cue from
# the <target> executable (which must use libsaturn_add_binary). Needs a Python
# 3 interpreter and mkisofs, genisoimage or xorrisofs.
function(libsaturn_add_disc target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "ALL"
        "NAME;OUTPUT_DIR;IP_PROFILE;IP_TEMPLATE"
        "ROOT_DIRS;ROOT_FILES;DEPENDS")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "libsaturn_add_disc: unknown arguments ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT TARGET ${target})
        message(FATAL_ERROR "libsaturn_add_disc: '${target}' is not a target")
    endif()
    if(NOT LIBSATURN_DISC_TOOLS_DIR OR NOT IS_DIRECTORY "${LIBSATURN_DISC_TOOLS_DIR}")
        message(FATAL_ERROR
            "libsaturn_add_disc: disc tools directory '${LIBSATURN_DISC_TOOLS_DIR}' not found")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    find_program(LIBSATURN_MKISOFS NAMES mkisofs genisoimage xorrisofs
        DOC "ISO 9660 image writer used by libsaturn_add_disc")
    if(NOT LIBSATURN_MKISOFS)
        message(FATAL_ERROR
            "libsaturn_add_disc: mkisofs, genisoimage or xorrisofs not found; "
            "set LIBSATURN_MKISOFS")
    endif()

    set(_name "${ARG_NAME}")
    if(NOT _name)
        set(_name "${target}")
    endif()
    set(_dir "${ARG_OUTPUT_DIR}")
    if(NOT _dir)
        set(_dir "${CMAKE_CURRENT_BINARY_DIR}/disc")
    endif()
    set(_profile "${ARG_IP_PROFILE}")
    if(NOT _profile)
        set(_profile current)
    endif()
    if(NOT _profile STREQUAL "current" AND NOT _profile STREQUAL "safe")
        message(FATAL_ERROR "libsaturn_add_disc: IP_PROFILE must be current or safe")
    endif()
    set(_template "${ARG_IP_TEMPLATE}")
    if(NOT _template)
        set(_template "${LIBSATURN_DISC_BOOT_DIR}/ip_yaul_template.bin")
    endif()
    if(NOT EXISTS "${_template}")
        message(FATAL_ERROR "libsaturn_add_disc: IP template '${_template}' not found")
    endif()

    set(_content_deps ${ARG_ROOT_FILES})
    foreach(_d IN LISTS ARG_ROOT_DIRS)
        file(GLOB_RECURSE _found CONFIGURE_DEPENDS "${_d}/*")
        list(APPEND _content_deps ${_found})
    endforeach()

    list(JOIN ARG_ROOT_DIRS "\;" _dirs)
    list(JOIN ARG_ROOT_FILES "\;" _files)
    add_custom_command(
        OUTPUT "${_dir}/${_name}.cue"
        COMMAND "${CMAKE_COMMAND}"
                "-DPYTHON=${Python3_EXECUTABLE}"
                "-DTOOLS_DIR=${LIBSATURN_DISC_TOOLS_DIR}"
                "-DMKISOFS=${LIBSATURN_MKISOFS}"
                "-DAPP_BIN=$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_BASE_NAME:${target}>.app.bin"
                "-DOUT_DIR=${_dir}" "-DNAME=${_name}" "-DPROFILE=${_profile}"
                "-DTEMPLATE=${_template}"
                "-DROOT_DIRS=${_dirs}" "-DROOT_FILES=${_files}"
                -P "${LIBSATURN_CMAKE_SCRIPT_DIR}/LibSaturnBuildDisc.cmake"
        DEPENDS ${target} ${_content_deps} ${ARG_DEPENDS}
        COMMENT "Creating Saturn disc image ${_name}"
        VERBATIM)
    if(ARG_ALL)
        set(_all ALL)
    endif()
    add_custom_target(${target}_disc ${_all} DEPENDS "${_dir}/${_name}.cue")
endfunction()
