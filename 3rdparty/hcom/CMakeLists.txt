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

if (HCOM_INCLUDE_DIR AND HCOM_SHARED_LIBRARY)
    message(STATUS "Found system-installed HCOM: headers=${HCOM_INCLUDE_DIR}, lib=${HCOM_SHARED_LIBRARY}")
    set(USE_SYSTEM_HCOM ON)
else ()
    message(STATUS "HCOM not found in system. Building from source...")
    set(USE_SYSTEM_HCOM OFF)
endif ()

# -----------------------------
# 如果系统没有，从源码构建 HCOM
# -----------------------------
if (NOT USE_SYSTEM_HCOM)
    include(FetchContent)

    # 假设 ubs-comm 仓库包含 hcom 的实现
    FetchContent_Declare(
            ubs_comm
            GIT_REPOSITORY https://gitcode.com/openeuler/ubs-comm.git
            GIT_TAG f1c36bc3a063495a62616cd54721d84b65318876
    )

    set(HCOM_BUILD_TYPE "release" CACHE STRING "")
    set(HCOM_BUILD_UB ON CACHE BOOL "")
    set(HCOM_BUILD_HTRACER ON CACHE BOOL "")
    set(BUILD_TESTS OFF)

    FetchContent_MakeAvailable(ubs_comm)
    # 获取 hcom 源码路径
    FetchContent_GetProperties(ubs_comm)
    set(HCOM_SRC_DIR ${ubs_comm_SOURCE_DIR}/src/hcom)

    # 复现 INCLUDE_HEADERS 列表（与 hcom/CMakeLists.txt 一致）
    set(HCOM_PUBLIC_HEADERS
            ${HCOM_SRC_DIR}/hcom.h
            ${HCOM_SRC_DIR}/hcom_def.h
            ${HCOM_SRC_DIR}/hcom_err.h
            ${HCOM_SRC_DIR}/hcom_log.h
            ${HCOM_SRC_DIR}/hcom_num_def.h
            ${HCOM_SRC_DIR}/hcom_obj_statistics.h
            ${HCOM_SRC_DIR}/hcom_ref.h
            ${HCOM_SRC_DIR}/hcom_split.h
            ${HCOM_SRC_DIR}/hcom_utils.h
    )

    # service_v2/api 下的头文件
    list(APPEND HCOM_PUBLIC_HEADERS
            ${HCOM_SRC_DIR}/service_v2/api/hcom_service_channel.h
            ${HCOM_SRC_DIR}/service_v2/api/hcom_service_def.h
            ${HCOM_SRC_DIR}/service_v2/api/hcom_service_context.h
            ${HCOM_SRC_DIR}/service_v2/api/hcom_service.h
    )

    # 创建构建时 include/hcom 目录
    set(HCOM_BUILD_INCLUDE_DIR ${CMAKE_BINARY_DIR}/_deps/hcom/include/hcom)
    file(MAKE_DIRECTORY ${HCOM_BUILD_INCLUDE_DIR})

    # 复制每个头文件到 include/hcom/
    foreach (HEADER ${HCOM_PUBLIC_HEADERS})
        if (EXISTS ${HEADER})
            get_filename_component(HEADER_NAME ${HEADER} NAME)
            configure_file(${HEADER} ${HCOM_BUILD_INCLUDE_DIR}/${HEADER_NAME} COPYONLY)
        endif ()
    endforeach ()

    # 修改 hcom 目标的 include 路径
    target_include_directories(hcom SYSTEM BEFORE INTERFACE
            ${CMAKE_BINARY_DIR}/_deps/hcom/include
    )
endif ()

# -----------------------------
# 定义统一的 hcom 接口供上层使用
# -----------------------------
if (USE_SYSTEM_HCOM)
    # 系统模式：创建 INTERFACE 库
    add_library(hcom INTERFACE)
    target_include_directories(hcom SYSTEM INTERFACE ${HCOM_INCLUDE_DIR}/hcom)
    target_link_libraries(hcom INTERFACE ${HCOM_SHARED_LIBRARY})
else ()
    # 源码模式：假设 ubs-comm 导出了 `hcom` 目标
    # 如果没有，需要在这里手动创建 alias 或 INTERFACE
    if (NOT TARGET hcom)
        message(FATAL_ERROR "ubs-comm source must provide a 'hcom' target!")
    endif ()
endif ()
