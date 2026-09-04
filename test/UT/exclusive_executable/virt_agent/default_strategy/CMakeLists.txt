find_package(RapidJSON REQUIRED)

add_ut(strategy)
target_link_libraries(${CMAKE_PROJECT_NAME}_strategy_ut PUBLIC
        ubse_utils ubse_log
)
