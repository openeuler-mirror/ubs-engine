add_library(test_exclusive_main STATIC ${PROJECT_SOURCE_DIR}/test/UT/main.cpp)
target_link_libraries(test_exclusive_main PRIVATE boundscheck GTest::gmock_main)

add_subdirectory(ucache)
add_subdirectory(rmrs)
add_subdirectory(virt_agent)
