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

    # '|' and not '\;': under VERBATIM the escaped separator reaches the script as a literal '\;'
    # and a list of several entries arrives as one path. '|' cannot appear in a path.
    list(JOIN ARG_ROOT_DIRS "|" _dirs)
    list(JOIN ARG_ROOT_FILES "|" _files)
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

# ---- RAM cartridge overlays ----------------------------------------------------------------
# For programs whose read-only data does not all fit the cart at once (see saturn/cart_overlay.h).
# Three steps: generate an ld OVERLAY block, inject it (and any resident cart block) into the
# package's linker script, then split the linked ELF into the app image, the resident cart data
# and one file per overlay member.

# libsaturn_cart_overlay_script(<out.ld>
#     SLOT_ADDRESS <addr>        cart address every member is linked at (e.g. 0x02400000)
#     SLOT_BYTES <n>             the slot size; a member larger than this fails the link
#     PHDR <name>                program header the members go in (declare it with
#                                libsaturn_inject_link_script PHDRS)
#     MEMBERS <name:.sec,.sec>...)
#
# Writes an ld OVERLAY block: every member shares SLOT_ADDRESS and has load address 0x10000000
# (outside the image, so objcopy -O binary leaves them out of app.bin). A member is a section
# `.<name>` that collects the listed input sections, each word-aligned (an OVERLAY member takes no
# SUBALIGN, and a misaligned long read is an address error on the SH-2). Compile with
# -fdata-sections so each table is its own input section. The linker defines ___<name>_start and
# ___<name>_end (C sees __<name>_start / __<name>_end) and asserts the size.
function(libsaturn_cart_overlay_script out)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "SLOT_ADDRESS;SLOT_BYTES;PHDR" "MEMBERS")
    foreach(_a SLOT_ADDRESS SLOT_BYTES PHDR MEMBERS)
        if(NOT ARG_${_a})
            message(FATAL_ERROR "libsaturn_cart_overlay_script: ${_a} is required")
        endif()
    endforeach()
    set(_text "/* generated by libsaturn_cart_overlay_script */\n")
    string(APPEND _text "    OVERLAY ${ARG_SLOT_ADDRESS} : NOCROSSREFS AT (0x10000000) {\n")
    set(_asserts "")
    foreach(_m IN LISTS ARG_MEMBERS)
        if(NOT _m MATCHES "^([A-Za-z_][A-Za-z0-9_]*):(.+)$")
            message(FATAL_ERROR "libsaturn_cart_overlay_script: member '${_m}' is not <name>:<.section,...>")
        endif()
        set(_name "${CMAKE_MATCH_1}")
        string(REPLACE "," ";" _secs "${CMAKE_MATCH_2}")
        set(_body "")
        foreach(_s IN LISTS _secs)
            string(APPEND _body " . = ALIGN(4); *(${_s})")
        endforeach()
        string(APPEND _text "        .${_name} { ___${_name}_start = .;${_body} . = ALIGN(4); ___${_name}_end = .; }\n")
        string(APPEND _asserts "    ASSERT(___${_name}_end - ___${_name}_start <= ${ARG_SLOT_BYTES}, \"overlay ${_name} overflows its slot\")\n")
    endforeach()
    string(APPEND _text "    } :${ARG_PHDR}\n${_asserts}")
    _libsaturn_write_if_changed("${out}" "${_text}")
endfunction()

# libsaturn_inject_link_script(<out.ld>
#     BLOCKS <file>...          linker script statements, put in front of `.text`
#     [PHDRS <name>...])        program headers to declare (each PT_LOAD FLAGS(4))
#
# Writes a copy of LIBSATURN_LINKER_SCRIPT with the blocks injected before `.text`, so they claim
# their input sections before `*(.rodata*)` does (ld's INSERT cannot augment a -T script). The
# caller then sets LIBSATURN_LINKER_SCRIPT to <out.ld> before libsaturn_configure_executable().
function(libsaturn_inject_link_script out)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "BLOCKS;PHDRS")
    if(NOT LIBSATURN_LINKER_SCRIPT OR NOT EXISTS "${LIBSATURN_LINKER_SCRIPT}")
        message(FATAL_ERROR "libsaturn_inject_link_script: linker script '${LIBSATURN_LINKER_SCRIPT}' not found")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${LIBSATURN_LINKER_SCRIPT}" ${ARG_BLOCKS})
    file(READ "${LIBSATURN_LINKER_SCRIPT}" _ld)
    set(_blocks "")
    foreach(_b IN LISTS ARG_BLOCKS)
        file(READ "${_b}" _one)
        string(APPEND _blocks "${_one}\n")
    endforeach()
    set(_anchor "    .text 0x06004000 : {")
    string(FIND "${_ld}" "${_anchor}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR "libsaturn_inject_link_script: saturn.ld has no '${_anchor}'")
    endif()
    string(REPLACE "${_anchor}" "${_blocks}${_anchor}" _ld "${_ld}")
    set(_phdrs "")
    foreach(_p IN LISTS ARG_PHDRS)
        string(APPEND _phdrs "\n    ${_p} PT_LOAD FLAGS(4);")
    endforeach()
    if(_phdrs)
        string(REPLACE "PHDRS {" "PHDRS {${_phdrs}" _ld "${_ld}")
    endif()
    _libsaturn_write_if_changed("${out}" "${_ld}")
endfunction()

# libsaturn_split_cart_image(<target>
#     APP_BIN <path>                 the program image (everything but the cart sections); a plain
#                                    path, generator expressions are not allowed in BYPRODUCTS
#     [MAX_BYTES <n>]                fail when APP_BIN is larger
#     [DATA_SECTION <name> DATA_BIN <path>]   a resident cart section, extracted to its own file
#     [OVERLAYS <name>=<path>...])   one file per libsaturn_cart_overlay_script member
#
# After linking, writes APP_BIN without the cart sections (libsaturn_add_binary would pad the gap
# up to the cart address) and each cart section as a flat file for the disc.
function(libsaturn_split_cart_image target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "APP_BIN;MAX_BYTES;DATA_SECTION;DATA_BIN" "OVERLAYS")
    if(NOT CMAKE_OBJCOPY)
        message(FATAL_ERROR "libsaturn_split_cart_image: CMAKE_OBJCOPY is not set")
    endif()
    if(NOT ARG_APP_BIN)
        message(FATAL_ERROR "libsaturn_split_cart_image: APP_BIN is required")
    endif()
    if(ARG_DATA_SECTION AND NOT ARG_DATA_BIN)
        message(FATAL_ERROR "libsaturn_split_cart_image: DATA_SECTION needs DATA_BIN")
    endif()
    set(_strip "")
    set(_extract "")
    set(_outputs "")
    if(ARG_DATA_SECTION)
        list(APPEND _strip -R "${ARG_DATA_SECTION}")
        list(APPEND _extract COMMAND "${CMAKE_OBJCOPY}" -O binary -j "${ARG_DATA_SECTION}"
                                     "$<TARGET_FILE:${target}>" "${ARG_DATA_BIN}")
        list(APPEND _outputs "${ARG_DATA_BIN}")
    endif()
    foreach(_o IN LISTS ARG_OVERLAYS)
        if(NOT _o MATCHES "^([^=]+)=(.+)$")
            message(FATAL_ERROR "libsaturn_split_cart_image: overlay '${_o}' is not <name>=<path>")
        endif()
        list(APPEND _strip -R ".${CMAKE_MATCH_1}")
        list(APPEND _extract COMMAND "${CMAKE_OBJCOPY}" -O binary -j ".${CMAKE_MATCH_1}"
                                     "$<TARGET_FILE:${target}>" "${CMAKE_MATCH_2}")
        list(APPEND _outputs "${CMAKE_MATCH_2}")
    endforeach()
    set(_check "")
    if(ARG_MAX_BYTES)
        set(_check COMMAND "${CMAKE_COMMAND}" "-DFILE=${ARG_APP_BIN}" "-DMAX_BYTES=${ARG_MAX_BYTES}"
                   -P "${LIBSATURN_CMAKE_SCRIPT_DIR}/LibSaturnCheckImageSize.cmake")
    endif()
    add_custom_command(TARGET ${target} POST_BUILD BYPRODUCTS "${ARG_APP_BIN}" ${_outputs}
        COMMAND "${CMAKE_OBJCOPY}" -O binary ${_strip} "$<TARGET_FILE:${target}>" "${ARG_APP_BIN}"
        ${_extract}
        ${_check}
        COMMENT "Splitting ${target} into the program image and its cart files"
        VERBATIM)
endfunction()

# Rewrites a generated file only when its content changed, so an unchanged script does not
# relink the executable.
function(_libsaturn_write_if_changed path content)
    set(_old "")
    if(EXISTS "${path}")
        file(READ "${path}" _old)
    endif()
    if(NOT _old STREQUAL content)
        get_filename_component(_dir "${path}" DIRECTORY)
        file(MAKE_DIRECTORY "${_dir}")
        file(WRITE "${path}" "${content}")
    endif()
endfunction()
