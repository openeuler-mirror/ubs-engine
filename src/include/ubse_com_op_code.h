/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_COM_OP_CODE_H
#define UBSE_COM_OP_CODE_H

/*
 * 本头文件中定义了UBSE通信系统的完整操作码体系
 * 通信模块标识由模块与操作(Module, Op)决定，使用以下规则：
 * 1. 新增模块功能时
 *    1.1 首先在UbseModuleCode中定义新的模块标识
 *    1.2 然后创建对应的XXXOpCode枚举，为模块定义操作码
 * 2. 扩展现有模块功能时
 *    2.1 查找已有的对应模块枚举
 *    2.2 在对应模块的OpCode枚举中添加新操作码
 * 模块与操作码采用分层设计，模块码标识功能域，操作码标识具体操作
 */

namespace ubse::com {
// 模块码
enum class UbseModuleCode {
    UBSE_MEM = 0x0001,             // UBSE内存基础模块

    ELECTION = 0x0002,             // 选举模块

    STORAGE = 0x0003,              // 存储模块

    NODE_CONTROLLER = 0x0004,      // 节点控制器模块

    UBSE_MEM_QUERY = 0x0005,       // UBSE内存查询模块

    UBSE_MEM_BORROW = 0x0006,      // UBSE内存借用回调模块

    UBSE_MEM_FAULT = 0x0007,       // 内存故障模块

    UBSE_MEM_RESP = 0x0008,        // UBSE内存响应控制模块

    RAS = 0x0009,                  // RAS模块

    VM = 0x000A,                   // 虚拟机模块

    UBSE_URMA = 0x000B,            // URMA Controller模块

    NODE_MGR = 0x000C,       // 节点发现模块

    UBSE_SSU = 0x000D,             // SSU控制器模块
};

// 内存基础操作码
enum class UbseMemOpCode {
    // FD操作
    UBSE_MEM_FD_CREATE = 0x0001,                   // FD内存创建
    UBSE_MEM_FD_WITH_LEND_INFO = 0x0002,           // 带出借方信息的FD创建
    UBSE_MEM_FD_CREATE_WITH_CANDIDATE = 0x0003,    // 带候选节点的FD创建
    UBSE_MEM_FD_PERMISSION = 0x0004,               // FD权限操作
    UBSE_MEM_FD_GET = 0x0005,                      // FD信息获取
    UBSE_MEM_FD_LIST = 0x0006,                     // FD列表查询
    UBSE_MEM_FD_DELETE = 0x0007,                   // FD删除/归还
    UBSE_MEM_FD_GET_MEM_ID_BY_IMPORT = 0x0008,     // 根据导入信息查询导出的memid信息

    // NUMA操作
    UBSE_MEM_NUMA_CREATE = 0x0010,                 // NUMA内存创建
    UBSE_MEM_NUMA_WITH_LEND_INFO = 0x0011,         // 带出借方信息的NUMA创建
    UBSE_MEM_NUMA_CREATE_WITH_CANDIDATE = 0x0012,  // 带候选节点的NUMA创建
    UBSE_MEM_NUMA_GET = 0x0013,                    // NUMA信息获取
    UBSE_MEM_NUMA_LIST = 0x0014,                   // NUMA列表查询
    UBSE_MEM_NUMA_DELETE = 0x0015,                 // NUMA删除/归还
    UBSE_MEM_NUMA_GET_MEM_ID_BY_IMPORT = 0x0016,   // 根据导入信息查询导出的memid信息

    // 共享内存(SHM)操作
    UBSE_MEM_SHM_CREATE = 0x0020,                  // 共享内存创建
    UBSE_MEM_SHM_ATTACH = 0x0021,                  // 共享内存附加
    UBSE_MEM_SHM_GET = 0x0022,                     // 共享内存信息获取
    UBSE_MEM_SHM_LIST = 0x0023,                    // 共享内存列表查询
    UBSE_MEM_SHM_DETACH = 0x0024,                  // 共享内存分离
    UBSE_MEM_SHM_DELETE = 0x0025,                  // 共享内存删除
    UBSE_MEM_SHM_MEMID_STATUS_GET = 0x0026,        // 共享内存状态获取
    UBSE_MEM_SHM_CREATE_WITH_AFFINITY = 0x0027,    // 带亲和的共享内存创建
    UBSE_MEM_SHM_LIST_WITH_PREFIX = 0x0028,        // 带前缀的共享内存列表查询
    UBSE_MEM_SHM_CREATE_WITH_LENDER = 0x0029,      // 带指定借出方的共享内存创建
    UBSE_MEM_SHM_GET_MEM_ID_BY_IMPORT = 0x002A,    // 根据导入信息查询导出的memid信息

    // 命令行(CLI)操作
    UBSE_MEM_CLI_NODE_BORROW = 0x0030,             // 节点借用信息查询(CLI)
    UBSE_MEM_CLI_NODE_LEND = 0x0031,               // 节点出借信息查询(CLI)
    UBSE_MEM_CLI_BORROW_DETAIL_DEBT_PARTIAL_FETCH = 0x0032,  // 借用详情部分获取(CLI)
    UBSE_MEM_CLI_CHECK_STATUS = 0x0033,            // 状态检查(CLI)
    UBSE_MEM_CLI_CONFIG = 0x0034,                  // 配置操作(CLI)
    UBSE_MEM_CLI_NUMA_STATUS = 0x0035,             // NUMA状态查询(CLI)
    UBSE_MEM_CLI_SPECIFY_LINK_BORROW = 0x0036,     // 指定链路借用(CLI)
    UBSE_MEM_CLI_DELETE_MEMORY = 0x0037,           // 内存删除(CLI)
    UBSE_MEM_CLI_NUMA_STATE_QUERY = 0x0038,        // NUMA状态查询(CLI)
    UBSE_MEM_CLI_CLOSE_BORROW_DETAIL_QUERY = 0x0039, // close组网账本查询(CLI)
    UBSE_MEM_CLI_IS_CLOS_TYPE = 0x003A,             // 判断是否为clos组网(CLI)
};

// 选举模块操作码
enum class UbseElectionOpCode {
    ELECTION_PKT = 0x0001,                         // 选举包
    ELECTION_INTER_GROUP_INFO = 0x0002,            // 组间信息
};

// 存储模块操作码
enum class UbseStorageOpCode {
    STORAGE_REQ = 0x0001,                     // 存储请求
};

// 节点控制器操作码
enum class UbseNodeControllerOpCode {
    NODE_CONTROLLER_COLLECT = 0x0001,         // 节点控制器收集
    NODE_CONTROLLER_ALL_NODE = 0x0002,        // 所有节点
    NODE_CONTROLLER_LCNE_CHANGE_REPORT_TOPOLOGY = 0x0003, // LCNE变更报告拓扑
    NODE_CONTROLLER_GET_DEV_CONNECT = 0x0004, // 获取设备连接
    NODE_CONTROLLER_REPORT = 0x0005,          // 节点控制器报告
    NODE_CONTROLLER_NODE_CHANGE = 0x0006,         // 节点变更(节点上线、节点链路变化)
    NODE_CONTROLLER_CABINET_FULL_REPORT = 0x0007, // 柜级全量上报
    NODE_CONTROLLER_GLOBAL_FULL_REPORT = 0x0008,  // 全局全量上报
    NODE_CONTROLLER_SINGLE_NODE_REPORT = 0x0009,  // 单节点上报
};

// UBSE内存响应控制操作码
enum class UbseMemRespCtrlOpCode {
    UBSE_MEM_SHARE_BORROW_RESP = 0x0001,      // 共享内存借用响应
    UBSE_MEM_SHARE_ATTACH_RESP = 0x0002,      // 共享内存附加响应
    UBSE_MEM_SHARE_DETACH_RESP = 0x0003,      // 共享内存分离响应
    UBSE_MEM_COMMON_RETURN_RESP = 0x0004,     // 通用归还响应
    UBSE_MEM_FD_BORROW_RESP = 0x0005,         // FD借用响应
    UBSE_MEM_NUMA_BORROW_RESP = 0x0006,       // NUMA借用响应
    UBSE_MEM_FD_RETURN_RESP = 0x0007,         // FD归还响应
    UBSE_MEM_NUMA_RETURN_RESP = 0x0008,       // NUMA归还响应
    UBSE_MEM_FD_RETURN = 0x0009,              // FD归还
    UBSE_MEM_NUMA_RETURN = 0x000A,            // NUMA归还
    UBSE_MEM_SHARE_RETURN = 0x000B,           // 共享内存归还
    UBSE_MEM_ADDR_RETURN = 0x000C,            // 地址归还
    UBSE_MEM_LEDGER = 0x000D,                 // 内存账本
    UBSE_MEM_FD_IMPORT = 0x000E,              // FD导入
    UBSE_MEM_NUMA_IMPORT = 0x000F,            // NUMA导入
    UBSE_MEM_ADDR_IMPORT = 0x0010,            // 地址导入
    UBSE_MEM_PRE_ONLINE_REQ = 0x0011,         // 预上线请求
    UBSE_MEM_PRE_ONLINE_RESP = 0x0012,        // 预上线响应
    UBSE_MEM_BORROW_RESULT_NOTIFY = 0x0013,   // 借用结果通知
    UBSE_MEM_FD_PERMISSION = 0x0014,          // FD权限
    UBSE_MEM_FD_BORROW_IMPORT_OBJ_FOR_PERMISSION_CALLBACK = 0x0015, // FD借用导入对象权限回调
    UBSE_MEM_GET_OPT_RES = 0x0016,            // 获取操作结果
    UBSE_MEM_INVALIDATE_SINGLE_IMPORT_DEBT = 0x0017, // 无效单个导入账本
    UBSE_MEM_GLOBAL_LEDGER_SUMMARY_REPORT = 0x0018, // 全局账本摘要上报
};

// UBSE内存查询操作码
enum class UbseMemQueryOpCode {
    UBSE_MEM_DEBINFO_QUERY = 0x0001,          // 内存账本信息查询
    UBSE_MEM_DEBT_INFO_FD_GET = 0x0002,       // FD账本信息获取
    UBSE_MEM_DEBT_INFO_FD_LIST = 0x0003,      // FD账本信息列表
    UBSE_MEM_DEBT_INFO_NUMA_GET = 0x0004,     // NUMA账本信息获取
    UBSE_MEM_DEBT_INFO_NUMA_LIST = 0x0005,    // NUMA账本信息列表
    UBSE_MEM_DEBT_INFO_NUMA_GET_WITH_IMPORT_NODE = 0x0006, // 带导入节点的NUMA账本信息获取
    UBSE_MEM_DEBT_INFO_SHM_GET = 0x0007,      // 共享内存账本信息获取
    UBSE_MEM_DEBT_INFO_SHM_LIST = 0x0008,     // 共享内存账本信息列表
    UBSE_MEM_DEBT_INFO_SHM_STATUS_GET = 0x0009, // 共享内存状态获取
    UBSE_MEM_DEBT_INFO_ADDR_GET = 0x000A,     // 地址账本信息获取
    UBSE_MEM_DEBT_INFO_PARTIAL_FETCH = 0x000B, // 部分获取账本信息
    UBSE_MEM_NODE_BORROW_INFO_QUERY = 0x000C, // 节点借用信息查询
    UBSE_MEM_GET_NUMAINFO_BY_PID = 0x000D,    // 通过PID获取NUMA信息
    UBSE_MEM_QUERY_FD_EXPORT = 0x000E,        // 查询FD导出
    UBSE_MEM_QUERY_FD_IMPORT = 0x000F,        // 查询FD导入
    UBSE_MEM_QUERY_NUMA_EXPORT = 0x0010,      // 查询NUMA导出
    UBSE_MEM_QUERY_NUMA_IMPORT = 0x0011,      // 查询NUMA导入
    UBSE_MEM_QUERY_ADDR_EXPORT = 0x0012,      // 查询地址导出
    UBSE_MEM_QUERY_ADDR_IMPORT = 0x0013,      // 查询地址导入
    UBSE_MEM_QUERY_SHARE_EXPORT = 0x0014,     // 查询共享内存导出
    UBSE_MEM_QUERY_SHARE_IMPORT = 0x0015,     // 查询共享内存导入
    UBSE_MEM_ID_DEBINFO_QUERY = 0x00016,      // 内存账本信息查询
    UBSE_MEM_REMOTE_NUMA_STATUS = 0x0017,     // 远端numa状态
    UBSE_MEM_QUERY_CLOSE_SHARE_EXPORT = 0x0018,     // close组网查询共享内存导出对象
    UBSE_MEM_QUERY_CLOSE_SHARE_IMPORT = 0x0019,     // close组网查询共享内存导入对象
    UBSE_MEM_DEBT_INFO_PARTIAL_FETCH_CLOSE = 0x001A, // close组网部分获取账本信息
    UBSE_MEM_CLOSE_BORROW_DETAIL_QUERY = 0x001B,       // close组网账本批量查询
};

// UBSE内存借用回调操作码
enum class UbseMemBorrowCallbackOpCode {
    // UBSE内存借用操作
    UBSE_MEM_FD_BORROW = 0x0001,              // FD内存借用
    UBSE_MEM_NUMA_BORROW = 0x0002,            // NUMA内存借用
    UBSE_MEM_ADDR_BORROW = 0x0003,            // 地址内存借用
    UBSE_MEM_SHARE_BORROW = 0x0004,           // 共享内存借用
    UBSE_MEM_SHARE_ATTACH = 0x0005,           // 共享内存附加
    UBSE_MEM_SHARE_DETACH = 0x0006,           // 共享内存分离

    // UBSE内存回调操作
    UBSE_MEM_FD_BORROW_EXPORT_OBJ_CALLBACK = 0x0007,   // FD借用导出对象回调
    UBSE_MEM_FD_BORROW_IMPORT_OBJ_CALLBACK = 0x0008,   // FD借用导入对象回调
    UBSE_MEM_NUMA_BORROW_EXPORT_OBJ_CALLBACK = 0x0009, // NUMA借用导出对象回调
    UBSE_MEM_NUMA_BORROW_IMPORT_OBJ_CALLBACK = 0x000A, // NUMA借用导入对象回调
    UBSE_MEM_SHARE_BORROW_EXPORT_OBJ_CALLBACK = 0x000B, // 共享内存借用导出对象回调
    UBSE_MEM_SHARE_BORROW_IMPORT_OBJ_CALLBACK = 0x000C, // 共享内存借用导入对象回调
    UBSE_MEM_ADDR_BORROW_EXPORT_OBJ_CALLBACK = 0x000D, // 地址借用导出对象回调
    UBSE_MEM_ADDR_BORROW_IMPORT_OBJ_CALLBACK = 0x000E, // 地址借用导入对象回调
    UBSE_MEM_UPDATE_OBJ_STATE_CALLBACK = 0x000F,        // 主节点更新从节点对象状态回调

    // close组网回调操作
    UBSE_MEM_SHARE_BORROW_GLOBAL_MASTER_TO_EXPORT_CASCADE_MASTER = 0x0010, // close组网: 全局主→导出Cascade Master下发导出
    UBSE_MEM_SHARE_BORROW_EXPORT_CASCADE_MASTER_TO_GLOBAL_MASTER = 0x0011, // close组网: 导出Cascade Master→全局主回调导出结果
    UBSE_MEM_SHARE_BORROW_EXPORT_AGENT_TO_CASCADE_MASTER = 0x0012,         // close组网: 导出Agent→导出Cascade Master回调导出结果
    UBSE_MEM_SHARE_ATTACH_GLOBAL_MASTER_TO_IMPORT_CASCADE_MASTER = 0x0013, // close组网: 全局主→导入Cascade Master下发导入
    UBSE_MEM_SHARE_ATTACH_IMPORT_CASCADE_MASTER_TO_GLOBAL_MASTER = 0x0014, // close组网: 导入Cascade Master→全局主回调导入结果
    UBSE_MEM_SHARE_ATTACH_IMPORT_AGENT_TO_CASCADE_MASTER = 0x0015,         // close组网: 导入Agent→导入Cascade Master回调导入结果
    UBSE_MEM_SHARE_DETACH_CASCADE_MASTER_TO_PD_MASTER = 0x0016,            // close组网: Cascade Master→PD主转发Detach
    UBSE_MEM_SHARE_DETACH_PD_MASTER_TO_GLOBAL_MASTER = 0x0017,           // close组网: PD主→全局主转发Detach
    UBSE_MEM_SHARE_DETACH_CASCADE_MASTER_HANDLE = 0x0018,                  // close组网: 全局主→导入Cascade Master下发Detach
    UBSE_MEM_SHARE_DELETE_CASCADE_MASTER_HANDLE = 0x0019,                   // close组网: 全局主→导出Cascade Master下发Delete
    UBSE_MEM_SHARE_DELETE_CASCADE_MASTER_TO_GLOBAL_MASTER = 0x0020,         // close组网: 导出Cascade Master→全局主回调Delete结果
    UBSE_MEM_SHARE_DETACH_IMPORT_CASCADE_MASTER_TO_GLOBAL_MASTER = 0x0021,  // close组网: 导入Cascade Master→全局主回调Detach结果
    UBSE_MEM_SHARE_BORROW_FORWARD_CASCADE_MASTER_TO_GLOBAL_MASTER = 0x0022, // close组网: Cascade Master→全局主 转发借用请求
    UBSE_MEM_SHARE_ATTACH_FORWARD_CASCADE_MASTER_TO_GLOBAL_MASTER = 0x0023,  // close组网: Cascade Master→全局主 转发挂载请求
    UBSE_MEM_SHARE_RETURN_FORWARD_CASCADE_MASTER_TO_GLOBAL_MASTER = 0x0024,  // close组网: Cascade Master→全局主 转发归还请求
};

// UBSE内存故障处理操作码
enum class UbseMemFaultOpCode {
    UBSE_SHARE_MEM_FAULT_REPORT = 0x0001,           // 共享内存故障报告
    UBSE_SHARE_MEM_FAULT_REPORT_REPLY = 0x0002,     // 共享内存故障报告回复
    UBSE_SHARE_MEM_FAULT_NOTIFY = 0x0003,           // 共享内存故障通知
    UBSE_SHARE_MEM_FAULT_NOTIFY_REPLY = 0x0004,     // 共享内存故障通知回复
};

// RAS模块操作码
enum class UbseRasOpCode {
    UBSE_RAS_BMC_REBOOT = 0x0001,             // BMC重启
    UBSE_RAS_SWITCH_ROLE = 0x0002,            // 切换角色
    UBSE_RAS_OOM = 0x0003,                    // OOM处理
};

// Urma controller模块操作码
enum class UbseUrmaRpcOpCode {
    URMA_RPC_URMA_INFO_REPORT = 0x0001,
    URMA_RPC_URMA_INFO_BROADCAST = 0x0002,
    URMA_RPC_URMA_INFO_QUERY = 0x0003,
    URMA_RPC_DEV_QUERY = 0x0004,
    URMA_RPC_DEV_ACTIVATE = 0x0005,
    BUTT = 0x0006,
};

enum class UbseNodeMgrOpCode {
    NODE_INFO_REPORT = 0x0001,     // 节点发现, 普通节点向根节点上报本节点信息
    NODE_INFO_BROADCAST = 0x0002, // 节点发现,根节点向普通节点下发/其余根节点同步全量节点信息
};

// SSU控制器模块操作码
enum class UbseSsuOpCode {
    UBSE_SSU_ALLOC_REQ = 0x0001,              // SSU空间分配请求
    UBSE_SSU_ALLOC_RESP = 0x0002,             // SSU空间分配响应
    UBSE_SSU_STATUS_UPDATE = 0x0003,          // SSU状态更新
    UBSE_SSU_FREE_REQ = 0x0004,               // SSU空间释放请求
    UBSE_SSU_FREE_RESP = 0x0005,              // SSU空间释放响应
    UBSE_SSU_LIST_ALLOC_INFO_REQ = 0x0006,          // 查询分配信息列表请求
    UBSE_SSU_GET_ALLOC_INFO_BY_NAME_REQ = 0x0007,   // 根据名称查询分配信息请求
    UBSE_SSU_GET_NS_STATS_REQ = 0x0008,             // 查询命名空间统计信息请求
    UBSE_SSU_GET_CONNECT_INFO_REQ = 0x0009,         // 查询连接信息请求
    UBSE_SSU_ADD_ACCESS_PERMISSION_REQ = 0x000A,    // 添加访问权限请求
    UBSE_SSU_REMOVE_ACCESS_PERMISSION_REQ = 0x000B, // 移除访问权限请求
    UBSE_SSU_ATTACH_SPACE_REQ = 0x000C,             // 挂载存储空间请求
    UBSE_SSU_DETACH_SPACE_REQ = 0x000D,             // 卸载存储空间请求
    UBSE_SSU_ATTACH_LINEAR_SPACE_REQ = 0x000E,      // 挂载线性编址空间请求
    UBSE_SSU_DETACH_LINEAR_SPACE_REQ = 0x000F,      // 卸载线性编址空间请求
    UBSE_SSU_ATTACH_STRIPED_SPACE_REQ = 0x0010,     // 挂载条带化空间请求
    UBSE_SSU_DETACH_STRIPED_SPACE_REQ = 0x0011,     // 卸载条带化空间请求
    UBSE_SSU_GET_FE_DEVICE_LIST_REQ = 0x0012,       // 查询FE设备列表请求
    UBSE_SSU_FE_DEVICE_ALLOC_REQ = 0x0013,          // 分配VFE设备请求
    UBSE_SSU_FE_DEVICE_FREE_REQ = 0x0014,           // 释放VFE设备请求
    UBSE_SSU_ADD_ACCESS_PERMISSION_RESP = 0x0015,   // 添加访问权限响应
    UBSE_SSU_REMOVE_ACCESS_PERMISSION_RESP = 0x0016, // 移除访问权限响应
    UBSE_SSU_ATTACH_DETACH_VERIFY_REQ = 0x0017,     // attach/detach前identity验证请求
};
} // namespace ubse::com
#endif // UBSE_COM_OP_CODE_H