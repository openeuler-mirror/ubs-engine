add_subdirectory(ipc)
file(GLOB UBSE_SSU_CONTROLLER_SOURCES "*.cpp" "*.h")
file(GLOB UBSE_SSU_CONTROLLER_MESSAGE_SOURCES "message/*.cpp" "message/*.h")
file(GLOB UBSE_SSU_CONTROLLER_DEBT_SOURCES "debt/*.cpp" "debt/*.h")
set(UBSE_SSU_CONTROLLER_ALL_SOURCES ${UBSE_SSU_CONTROLLER_SOURCES} ${UBSE_SSU_CONTROLLER_MESSAGE_SOURCES} ${UBSE_SSU_CONTROLLER_DEBT_SOURCES})

# 构建 SSU 插件共享库（.so）
add_library(ssu_plugin SHARED ${UBSE_SSU_CONTROLLER_ALL_SOURCES})

set(SSU_PLUGIN_DEP_TARGETS
    ubse_log ubse_context ubse_com ubse_serial ubse_election
    ubse_timer ubse_utils ubse_vip ubse_ssu_ipc ubse_mti ubse_ssu)

# 获取其它模块的头文件路径（遍历所有依赖，不限于链接依赖）
# 注意：部分依赖（如 ubse_mti）目标尚未创建（controllers 在 adapter_plugins 之前加载），
# 需要手动补充其 PUBLIC include 路径
set(SSU_PLUGIN_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/adapter_plugins/ssu
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/framework/filter_chain
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/plugin_services/ssu
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/adapter_plugins/mti
    ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/http)
foreach(dep IN LISTS SSU_PLUGIN_DEP_TARGETS)
    if(TARGET ${dep})
        get_target_property(inc_dirs ${dep} INTERFACE_INCLUDE_DIRECTORIES)
        if(inc_dirs)
            list(APPEND SSU_PLUGIN_INCLUDES ${inc_dirs})
        endif()
    endif()
endforeach()

target_include_directories(ssu_plugin PUBLIC ${SSU_PLUGIN_INCLUDES})

# ubse_ssu 由主程序通过 --whole-archive 提供，插件运行时解析
# target_link_libraries(ssu_plugin PRIVATE ubse_ssu)

target_link_options(ssu_plugin PRIVATE -Wl,--allow-shlib-undefined)

set_target_properties(ssu_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
