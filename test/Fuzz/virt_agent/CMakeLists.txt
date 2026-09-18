# virt_agent 模块 DT Fuzz harness 集合
# 4 个攻击面：
#   ft_va_msg_deser      - SDK 消息反序列化（BaseMessage 子类 Deserialize）
#   ft_va_case_conf_json - JSON 解析（VMJsonUtil：北向配置/响应 JSON 入口）
#   ft_va_sdk_unpack     - SDK unpack 函数（daemon 响应 -> C 结构体）
#   ft_va_ipc_raw        - IPC 客户端栈（ubse_invoke_call 全链路，伪造恶意 daemon）

set(VA_DIR ${PROJECT_SOURCE_DIR}/src/addons/virt_agent)

# virt_agent 消息/工具头文件的公共搜索路径（不含 sdk/ 下各实现的私有路径）
set(FT_VA_INCLUDE
        ${VA_DIR}/common
        ${VA_DIR}/common/util
        ${VA_DIR}/common/message
        ${VA_DIR}/common/message/include
        ${VA_DIR}/common/message/sdk
        ${VA_DIR}/common/mempooling
        ${VA_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/framework/serde
        ${PROJECT_SOURCE_DIR}/src/framework/ipc/include
        ${PROJECT_SOURCE_DIR}/src/include
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}
        )

# ---------------- harness 1: 消息反序列化 ----------------
add_executable(ft_va_msg_deser ft_va_msg_deser.cpp)
target_include_directories(ft_va_msg_deser PRIVATE ${FT_VA_INCLUDE})
target_link_libraries(ft_va_msg_deser PRIVATE ft_driver virt_agent_sdk_message)

# ---------------- harness 2: JSON 解析 ----------------
# vm_json_util/vm_string_util 属于 daemon 侧 virtagent 插件库，依赖较重（storage/mempooling），
# 故直接以源文件方式编入 harness（FromJson 的实际解析路径即 VMConvertJsonStr2Map + SafeStof）。
add_executable(ft_va_case_conf_json
        ft_va_case_conf_json.cpp
        ${VA_DIR}/common/util/vm_json_util.cpp
        ${VA_DIR}/common/util/vm_string_util.cpp)
target_include_directories(ft_va_case_conf_json PRIVATE ${FT_VA_INCLUDE})
target_link_libraries(ft_va_case_conf_json PRIVATE ft_driver ubse_log)

# ---------------- harness 3: SDK unpack ----------------
# helper 属于 ubs_virt_agent_sdk 共享库，同样以源文件方式编入以便精确定位链接面。
add_executable(ft_va_sdk_unpack
        ft_va_sdk_unpack.cpp
        ${VA_DIR}/sdk/case_conf/ubs_virt_agent_case_conf.cpp
        ${VA_DIR}/sdk/case_conf/ubs_virt_agent_case_conf_helper.cpp
        ${VA_DIR}/sdk/mem_fragmentation/ubs_virt_agent_mem_fragmentation_helper.cpp)
target_include_directories(ft_va_sdk_unpack PRIVATE
        ${FT_VA_INCLUDE}
        ${VA_DIR}/sdk/include
        ${VA_DIR}/sdk/case_conf
        ${VA_DIR}/sdk/mem_fragmentation)
target_link_libraries(ft_va_sdk_unpack PRIVATE ft_driver virt_agent_sdk_message ubse_ipc_client)

# ---------------- harness 4: IPC 全链路 ----------------
add_executable(ft_va_ipc_raw ft_va_ipc_raw.cpp)
target_include_directories(ft_va_ipc_raw PRIVATE
        ${PROJECT_SOURCE_DIR}/src/framework/ipc
        ${PROJECT_SOURCE_DIR}/src/framework/ipc/include
        ${PROJECT_SOURCE_DIR}/src/include
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR})
target_link_libraries(ft_va_ipc_raw PRIVATE ft_driver ubse_ipc_client UbseLog)
