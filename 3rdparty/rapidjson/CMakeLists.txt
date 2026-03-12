set(RAPID_JSON_VERSION 6089180ecb704cb2b136777798fa1be303618975)

include(FetchContent)

FetchContent_Declare(
        build-rapidjson
        GIT_REPOSITORY https://gitcode.com/GitHub_Trending/ra/rapidjson.git
        GIT_TAG  ${RAPID_JSON_VERSION}
        GIT_SUBMODULES ""
)

# 只下载不构建
FetchContent_GetProperties(build-rapidjson)
if(NOT rapidjson_POPULATED)
    FetchContent_Populate(build-rapidjson)  # 只下载
endif()

# 创建导入目标（RapidJSON 是头文件库）
add_library(rapidjson INTERFACE)
message(STATUS "rapidjson: ${build-rapidjson_SOURCE_DIR}")
target_include_directories(rapidjson INTERFACE
        ${build-rapidjson_SOURCE_DIR}/include
)
