add_subdirectory(message)

add_library(ubse_storage STATIC
        ubse_storage.cpp
        ubse_storage_module.cpp
        ubse_storage_req_handler.cpp
)
target_include_directories(ubse_storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ubse_storage PUBLIC ubse_storage_message PRIVATE
        ubse_utils
        ubse_com
        ubse_context
        ubse_election
        ubse_security
)
