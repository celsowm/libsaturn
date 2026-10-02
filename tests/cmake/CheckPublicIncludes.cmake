# Public headers are a compatibility promise: they may include other public
# headers (<saturn/...> or "saturn/...") and the C standard library, but never
# anything from src/, a relative path out of the tree, or an .hpp.
#   cmake -DINCLUDE_DIR=<repo>/include -P CheckPublicIncludes.cmake
if(NOT INCLUDE_DIR)
    message(FATAL_ERROR "usage: cmake -DINCLUDE_DIR=<dir> -P ${CMAKE_CURRENT_LIST_FILE}")
endif()

file(GLOB_RECURSE _headers "${INCLUDE_DIR}/*.h" "${INCLUDE_DIR}/*.hpp")
if(NOT _headers)
    message(FATAL_ERROR "no headers found below ${INCLUDE_DIR}")
endif()

set(_bad "")
foreach(_h IN LISTS _headers)
    file(STRINGS "${_h}" _lines REGEX "^[ \t]*#[ \t]*include")
    foreach(_line IN LISTS _lines)
        if(_line MATCHES "src/" OR _line MATCHES "\\.\\./" OR _line MATCHES "\\.hpp"
           OR _line MATCHES "examples/")
            list(APPEND _bad "${_h}: ${_line}")
        endif()
    endforeach()
endforeach()

if(_bad)
    list(JOIN _bad "\n  " _text)
    message(FATAL_ERROR "public headers include private files:\n  ${_text}")
endif()
list(LENGTH _headers _count)
message(STATUS "${_count} public headers include only public files")
