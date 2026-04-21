find_package(RapidJSON QUIET)

if(NOT RapidJSON_FOUND)
    message(FATAL_ERROR "RapidJSON not found! Please execute:
                sudo yum install 'rapidjson-devel >= 1.1.0'")
endif()

message(STATUS "RapidJSON found. Headers: ${RapidJSON_INCLUDE_DIRS}")

add_library(rapidjson INTERFACE)
target_include_directories(rapidjson SYSTEM INTERFACE ${RAPIDJSON_INCLUDE_DIRS})