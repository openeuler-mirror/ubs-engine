file(GLOB SRC *.cpp "${CMAKE_CURRENT_SOURCE_DIR}/protocol/*.cpp")

add_library(mti_ctrl_q STATIC ${SRC})
target_link_libraries(mti_ctrl_q PRIVATE boundscheck ubse_utils)