set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(cert_api_static STATIC ubse_cert_cli_import.cpp ubse_cert_file_utils.cpp)
target_link_libraries(cert_api_static PRIVATE ubse_utils)
target_include_directories(cert_api_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})