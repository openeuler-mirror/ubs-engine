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

# -----------------------------
# 定义 INTERFACE 库
# -----------------------------

# === libboundscheck (头文件 + 库) ===
add_library(libboundscheck INTERFACE)

# 添加头文件路径
target_include_directories(libboundscheck SYSTEM INTERFACE
        ${LIBBOUNDCHECK_INCLUDE_DIR}
)

# 链接共享库
target_link_libraries(libboundscheck INTERFACE ${LIBBOUNDCHECK_SHARED_LIBRARY})
