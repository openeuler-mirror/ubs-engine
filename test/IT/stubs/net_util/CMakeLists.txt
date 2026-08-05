add_library(ubse_net_util_it_mock STATIC
    ubse_net_util_it_mock.cpp
)

target_include_directories(ubse_net_util_it_mock PRIVATE
    ${PROJECT_SOURCE_DIR}/src/framework/misc
    ${PROJECT_SOURCE_DIR}/src/include
    ${CMAKE_BINARY_DIR}/include
)
