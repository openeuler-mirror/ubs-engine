# -----------------------------
# 尝试查找系统已安装的 HCOM (RPM 模式)
# -----------------------------
find_path(HCOM_INCLUDE_DIR
        NAMES hcom/hcom.h
        PATHS /usr/include
        NO_DEFAULT_PATH
)

find_library(HCOM_SHARED_LIBRARY
        NAMES hcom libhcom.so
        PATHS /usr/lib64 /usr/lib
        NO_DEFAULT_PATH
)

if(HCOM_INCLUDE_DIR AND HCOM_SHARED_LIBRARY)
    message(STATUS "Found system-installed HCOM: headers=${HCOM_INCLUDE_DIR}, lib=${HCOM_SHARED_LIBRARY}")
    set(USE_SYSTEM_HCOM ON)
else()
    message(STATUS "HCOM not found in system. Building from source...")
    set(USE_SYSTEM_HCOM OFF)
endif()

# -----------------------------
# 如果系统没有，从源码构建 HCOM
# -----------------------------
if(NOT USE_SYSTEM_HCOM)
    include(FetchContent)

    # 假设 ubs-comm 仓库包含 hcom 的实现
    FetchContent_Declare(
            ubs_comm
            GIT_REPOSITORY https://gitee.com/openeuler/ubs-comm.git
            GIT_TAG        master
    )

    set(HCOM_BUILD_TYPE "release" CACHE STRING "")
    set(HCOM_BUILD_UB ON CACHE BOOL "")
    set(HCOM_BUILD_HTRACER ON CACHE BOOL "")

    FetchContent_MakeAvailable(ubs_comm)
endif()

# -----------------------------
# 定义统一的 hcom 接口供上层使用
# -----------------------------
if(USE_SYSTEM_HCOM)
    # 系统模式：创建 INTERFACE 库
    add_library(hcom INTERFACE)
    target_include_directories(hcom SYSTEM INTERFACE ${HCOM_INCLUDE_DIR}/hcom)
    target_link_libraries(hcom INTERFACE ${HCOM_SHARED_LIBRARY})
else()
    # 源码模式：假设 ubs-comm 导出了 `hcom` 目标
    # 如果没有，需要在这里手动创建 alias 或 INTERFACE
    if(NOT TARGET hcom)
        message(FATAL_ERROR "ubs-comm source must provide a 'hcom' target!")
    endif()
endif()
