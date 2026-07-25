add_library(it_election_cases STATIC
    election_cases.cpp
)
target_include_directories(it_election_cases PUBLIC
    ${CMAKE_SOURCE_DIR}/test/IT
)
target_link_libraries(it_election_cases PUBLIC
    ubse_it_infra
)
target_compile_options(it_election_cases PRIVATE ${DEBUG_FLAGS})
