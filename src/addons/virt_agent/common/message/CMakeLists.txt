file(GLOB_RECURSE VIRT_AGENT_SDK_MESSAGE_CPP "*.cpp")          # 收集所有 .cpp 文件

FIND_INCLUDE_DIR(VIRT_AGENT_SDK_MESSAGE_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${VIRT_AGENT_SDK_MESSAGE_INCLUDE}
        ${PROJECT_SOURCE_DIR}/src/addons/virt_agent/common
        ${PROJECT_SOURCE_DIR}/src/addons/virt_agent/common/mempooling
        ${PROJECT_SOURCE_DIR}/src/addons/virt_agent/common/util
        ${PROJECT_SOURCE_DIR}/src/addons/virt_agent/common/libvirt
        ${PROJECT_SOURCE_DIR}/src/addons/virt_agent/include
        ${PROJECT_SOURCE_DIR}/src/framework/serde
        ${PROJECT_SOURCE_DIR}/include)
add_library(virt_agent_sdk_message STATIC
        ${VIRT_AGENT_SDK_MESSAGE_CPP}
        ${PROJECT_SOURCE_DIR}/src/addons/virt_agent/common/libvirt/libvirt_module.cpp
        )
target_link_options(virt_agent_sdk_message PRIVATE -Wl,-z,relro,-z,now)
target_link_libraries(virt_agent_sdk_message PUBLIC rapidjson boundscheck ubse_log ubse_config ubse_serial)