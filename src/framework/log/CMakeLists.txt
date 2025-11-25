add_library(UbseLog STATIC ubse_logger.cpp ubse_logger_manager.cpp ubse_logger_filesink.cpp ubse_logger_ringbuffer.cpp ubse_logger_filter.cpp)
target_include_directories(UbseLog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(UbseLog PRIVATE ubse_utils)
target_compile_options(UbseLog PRIVATE
        $<$<CONFIG:Release>: -O3>
        $<$<CONFIG:RelWithDebInfo>: -O3>
        $<$<CONFIG:MinSizeRel>: -O3>
)

add_library(ubse_log STATIC ubse_logger_module.cpp ubse_logger_config.cpp ubse_logger_audit.cpp)
target_include_directories(ubse_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ubse_log PRIVATE ubse_utils UbseLog ubse_context ubse_config)
target_compile_options(ubse_log PRIVATE
        $<$<CONFIG:Debug>: -O3 -ldl>
        $<$<CONFIG:Release>: -O3 -ldl>
        $<$<CONFIG:RelWithDebInfo>: -O3 -ldl>
        $<$<CONFIG:MinSizeRel>: -O3 -ldl>
)