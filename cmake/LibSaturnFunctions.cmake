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
