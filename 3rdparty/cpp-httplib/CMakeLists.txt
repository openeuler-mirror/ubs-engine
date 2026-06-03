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

if((NOT CPPHTTPLIB_INCLUDE_DIR OR NOT CPPHTTPLIB_LIBRARY) AND NOT SOURCE_COMPILING)
    message(FATAL_ERROR "cpp-httplib not found! Please execute:
                sudo yum install 'cpp-httplib-devel >= 0.40.0'")
endif()

if (SOURCE_COMPILING)
    set(CPPHTTPLIB_REPO "https://gitcode.com/GitHub_Trending/cp/cpp-httplib.git")
    set(CPPHTTPLIB_TAG "v0.40.0")
    FetchContent_Declare(
            build-cpp-httplib
            GIT_REPOSITORY ${CPPHTTPLIB_REPO}
            GIT_TAG ${CPPHTTPLIB_TAG}
    )
    FetchContent_MakeAvailable(build-cpp-httplib)
    add_library(cpp-httplib INTERFACE)
    target_include_directories(cpp-httplib INTERFACE
            ${build-cpp-httplib_SOURCE_DIR}
    )
elseif ()
    message(STATUS "Found cpp-httplib: ${CPPHTTPLIB_INCLUDE_DIR}, ${CPPHTTPLIB_LIBRARY}")
    add_library(cpp-httplib INTERFACE)
    target_include_directories(cpp-httplib SYSTEM INTERFACE ${CPPHTTPLIB_INCLUDE_DIR})
    target_link_libraries(cpp-httplib INTERFACE ${CPPHTTPLIB_LIBRARY})
endif ()