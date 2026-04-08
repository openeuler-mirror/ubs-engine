include(FetchContent)

# --- 配置部分 ---
set(CPPHTTPLIB_REPO "https://gitcode.com/GitHub_Trending/cp/cpp-httplib.git")
set(CPPHTTPLIB_TAG "v0.27.0")
set(CPPHTTPLIB_LOCAL_TAR_GZ "${CMAKE_SOURCE_DIR}/.deps/cpp-httplib-v0.27.0.tar.gz")

# 定义最终解压目标目录
set(CPPHTTPLIB_EXTRACTED_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps_src/cpp-httplib-extracted-v0.27.0")

if(EXISTS ${CPPHTTPLIB_LOCAL_TAR_GZ})
    message(STATUS ">>> [cpp-httplib] Found local archive: ${CPPHTTPLIB_LOCAL_TAR_GZ}")
    message(STATUS ">>> [cpp-httplib] Extracting directly to target...")
    file(MAKE_DIRECTORY ${CPPHTTPLIB_EXTRACTED_SOURCE_DIR})
    file(ARCHIVE_EXTRACT
            INPUT ${CPPHTTPLIB_LOCAL_TAR_GZ}
            DESTINATION ${CPPHTTPLIB_EXTRACTED_SOURCE_DIR}
    )
    # --- 本地优先：声明使用本地源码目录 ---
    FetchContent_Declare(
            build-cpp-httplib
            SOURCE_DIR ${CPPHTTPLIB_EXTRACTED_SOURCE_DIR}/cpp-httplib-v0.27.0
    )
else()
    message(STATUS "[cpp-httplib] Local archive not found, using Git: ${CPPHTTPLIB_REPO}")
    FetchContent_Declare(
            build-cpp-httplib
            GIT_REPOSITORY ${CPPHTTPLIB_REPO}
            GIT_TAG ${CPPHTTPLIB_TAG}
    )
endif()

# 使用 MakeAvailable (会自动调用 Populate)
FetchContent_MakeAvailable(build-cpp-httplib)

# 直接使用 FetchContent 提供的变量
add_library(cpp-httplib INTERFACE)
target_include_directories(cpp-httplib INTERFACE
        ${build-cpp-httplib_SOURCE_DIR}
)