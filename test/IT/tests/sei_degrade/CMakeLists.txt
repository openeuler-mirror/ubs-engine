add_library(it_sei_degrade_cases STATIC
    sei_degrade_cases.cpp
)
target_include_directories(it_sei_degrade_cases PUBLIC
    ${CMAKE_SOURCE_DIR}/test/IT
)
target_link_libraries(it_sei_degrade_cases PUBLIC
    ubse_it_infra
)
target_compile_options(it_sei_degrade_cases PRIVATE ${DEBUG_FLAGS})
