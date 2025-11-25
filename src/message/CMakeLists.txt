add_library(ubse_message STATIC ubse_base_message.cpp)
target_link_libraries(ubse_message
        PRIVATE ubse_utils
)
