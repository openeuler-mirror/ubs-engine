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

/**
 * @brief IPC error code enumeration
 */
typedef enum {
    IPC_SUCCESS = 0,                  // Indicates successful operation with no errors
    IPC_ERROR_INVALID_ARGUMENT,       // Indicates invalid or incorrect input arguments
    IPC_ERROR_SERIALIZATION_FAILED,   // Indicates failure during serialization
    IPC_ERROR_DESERIALIZATION_FAILED, // Indicates failure during deserialization
    IPC_SOCKET_LISTEN_FAILED,         // Indicates failure to listen on the socket
    IPC_ERROR_CONNECTION_FAILED,      // Indicates failure to establish a connection
    IPC_ERROR_SEND_FAILED,            // Indicates failure to send data
    IPC_ERROR_RECV_FAILED,            // Indicates failure to receive data
    IPC_ERROR_TIMEOUT,                // Indicates operation timed out
    IPC_ERROR_INVALID_HANDLE,         // Indicates an invalid handle
    IPC_ERROR_DAEMON_NOT_READY,       // Indicates the daemon is not ready to handle the request
    IPC_ERROR_INVALID_PATH_LENGTH,    // Indicates failure to establish a connection due to invalid socket path length
} ubse_ipc_error_code_t;

typedef enum {
    UBSE_MEM = 0x0001,
    UBSE_NODE = 0x0002,
    UBSE_URMA = 0x0006
} ubse_ipc_module_code_t;

typedef enum {
    UBSE_MEM_FD_CREATE = 0x0001,
    UBSE_MEM_FD_WITH_LEND_INFO = 0x0002,
    UBSE_MEM_FD_GET = 0x0003,
    UBSE_MEM_FD_LIST = 0x0004,
    UBSE_MEM_FD_DELETE = 0x0005,
    UBSE_MEM_NUMA_CREATE = 0x0006,
    UBSE_MEM_NUMA_WITH_LEND_INFO = 0x0007,
    UBSE_MEM_NUMA_GET = 0x0008,
    UBSE_MEM_NUMA_LIST = 0x0009,
    UBSE_MEM_NUMA_DELETE = 0x0010,
    USBE_MEM_CLI_NODE_BORROW = 0x0011,
    USBE_MEM_CLI_BORROW_DETAIL = 0x0012,
    USBE_MEM_CLI_CHECK_STATUS = 0x0013
} ubse_ipc_mem_op_code_t;

typedef enum {
    UBSE_NODE_LIST = 0x0001,
    UBSE_NODE_CPU_TOPO_LIST = 0x0002,
    UBSE_NODE_NUMA_MEM_GET = 0x0003,
    UBSE_NODE_CLI_TOPO_LIST = 0x0004,
    UBSE_CLUSTER_INFO = 0x0005,
    UBSE_NODE_GET = 0x0006,
    UBSE_NODE_CLI_NODE_INFO = 0x0007
} ubse_ipc_node_op_code_t;

typedef enum {
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
