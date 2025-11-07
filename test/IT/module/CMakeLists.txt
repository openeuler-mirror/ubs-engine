add_library(ubse_mock STATIC
        mock_module.cpp
        mock_dependency_module.cpp
)
target_include_directories(ubse_mock PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ubse_mock PUBLIC ubse_context)
