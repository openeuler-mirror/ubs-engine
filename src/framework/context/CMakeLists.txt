add_library(ubse_context STATIC ubse_context.cpp)
target_include_directories(ubse_context PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ubse_context PRIVATE ubse_utils)
