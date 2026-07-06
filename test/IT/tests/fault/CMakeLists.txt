add_library(it_fault_cases STATIC
    fault_cases.cpp
)
target_include_directories(it_fault_cases PUBLIC
    ${CMAKE_SOURCE_DIR}/test/IT
)
target_link_libraries(it_fault_cases PUBLIC
    ubse_it_infra
)
target_compile_options(it_fault_cases PRIVATE ${DEBUG_FLAGS})
