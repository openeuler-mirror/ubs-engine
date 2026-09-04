# check_requirements.cmake --- 依赖检查与统一接入
# 全部链接系统安装的三方库。

# ============ hcom（UBS 通信库）============
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
                sudo yum install 'ubs-comm-devel >= 1.0.0-27'")
endif()

message(STATUS "Found hcom: ${HCOM_INCLUDE_DIR}, ${HCOM_SHARED_LIBRARY}")

# ============ cpp-httplib ============
find_path(CPPHTTPLIB_INCLUDE_DIR
        NAMES httplib.h
        PATHS /usr/include/cpp-httplib /usr/include
        NO_DEFAULT_PATH
)

find_library(CPPHTTPLIB_LIBRARY
        NAMES cpp-httplib libcpp-httplib.so
        PATHS /usr/lib64 /usr/lib /usr/lib/aarch64-linux-gnu
        NO_DEFAULT_PATH
)

if(NOT CPPHTTPLIB_INCLUDE_DIR OR NOT CPPHTTPLIB_LIBRARY)
    message(FATAL_ERROR "cpp-httplib not found! Please execute:
                sudo yum install 'cpp-httplib-devel >= 0.40.0'")
endif()

message(STATUS "Found cpp-httplib: ${CPPHTTPLIB_INCLUDE_DIR}, ${CPPHTTPLIB_LIBRARY}")

# ============ securec（libboundscheck）============
find_path(LIBBOUNDCHECK_INCLUDE_DIR
        NAMES securec.h
        PATHS /usr/include
        NO_DEFAULT_PATH
)

find_library(LIBBOUNDCHECK_SHARED_LIBRARY
        NAMES libboundscheck.so boundscheck
        PATHS /usr/lib64 /usr/lib /usr/lib/aarch64-linux-gnu
        NO_DEFAULT_PATH
)

if(NOT LIBBOUNDCHECK_INCLUDE_DIR OR NOT LIBBOUNDCHECK_SHARED_LIBRARY)
    message(FATAL_ERROR "libboundscheck not found! Please execute:
                sudo yum install libboundscheck-v1.1.11")
endif()

message(STATUS "Found libboundscheck: ${LIBBOUNDCHECK_INCLUDE_DIR}, ${LIBBOUNDCHECK_SHARED_LIBRARY}")

# ============ mockcpp（仅测试使用）============
if (BUILD_TESTS)
    include(mockcpp)
    add_library(mockcpp INTERFACE)
    target_include_directories(mockcpp SYSTEM INTERFACE ${DEPS_DIR}/mockcpp/include)
    target_link_libraries(mockcpp INTERFACE ${DEPS_DIR}/mockcpp/lib/libmockcpp.a)
endif ()
