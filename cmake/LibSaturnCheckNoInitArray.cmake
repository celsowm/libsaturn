# Post-link guard: fail when a Saturn ELF contains static constructors.
# CMake-native equivalent of tools/check_no_static_ctors.py, so a package
# consumer needs no Python.
#
#   cmake -DELF=<file> -DOBJDUMP=<sh2eb-elf-objdump> -P LibSaturnCheckNoInitArray.cmake
#
# crt0.s jumps straight to the program and saturn.ld has no .init_array /
# .ctors pass, so a namespace-scope global that needs dynamic initialization
# would stay zero. The usual cause is a reference or pointer bound to a
# reinterpret_cast; make it a macro or inline accessor instead.

if(NOT ELF OR NOT OBJDUMP)
    message(FATAL_ERROR "usage: cmake -DELF=<elf> -DOBJDUMP=<objdump> -P ${CMAKE_CURRENT_LIST_FILE}")
endif()

execute_process(COMMAND "${OBJDUMP}" -h "${ELF}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "[ctors] cannot read ${ELF}: ${_err}")
endif()

string(REPLACE "\n" ";" _lines "${_out}")
set(_offenders "")
foreach(_line IN LISTS _lines)
    if(_line MATCHES "^[ \t]*[0-9]+[ \t]+(\\.init_array|\\.ctors|\\.preinit_array)[ \t]+([0-9a-fA-F]+)")
        set(_name "${CMAKE_MATCH_1}")
        math(EXPR _size "0x${CMAKE_MATCH_2}")
        if(_size GREATER 0)
            math(EXPR _count "${_size} / 4")
            list(APPEND _offenders "  ${_name}: ${_size} bytes = ${_count} static constructor(s) that will never run")
        endif()
    endif()
endforeach()

if(_offenders)
    list(JOIN _offenders "\n" _text)
    message(FATAL_ERROR
        "[ctors] FAIL ${ELF}\n${_text}\n"
        "  A namespace-scope global needs dynamic initialization and crt0 never "
        "runs constructors. Make the initializer a constant expression.")
endif()
message(STATUS "[ctors] ${ELF}: no static constructors")
