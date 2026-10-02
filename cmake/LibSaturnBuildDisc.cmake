# Builds a bootable Saturn disc image from a flat application binary.
# Run by libsaturn_add_disc() in script mode:
#
#   cmake -DPYTHON=<python3> -DTOOLS_DIR=<dir> -DMKISOFS=<mkisofs|xorrisofs>
#         -DAPP_BIN=<app.bin> -DOUT_DIR=<dir> -DNAME=<base> -DPROFILE=current|safe
#         -DTEMPLATE=<ip template> [-DROOT_DIRS=<a;b>] [-DROOT_FILES=<a;b>]
#         -P LibSaturnBuildDisc.cmake
#
# Steps mirror the repository Makefile so both paths produce the same disc:
# IP.BIN -> staged root (0.BIN, IP.BIN, extra content) -> ISO -> raw Mode 1
# BIN -> CUE, each validated by the shipped check tools.

foreach(_v PYTHON TOOLS_DIR MKISOFS APP_BIN OUT_DIR NAME PROFILE TEMPLATE)
    if(NOT ${_v})
        message(FATAL_ERROR "LibSaturnBuildDisc: -D${_v}=... is required")
    endif()
endforeach()

function(_disc_run description)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "disc: ${description} failed (${_rc})\n${_out}\n${_err}")
    endif()
    string(STRIP "${_out}" _out)
    if(_out)
        message(STATUS "${_out}")
    endif()
endfunction()

execute_process(COMMAND "${PYTHON}" "${TOOLS_DIR}/memory_layout.py" app_load_hex
    OUTPUT_VARIABLE _load RESULT_VARIABLE _rc OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _rc EQUAL 0 OR NOT _load)
    message(FATAL_ERROR "disc: could not read the application load address")
endif()
set(_load "0x${_load}")
file(SIZE "${APP_BIN}" _app_size)

set(_ip "${OUT_DIR}/${NAME}.IP.BIN")
set(_root "${OUT_DIR}/iso_root")
set(_iso "${OUT_DIR}/${NAME}.iso")
set(_bin "${OUT_DIR}/${NAME}.bin")
set(_cue "${OUT_DIR}/${NAME}.cue")

file(MAKE_DIRECTORY "${OUT_DIR}")
_disc_run("generate IP.BIN"
    "${PYTHON}" "${TOOLS_DIR}/gen_ip_bin.py" --template "${TEMPLATE}" --output "${_ip}"
    --profile "${PROFILE}" --load-addr "${_load}" --first-read-file "${APP_BIN}")
_disc_run("check IP.BIN"
    "${PYTHON}" "${TOOLS_DIR}/check_ip_bin.py" --ip-bin "${_ip}" --template "${TEMPLATE}"
    --expected-size "${_app_size}" --profile "${PROFILE}" --load-addr "${_load}")

file(REMOVE_RECURSE "${_root}")
file(MAKE_DIRECTORY "${_root}")
file(COPY_FILE "${APP_BIN}" "${_root}/0.BIN")
file(COPY_FILE "${_ip}" "${_root}/IP.BIN")
foreach(_dir IN LISTS ROOT_DIRS)
    if(NOT IS_DIRECTORY "${_dir}")
        message(FATAL_ERROR "disc: ROOT_DIRS entry is not a directory: ${_dir}")
    endif()
    file(COPY "${_dir}/" DESTINATION "${_root}")
endforeach()
foreach(_file IN LISTS ROOT_FILES)
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "disc: ROOT_FILES entry is missing: ${_file}")
    endif()
    file(COPY "${_file}" DESTINATION "${_root}")
endforeach()

file(REMOVE "${_iso}")
# Run from the staged root with relative paths: the ISO writer may be an
# MSYS/Cygwin binary that cannot interpret drive-letter paths.
execute_process(
    COMMAND "${MKISOFS}" -quiet -sysid "SEGA SATURN" -volid "LIBSATURN" -volset "LIBSATURN"
            -publisher "LIBSATURN" -preparer "LIBSATURN" -A "LIBSATURN"
            -G "../${NAME}.IP.BIN" -full-iso9660-filenames -o "../${NAME}.iso" .
    WORKING_DIRECTORY "${_root}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0 OR NOT EXISTS "${_iso}")
    message(FATAL_ERROR "disc: create ISO failed (${_rc})
${_out}
${_err}")
endif()
_disc_run("check ISO"
    "${PYTHON}" "${TOOLS_DIR}/check_iso.py" --iso "${_iso}" --expected-size "${_app_size}"
    --profile "${PROFILE}" --load-addr "${_load}")
_disc_run("convert ISO to raw Mode 1"
    "${PYTHON}" "${TOOLS_DIR}/iso_to_raw.py" --iso "${_iso}" --output "${_bin}")
_disc_run("generate CUE"
    "${PYTHON}" "${TOOLS_DIR}/gen_cue.py" --bin-name "${NAME}.bin" --cue-output "${_cue}")
_disc_run("check disc image"
    "${PYTHON}" "${TOOLS_DIR}/check_disc_image.py" --iso "${_iso}" --bin "${_bin}" --cue "${_cue}")
message(STATUS "disc: ${_cue}")
