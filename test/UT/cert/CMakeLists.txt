add_ut(cert_api_static)

target_link_libraries(${CMAKE_PROJECT_NAME}_cert_api_static_ut
    PRIVATE
    ubse_cert
)