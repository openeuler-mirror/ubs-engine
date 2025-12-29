file(GLOB SOURCES "*.cpp" "*.h")
add_library(ubse_urma_uvs STATIC ${SOURCES})

target_link_libraries(ubse_urma_uvs PRIVATE ubse_utils ubse_context)