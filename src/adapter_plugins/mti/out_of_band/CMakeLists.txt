file(GLOB SRC *.cpp "${CMAKE_CURRENT_SOURCE_DIR}/message/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/proxy/*.cpp")

add_library(mti_out_of_band STATIC ${SRC})
target_include_directories(mti_out_of_band PUBLIC ${SRC_DIR}/framework/security)
target_link_libraries(mti_out_of_band PRIVATE boundscheck ubse_utils ubse_security)
