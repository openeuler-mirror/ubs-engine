file(GLOB_RECURSE SRC_FILES *.cpp)
add_library(ubse_cli_framework STATIC ${SRC_FILES})
target_include_directories(ubse_cli_framework PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

