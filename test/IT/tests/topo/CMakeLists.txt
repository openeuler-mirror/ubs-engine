add_library(it_topo_cases STATIC
    topo_cases.cpp
)
target_include_directories(it_topo_cases PUBLIC
    ${CMAKE_SOURCE_DIR}/test/IT
)
target_link_libraries(it_topo_cases PUBLIC
    ubse_it_infra
)
target_compile_options(it_topo_cases PRIVATE ${DEBUG_FLAGS})
