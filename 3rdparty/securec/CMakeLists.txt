find_path(LIBBOUNDCHECK_INCLUDE_DIR
        NAMES securec.h
        PATHS /usr/include
        NO_DEFAULT_PATH
)

find_library(LIBBOUNDCHECK_SHARED_LIBRARY
        NAMES libboundscheck.so boundscheck
        PATHS /usr/lib64 /usr/lib
        NO_DEFAULT_PATH
)

if(NOT LIBBOUNDCHECK_INCLUDE_DIR OR NOT LIBBOUNDCHECK_SHARED_LIBRARY)
    message(FATAL_ERROR "libboundscheck not found! Please execute:
                sudo yum install libboundscheck-v1.1.11")
endif()

message(STATUS "Found libboundscheck: ${SECUREC_INCLUDE_DIR}, ${SECUREC_LIBRARY}")
