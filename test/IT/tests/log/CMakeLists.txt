add_library(it_log_cases STATIC
    log_cases.cpp
)
target_include_directories(it_log_cases PUBLIC
    ${CMAKE_SOURCE_DIR}/test/IT
    ${CMAKE_SOURCE_DIR}/src/framework/ipc/include
)
target_link_libraries(it_log_cases PUBLIC
    ubse_it_infra
)
target_compile_options(it_log_cases PRIVATE ${DEBUG_FLAGS})