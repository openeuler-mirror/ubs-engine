file(GLOB SOURCES "*.cpp" "*.h")
add_library(ubse_ubturbo STATIC ${SOURCES})

target_link_libraries(ubse_ubturbo PRIVATE ubse_utils ubse_context)