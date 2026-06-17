add_library(ubse_com_engine_it_mock STATIC
    ubse_com_engine_it_mock.cpp
)

target_include_directories(ubse_com_engine_it_mock PRIVATE
    ${PROJECT_SOURCE_DIR}/src/framework/com
    ${PROJECT_SOURCE_DIR}/src/framework/misc
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/src/include/adapter_plugins/smbios
    ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(ubse_com_engine_it_mock PRIVATE
    hcom
)
