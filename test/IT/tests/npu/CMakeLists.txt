add_library(it_npu_cases STATIC
    npu_cases.cpp
)
target_include_directories(it_npu_cases PUBLIC
    ${CMAKE_SOURCE_DIR}/test/IT
)
target_link_libraries(it_npu_cases PUBLIC
    ubse_it_infra
)
target_compile_options(it_npu_cases PRIVATE ${DEBUG_FLAGS})
