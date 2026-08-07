set(MODULE_NAME ssu_plugin)

file(GLOB TEST_SOURCES "*.cpp" "*.h")

list(FILTER TEST_SOURCES EXCLUDE REGEX "stub_mti_out_of_band.cpp")

add_executable(${CMAKE_PROJECT_NAME}_${MODULE_NAME}_ut ${TEST_SOURCES})

target_include_directories(${CMAKE_PROJECT_NAME}_${MODULE_NAME}_ut PRIVATE
        ${PROJECT_SOURCE_DIR}/src/include
        ${PROJECT_SOURCE_DIR}/src/adapter_plugins/mti/out_of_band
)

target_link_libraries(${CMAKE_PROJECT_NAME}_${MODULE_NAME}_ut PRIVATE
        ubse_mti
        mti_out_of_band
        ubse_log
        ubse_timer
        ssu_plugin
        ubse_ssu
        GTest::gmock_main
        mockcpp
        -Wl,--whole-archive ubse_plugin ubse_vip syssentry -Wl,--no-whole-archive
)

target_compile_features(${CMAKE_PROJECT_NAME}_${MODULE_NAME}_ut PRIVATE cxx_std_17)
target_compile_options(${CMAKE_PROJECT_NAME}_${MODULE_NAME}_ut PRIVATE -fno-access-control -w)

include(GoogleTest)
gtest_add_tests(TARGET ${CMAKE_PROJECT_NAME}_${MODULE_NAME}_ut)
