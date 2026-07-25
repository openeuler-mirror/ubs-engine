add_library(it_urma_qos_cases STATIC
    urma_qos_cases.cpp
)
target_include_directories(it_urma_qos_cases PUBLIC
    ${CMAKE_SOURCE_DIR}/test/IT
)
target_link_libraries(it_urma_qos_cases PUBLIC
    ubse_it_infra
)
target_compile_options(it_urma_qos_cases PRIVATE ${DEBUG_FLAGS})
