file(GLOB SOURCES "*.cpp" "*.h")
add_library(ubse_storage_message STATIC ${SOURCES})
target_include_directories(ubse_storage_message PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(ubse_storage_message PUBLIC ${SRC_DIR}/storage)

target_compile_definitions(ubse_storage_message PUBLIC "VOS_OS_VER=VOS_LINUX" "VOS_HARDWARE_PLATFORM=VOS_ARM" "VOS_CPU_TYPE=VOS_CORTEXA72")
target_link_libraries(ubse_storage_message PUBLIC ubse_message ubse_utils ubse_serial)