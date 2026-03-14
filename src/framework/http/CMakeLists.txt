file(GLOB SOURCES "*.cpp" "*.h")
find_package(OpenSSL REQUIRED)
add_library(ubse_http STATIC ${SOURCES})
target_include_directories(ubse_http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cert/)
target_link_libraries(ubse_http
        PUBLIC cpp-httplib
        PRIVATE ubse_cert ubse_log ubse_utils ubse_context ubse_config ubse_security
)