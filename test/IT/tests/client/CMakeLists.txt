add_library(it_client_cases STATIC
    client_cases.cpp
)
target_include_directories(it_client_cases PUBLIC
    ${CMAKE_SOURCE_DIR}/test/IT
)
target_link_libraries(it_client_cases PUBLIC
    ubse_it_infra
)
target_compile_options(it_client_cases PRIVATE ${DEBUG_FLAGS})
