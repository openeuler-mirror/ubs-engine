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

#ifndef UBSE_IPC_COMMON_H
#define UBSE_IPC_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    UBSE_MEM = 0x0001,
    UBSE_ELECTION = 0x0002,
    UBSE_NODE = 0x0003,
    UBSE_LONG_LINK_REGISTER = 0x0004, // moduleCode 为向服务端注册监听长连接事件
    UBSE_URMA = 0x0005,
} ubse_ipc_module_code_t;

typedef enum
{
    UBSE_LONGLINK_FAULT_SHM = 0x0001,
    UBSE_LONGLINK_FAULT_FD = 0x0002,
    UBSE_LONGLINK_FAULT_NUMA = 0x0003,
} ubse_ipc_long_link_op_code_t;

typedef enum
{
    UBSE_MEM_FD_CREATE = 0x0001,
    UBSE_MEM_FD_WITH_LEND_INFO = 0x0002,
    UBSE_MEM_FD_CREATE_WITH_CANDIDATE = 0x0003,
    UBSE_MEM_FD_PERMISSION = 0x0004,
    UBSE_MEM_FD_GET = 0x0005,
    UBSE_MEM_FD_LIST = 0x0006,
    UBSE_MEM_FD_DELETE = 0x0007,
    UBSE_MEM_FD_GET_MEM_ID_BY_IMPORT = 0x0008,

    UBSE_MEM_NUMA_CREATE = 0x0010,
    UBSE_MEM_NUMA_WITH_LEND_INFO = 0x0011,
    UBSE_MEM_NUMA_CREATE_WITH_CANDIDATE = 0x0012,
    UBSE_MEM_NUMA_GET = 0x0013,
    UBSE_MEM_NUMA_LIST = 0x0014,
    UBSE_MEM_NUMA_DELETE = 0x0015,
    UBSE_MEM_NUMA_GET_MEM_ID_BY_IMPORT = 0x0016,

    UBSE_MEM_SHM_CREATE = 0x0020,
    UBSE_MEM_SHM_ATTACH = 0x0021,
    UBSE_MEM_SHM_GET = 0x0022,
    UBSE_MEM_SHM_LIST = 0x0023,
    UBSE_MEM_SHM_DETACH = 0x0024,
    UBSE_MEM_SHM_DELETE = 0x0025,
    UBSE_MEM_SHM_MEMID_STATUS_GET = 0x0026,
    UBSE_MEM_SHM_CREATE_WITH_AFFINITY = 0x0027,
    UBSE_MEM_SHM_LIST_WITH_PREFIX = 0x0028,
    UBSE_MEM_SHM_CREATE_WITH_LENDER = 0x0029,
    UBSE_MEM_SHM_GET_MEM_ID_BY_IMPORT = 0x002A,

    UBSE_MEM_CLI_NODE_BORROW = 0x0030,
    UBSE_MEM_CLI_NODE_LEND = 0x0031,
    UBSE_MEM_CLI_BORROW_DETAIL_DEBT_PARTIAL_FETCH = 0x0032,
    UBSE_MEM_CLI_CHECK_STATUS = 0x0033,
    UBSE_MEM_CLI_CONFIG = 0x0034,
    UBSE_MEM_CLI_NUMA_STATUS = 0x0035,

    // todo 改名，删除接口简单，提供了type参数，可以合一，仅整改客户端代码
    UBSE_MEM_CLI_DELETE_MEMORY = 0x0037,
    // todo 删除,创建和查询过程不同，各类型查询结构体和接口不同，需拆分
    UBSE_MEM_CLI_NUMA_STATE_QUERY = 0x0038,

    UBSE_MEM_CLI_NUMA_CREATE = 0x0039,                // NUMA创建(CLI)
    UBSE_MEM_CLI_NUMA_INFO_GET_BY_NAME = 0x0040,      // NUMA借用关系查询(CLI)
    UBSE_MEM_CLI_FD_CREATE = 0x0041,                  // FD创建(CLI)
    UBSE_MEM_CLI_FD_INFO_GET_BY_NAME = 0x0042,        // FD借用关系查询(CLI)
    UBSE_MEM_CLI_SHM_CREATE = 0x0043,                 // SHM创建(CLI)
    UBSE_MEM_CLI_SHM_INFO_GET_BY_NAME = 0x0044,       // SHM借用关系查询(CLI)
    UBSE_MEM_CLI_SHM_ATTACH = 0x0045,                 // SHM链接(CLI)
    UBSE_MEM_CLI_SHM_DETACH = 0x0046,                 // SHM释放(CLI)
    UBSE_MEM_CLI_MEMORY_DELETE_BY_NAME_TYPE = 0x0047, // 内存删除(聚合接口)(CLI)
    UBSE_MEM_CLI_PID_SET_THRESHOLD = 0x0048,          // PID阈值设置(CLI)
    UBSE_MEM_CLI_PRINT_PID_INFO = 0x0049,             // PID信息查询(CLI)
    UBSE_MEM_CLI_PID_UNSET = 0x0050,                  // PID配置删除(CLI)
} ubse_ipc_mem_op_code_t;

typedef enum
{
    UBSE_NODE_LIST = 0x0001,
    UBSE_NODE_CPU_TOPO_LIST = 0x0002,
    UBSE_NODE_NUMA_MEM_GET = 0x0003,
    UBSE_NODE_GET = 0x0004,
    UBSE_NODE_CLI_NODE_INFO = 0x0005,
    UBSE_NODE_CLI_CPU_TOPO_LIST = 0x0006,
    UBSE_CLUSTER_INFO = 0x0007,
} ubse_ipc_node_op_code_t;

typedef enum
{
    UBSE_ELECTION_QUERY_NODE_ID = 0x0001,
} ubse_ipc_election_op_code_t;

typedef enum
{
    UBSE_URMA_QOS_SET = 0x0001,
    UBSE_URMA_QOS_GET = 0x0002,
    UBSE_URMA_QOS_RESET = 0x0003,
    UBSE_URMA_CLI_QOS_GET = 0x0004,
    UBSE_URMA_DEV_GET = 0x0005,
    UBSE_URMA_DEV_ALLOC = 0x0006,
    UBSE_URMA_DEV_FREE = 0x0007,
    UBSE_URMA_CLI_DEV_GET = 0x0008,
    UBSE_URMA_BUTT
} ubse_ipc_urma_op_code_t;
#ifdef __cplusplus
}
#endif
#endif // UBSE_IPC_COMMON_H
