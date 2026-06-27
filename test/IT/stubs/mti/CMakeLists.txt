# MTI interface mock for IT testing.
# Built as STATIC so --whole-archive can force-include its symbols into ubse_it_daemon,
# overriding the default UbseMtiInterface::GetInstance() at link time.

if(NOT BUILD_TESTS)
    return()
endif()

add_library(ubse_mti_mock STATIC ubse_mti_mock.cpp)
target_include_directories(ubse_mti_mock PRIVATE
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/src/include/adapter_plugins/mti
    ${PROJECT_SOURCE_DIR}/src/adapter_plugins/mti
    ${PROJECT_SOURCE_DIR}/src/adapter_plugins/mti/lcne
    ${PROJECT_SOURCE_DIR}/src/framework/context
    ${PROJECT_SOURCE_DIR}/src/framework/election
    ${CMAKE_BINARY_DIR}/include
)
target_link_libraries(ubse_mti_mock PRIVATE
    ubse_mti
)
