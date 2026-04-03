find_path(CPPHTTPLIB_INCLUDE_DIR
        NAMES httplib.h
        PATHS /usr/include/cpp-httplib /usr/include
        NO_DEFAULT_PATH
)

find_library(CPPHTTPLIB_LIBRARY
        NAMES cpp-httplib libcpp-httplib.so
        PATHS /usr/lib64 /usr/lib
        NO_DEFAULT_PATH
)

if(NOT CPPHTTPLIB_INCLUDE_DIR OR NOT CPPHTTPLIB_LIBRARY)
    message(FATAL_ERROR "cpp-httplib not found! Please execute:
                sudo yum install cpp-httplib-devel-0.27.0")
endif()

message(STATUS "Found cpp-httplib: ${CPPHTTPLIB_INCLUDE_DIR}, ${CPPHTTPLIB_LIBRARY}")

add_library(cpp-httplib INTERFACE)
target_include_directories(cpp-httplib SYSTEM INTERFACE ${CPPHTTPLIB_INCLUDE_DIR})
target_link_libraries(cpp-httplib INTERFACE ${CPPHTTPLIB_LIBRARY})