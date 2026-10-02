# cmake -DFILE=<image> -DMAX_BYTES=<n> -P LibSaturnCheckImageSize.cmake
if(NOT FILE OR NOT MAX_BYTES)
    message(FATAL_ERROR "usage: cmake -DFILE=<image> -DMAX_BYTES=<n> -P ${CMAKE_CURRENT_LIST_FILE}")
endif()
file(SIZE "${FILE}" _size)
if(_size GREATER MAX_BYTES)
    message(FATAL_ERROR "${FILE} is ${_size} bytes; maximum is ${MAX_BYTES} bytes")
endif()
message(STATUS "${FILE}: ${_size} bytes (limit ${MAX_BYTES})")
