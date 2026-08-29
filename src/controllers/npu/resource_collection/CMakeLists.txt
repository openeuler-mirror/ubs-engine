file(GLOB SRC *.cpp)
add_library(npu_resource_collection STATIC ${SRC})

target_include_directories(npu_resource_collection PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src/include)

target_include_directories(npu_resource_collection PRIVATE
        $<TARGET_PROPERTY:ubse_utils,INTERFACE_INCLUDE_DIRECTORIES>)