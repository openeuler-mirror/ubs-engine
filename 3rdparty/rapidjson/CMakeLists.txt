include(FetchContent)

set(RAPIDJSON_REPO "https://gitcode.com/GitHub_Trending/ra/rapidjson.git")
set(RAPIDJSON_LOCAL_ZIP "${CMAKE_SOURCE_DIR}/.deps/rapidjson-6089180ecb704cb2b136777798fa1be303618975.zip")
set(RAPIDJSON_TAG "6089180ecb704cb2b136777798fa1be303618975")
set(RAPIDJSON_EXTRACTED_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps_src/rapidjson-extracted-6089180ecb704cb2b136777798fa1be303618975")

if(EXISTS ${RAPIDJSON_LOCAL_ZIP})
    message(STATUS ">>> [RapidJSON] Found local archive: ${RAPIDJSON_LOCAL_ZIP}")
    message(STATUS ">>> [RapidJSON] Extracting to ${RAPIDJSON_EXTRACTED_ROOT_DIR}...")
    file(MAKE_DIRECTORY ${RAPIDJSON_EXTRACTED_SOURCE_DIR})
    file(ARCHIVE_EXTRACT
            INPUT ${RAPIDJSON_LOCAL_ZIP}
            DESTINATION ${RAPIDJSON_EXTRACTED_SOURCE_DIR}
    )
    message(STATUS ">>> [RapidJSON] Extraction complete.")
    FetchContent_Declare(
            build-rapidjson
            SOURCE_DIR ${RAPIDJSON_EXTRACTED_SOURCE_DIR}/rapidjson-6089180ecb704cb2b136777798fa1be303618975
    )
else()
    message(STATUS "[RapidJSON] Local archive not found, using Git: ${RAPIDJSON_REPO}")
    FetchContent_Declare(
            build-rapidjson
            GIT_REPOSITORY ${RAPIDJSON_REPO}
            GIT_TAG ${RAPIDJSON_TAG}
            GIT_SUBMODULES ""
    )
endif()

FetchContent_GetProperties(build-rapidjson)

if(NOT rapidjson_POPULATED)
    FetchContent_Populate(build-rapidjson)
endif()

add_library(rapidjson INTERFACE)
target_include_directories(rapidjson INTERFACE
        ${build-rapidjson_SOURCE_DIR}/include
)