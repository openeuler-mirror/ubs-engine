find_package(OpenSSL REQUIRED)
add_library(ubse_cert STATIC ubse_cert_validator.cpp)
target_include_directories(ubse_cert PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cert/)
target_link_libraries(ubse_cert
        PUBLIC  OpenSSL::SSL OpenSSL::Crypto
        PRIVATE ubse_log ubse_utils ubse_context ubse_config
)