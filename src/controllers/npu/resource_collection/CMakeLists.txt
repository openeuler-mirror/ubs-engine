file(GLOB SRC *.cpp)
add_library(npu_resource_collection STATIC ${SRC})
target_link_libraries(npu_resource_collection PRIVATE ubse_log ubse_utils mti_ctrl_q ubse_mti)
target_include_directories(npu_resource_collection PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})