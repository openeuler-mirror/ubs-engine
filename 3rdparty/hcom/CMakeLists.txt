find_path(HCOM_INCLUDE_DIR
        NAMES hcom/hcom.h
        PATHS /usr/include
        NO_DEFAULT_PATH
)
find_library(HCOM_SHARED_LIBRARY
        NAMES hcom libhcom.so
        PATHS /usr/lib64 /usr/lib /usr/lib/aarch64-linux-gnu
        NO_DEFAULT_PATH
)

if (NOT HCOM_INCLUDE_DIR OR NOT HCOM_SHARED_LIBRARY)
    message(FATAL_ERROR "hcom not found! Please execute:
                sudo yum install 'ubs-comm-devel >= 1.0.0-15'")
endif()

add_library(hcom INTERFACE)
target_include_directories(hcom SYSTEM INTERFACE ${HCOM_INCLUDE_DIR})
target_link_libraries(hcom INTERFACE ${HCOM_SHARED_LIBRARY})

message(STATUS "Found hcom: ${HCOM_INCLUDE_DIR}, ${HCOM_SHARED_LIBRARY}")