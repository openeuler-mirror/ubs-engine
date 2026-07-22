add_library(it_smoke_cases STATIC
    smoke_cases.cpp
)
target_include_directories(it_smoke_cases PUBLIC
    ${CMAKE_SOURCE_DIR}/test/IT
)
target_link_libraries(it_smoke_cases PUBLIC
    ubse_it_infra
)
target_compile_options(it_smoke_cases PRIVATE ${DEBUG_FLAGS})
