#if (SOURCE_COMPILING AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/source))
#    set(SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/source)
#
#    add_custom_target(build_securec
#            COMMAND mkdir -p ${DEPS_DIR}/securec
#            COMMAND rm -rf ${DEPS_DIR}/securec/*
#            COMMAND make -C src/
#            COMMAND cp -rv include/ ${DEPS_DIR}/securec/
#            COMMAND cp -rv lib/ ${DEPS_DIR}/securec/
#            COMMAND rm -rf lib/ obj/
#            WORKING_DIRECTORY ${SOURCE_DIR}
#            COMMENT "build securec from source code"
#            VERBATIM
#    )
#
#    add_library(boundscheck INTERFACE)
#    target_include_directories(boundscheck SYSTEM INTERFACE ${DEPS_DIR}/securec/include)
#    target_link_libraries(boundscheck INTERFACE ${DEPS_DIR}/securec/lib/libsecurec.so)
#    install_lib(securec)
#endif ()
#
## -----------------------------
## 查找 libboundscheck 头文件和库
## -----------------------------
#find_path(LIBBOUNDCHECK_INCLUDE_DIR
#        NAMES securec.h
#        PATHS /usr/include
#        NO_DEFAULT_PATH
#)
#
#find_library(LIBBOUNDCHECK_SHARED_LIBRARY
#        NAMES libboundscheck.so boundscheck
#        PATHS /usr/lib64 /usr/lib
#        NO_DEFAULT_PATH
#)
#
## -----------------------------
## 验证是否找到
## -----------------------------
#if (NOT LIBBOUNDCHECK_INCLUDE_DIR OR NOT LIBBOUNDCHECK_SHARED_LIBRARY)
#    message(STATUS "libboundscheck not found, please install")
#endif ()

# -----------------------------
# 查找 libboundscheck 头文件和库
# -----------------------------
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

# -----------------------------
# 验证是否找到
# -----------------------------
if(NOT LIBBOUNDCHECK_INCLUDE_DIR OR NOT LIBBOUNDCHECK_SHARED_LIBRARY)
    message(FATAL_ERROR
            "libboundscheck not found! Please install the RPM package:\n"
            "  sudo rpm -ivh libboundscheck-v1.1.11-6.oe2403.aarch64.rpm\n"
            "Or via yum/dnf:\n"
            "  sudo yum install libboundscheck\n"
            "Expected:\n"
            "  Header: /usr/include/securec.h\n"
            "  Shared: /usr/lib64/libboundscheck.so"
    )
endif()

message(STATUS "Found libboundscheck headers: ${LIBBOUNDCHECK_INCLUDE_DIR}")
message(STATUS "Found libboundscheck shared: ${LIBBOUNDCHECK_SHARED_LIBRARY}")
