add_library(syssentry STATIC
        sys_sentry_module.cpp
        sentry_observer.cpp
)
target_include_directories(syssentry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(syssentry PRIVATE ubse_utils ubse_mti ubse_ras ubse_com ubse_context ubse_http)