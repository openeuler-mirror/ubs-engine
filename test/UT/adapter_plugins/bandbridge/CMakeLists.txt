add_library(ubs-engine_bandbridge_ut_lib OBJECT
    ${CMAKE_CURRENT_SOURCE_DIR}/test_access_device.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_access_ctrlq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mock/mock_kernel_impl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_bandbridge_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_bandbridge_ctrlq.cpp
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_access_device.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_access_ctrlq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mock/mock_kernel_impl.c
    PROPERTIES
    LANGUAGE C
)

target_include_directories(ubs-engine_bandbridge_ut_lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mock
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SRC_DIR}/adapter_plugins/bandbridge
)

target_compile_options(ubs-engine_bandbridge_ut_lib PRIVATE -fno-access-control -w ${DEBUG_FLAGS})

target_link_libraries(ubs-engine_bandbridge_ut_lib PUBLIC
    GTest::gmock
    mockcpp
)

target_link_libraries(${UT_BINARY} PUBLIC ubs-engine_bandbridge_ut_lib)
