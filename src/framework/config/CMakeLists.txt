file(GLOB SOURCE "*.cpp")
add_library(ubse_config STATIC ${SOURCE})
target_include_directories(ubse_config PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(ubse_config PRIVATE ubse_utils ubse_security ubse_context)