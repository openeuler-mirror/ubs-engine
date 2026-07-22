add_library(it_mem_borrow_cases STATIC
    mem_borrow_cases.cpp
)
target_include_directories(it_mem_borrow_cases PUBLIC
    ${CMAKE_SOURCE_DIR}/test/IT
)
target_link_libraries(it_mem_borrow_cases PUBLIC
    ubse_it_infra
)
target_compile_options(it_mem_borrow_cases PRIVATE ${DEBUG_FLAGS})
