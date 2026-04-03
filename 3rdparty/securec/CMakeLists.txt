find_path(SECUREC_INCLUDE_DIR securec.h PATHS /usr/include NO_DEFAULT_PATH)
find_library(SECUREC_LIBRARY libboundscheck.so PATHS /usr/lib64 /usr/lib NO_DEFAULT_PATH)

if(NOT SECUREC_INCLUDE_DIR OR NOT SECUREC_LIBRARY)
    message(FATAL_ERROR "libboundscheck not found! Please execute:
                sudo yum install libboundscheck-v1.1.11")
endif()

message(STATUS "Found libboundscheck: ${SECUREC_INCLUDE_DIR}, ${SECUREC_LIBRARY}")
