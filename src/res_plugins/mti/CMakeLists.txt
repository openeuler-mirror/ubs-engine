file(GLOB SOURCES "*.cpp" "*.h")
file(GLOB SOURCESLCNE "${CMAKE_CURRENT_SOURCE_DIR}/lcne/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/lcne/*.h")
add_library(ubse_mti STATIC
        ${SOURCES}
        ${SOURCESLCNE}
)
target_include_directories(ubse_mti PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../../controllers/include)

target_link_libraries(ubse_mti PRIVATE ubse_election ubse_xml ubse_utils ubse_com ubse_context ubse_log ubse_http ubse_config)