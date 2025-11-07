# -----------------------------
# 检查 HCOM 是否已通过 RPM 安装到系统
# -----------------------------
find_path(HCOM_INCLUDE_DIR
        NAMES hcom/hcom.h
        PATHS /usr/include
        NO_DEFAULT_PATH
)

find_library(HCOM_SHARED_LIBRARY
        NAMES libhcom.so hcom
        PATHS /usr/lib64 /usr/lib
        NO_DEFAULT_PATH
)

# -----------------------------
# 验证是否找到
# -----------------------------
if(NOT HCOM_INCLUDE_DIR OR NOT HCOM_SHARED_LIBRARY)
    message(FATAL_ERROR
            "HCOM not found! Please install the HCOM RPM package:\n"
            "  rpm -ivh ubs-hcom-devel-2.0.0-1.oe2403sp1.aarch64.rpm\n"
            "Expected:\n"
            "  Header: /usr/include/hcom/hcom.h\n"
            "  Shared: /usr/lib64/libhcom.so"
    )
endif()

message(STATUS "Found HCOM headers: ${HCOM_INCLUDE_DIR}")
message(STATUS "Found HCOM shared: ${HCOM_SHARED_LIBRARY}")

# -----------------------------
# 定义 INTERFACE 库
# -----------------------------

# === hcom (头文件接口) ===
add_library(hcom INTERFACE)
target_include_directories(hcom SYSTEM INTERFACE
        ${HCOM_INCLUDE_DIR}/hcom
)

# === hcom_shared (共享库接口) ===
add_library(hcom_shared INTERFACE)
target_link_libraries(hcom_shared INTERFACE hcom ${HCOM_SHARED_LIBRARY})
