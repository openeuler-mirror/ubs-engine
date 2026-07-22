add_subdirectory(out_of_band)

# 首先，定义一个变量来存储源文件
set(SOURCES "")

# 检查ENABLE_UB变量
if(ENABLE_UB)
    # 如果ENABLE_UB为true，包含所有源文件
    file(GLOB SOURCES "*.cpp" "*.h")
    file(GLOB SOURCESLCNE "${CMAKE_CURRENT_SOURCE_DIR}/lcne/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/lcne/*.h}")
    list(APPEND SOURCES ${SOURCESLCNE})
else()
    # 如果ENABLE_UB为false，只包含特定的源文件
    set(SOURCES ubse_mti_interface.cpp ubse_topology_interface.cpp)
endif()

# 创建库并添加源文件
add_library(ubse_mti STATIC ${SOURCES})
target_include_directories(ubse_mti PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../controllers/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/adapter_plugins/mti)

target_link_libraries(ubse_mti PRIVATE mti_out_of_band ubse_election ubse_xml ubse_utils ubse_com ubse_context ubse_log ubse_http ubse_config ubse_smbios)