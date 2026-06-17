# SMBIOS mock library for IT testing.
# Built as STATIC so --whole-archive can force-include its symbols into ubse_it_daemon,
# overriding the real ubse_smbios symbols at link time.
# Only built when BUILD_TESTS=ON.

if(NOT BUILD_TESTS)
    return()
endif()

add_library(ubse_smbios_mock STATIC ubse_smbios_mock.cpp)
target_include_directories(ubse_smbios_mock PRIVATE
    ${PROJECT_SOURCE_DIR}/src/include/adapter_plugins/mti
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/src
)