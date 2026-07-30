add_library(ubse_com_engine_it_mock STATIC
    ubse_com_engine_it_mock.cpp
)

target_include_directories(ubse_com_engine_it_mock PRIVATE
    ${PROJECT_SOURCE_DIR}/src/framework/com
    ${PROJECT_SOURCE_DIR}/src/framework/misc
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/src/include/adapter_plugins/smbios
    ${PROJECT_SOURCE_DIR}/test/IT/infra/spec
    ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(ubse_com_engine_it_mock PRIVATE
    hcom
)

add_library(ubse_mem_controller_api_it_mock STATIC
    ubse_mem_controller_api_it_mock.cpp
)

target_include_directories(ubse_mem_controller_api_it_mock PRIVATE
    ${PROJECT_SOURCE_DIR}/test/IT/infra/spec
)
