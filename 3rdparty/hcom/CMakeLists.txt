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

if ((NOT HCOM_INCLUDE_DIR OR NOT HCOM_SHARED_LIBRARY) AND NOT SOURCE_COMPILING)
    message(FATAL_ERROR "hcom not found! Please execute:
                sudo yum install 'ubs-comm-devel >= 1.0.1-7'")
endif()

if (SOURCE_COMPILING)
    include(FetchContent)

    # ubs-comm 仓库包含 hcom 的实现
    FetchContent_Declare(
            ubs_comm
            GIT_REPOSITORY https://gitcode.com/openeuler/ubs-comm.git
            GIT_TAG master
    )

    # 只下载不构建
    FetchContent_GetProperties(ubs_comm)

    if(NOT ubs_comm_POPULATED)
        FetchContent_Populate(ubs_comm)  # 只下载
        message(STATUS "Downloaded ubs_comm to: ${ubs_comm_SOURCE_DIR}")
    endif()

    # 构建hcom
    set(HCOM_DIST ${ubs_comm_SOURCE_DIR}/dist/hcom)
    set(HCOM_LIB ${HCOM_DIST}/lib/libhcom.so)

    add_custom_command(OUTPUT ${HCOM_LIB}
            COMMAND env HCOM_BUILD_UB=on HCOM_BUILD_RDMA=off bash build.sh
            WORKING_DIRECTORY ${ubs_comm_SOURCE_DIR}
            VERBATIM
    )
    add_custom_target(build_hcom ALL DEPENDS ${HCOM_LIB})

    add_library(hcom INTERFACE)
    target_include_directories(hcom SYSTEM INTERFACE ${HCOM_DIST}/include)
    target_link_libraries(hcom INTERFACE ${HCOM_LIB})
    add_dependencies(hcom build_hcom)
elseif ()
    add_library(hcom INTERFACE)
    target_include_directories(hcom SYSTEM INTERFACE ${HCOM_INCLUDE_DIR})
    target_link_libraries(hcom INTERFACE ${HCOM_SHARED_LIBRARY})
endif ()

message(STATUS "Found hcom: ${HCOM_INCLUDE_DIR}, ${HCOM_SHARED_LIBRARY}")