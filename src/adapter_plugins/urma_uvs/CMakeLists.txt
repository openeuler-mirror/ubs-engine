file(GLOB SOURCES "*.cpp" "*.h")
add_library(ubse_urma_uvs STATIC ${SOURCES})

target_include_directories(ubse_urma_uvs PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(ubse_urma_uvs PRIVATE ubse_utils ubse_context)