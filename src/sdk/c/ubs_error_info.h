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

#ifndef UBS_ERROR_INFO_H
#define UBS_ERROR_INFO_H

#include "ubs_error.h"

#ifdef __cplusplus
extern "C" {
#endif

// 错误信息结构体
typedef struct {
    const char* name;
    const char* message;
} ubs_error_info_t;

// 公共错误码描述
static const ubs_error_info_t* const common_error_infos[] = {
    [UBS_SUCCESS] = &(ubs_error_info_t){"UBS_SUCCESS", "Operation completed successfully"},
    // 参数错误 (1-9)
    [UBS_ERR_INVALID_ARG] = &(ubs_error_info_t){"UBS_ERR_INVALID_ARG", "Invalid parameter provided"},
    [UBS_ERR_OUT_OF_RANGE] = &(ubs_error_info_t){"UBS_ERR_OUT_OF_RANGE", "Parameter value out of valid range"},
    [UBS_ERR_NULL_POINTER] = &(ubs_error_info_t){"UBS_ERR_NULL_POINTER", "Unexpected null pointer encountered"},
    [UBS_ERR_BUFFER_TOO_SMALL] = &(ubs_error_info_t){"UBS_ERR_BUFFER_TOO_SMALL", "Insufficient buffer space"},
    // 资源错误 (10-19)
    [UBS_ERR_OUT_OF_MEMORY] =
        &(ubs_error_info_t){.name = "UBS_ERR_OUT_OF_MEMORY", .message = "Memory allocation failure"},
    [UBS_ERR_RESOURCE_BUSY] =
        &(ubs_error_info_t){.name = "UBS_ERR_RESOURCE_BUSY", .message = "Resource is currently in use"},
    [UBS_ERR_RESOURCE_EXHAUSTED] =
        &(ubs_error_info_t){.name = "UBS_ERR_RESOURCE_EXHAUSTED", .message = "Resource limit reached"},
    [UBS_ERR_QUOTA_EXCEEDED] =
        &(ubs_error_info_t){.name = "UBS_ERR_QUOTA_EXCEEDED", .message = "Operation exceeds allocated quota"},

    // IPC通信错误 (20-29)
    [UBS_ERR_IPC_CONNECTION_FAILED] =
        &(ubs_error_info_t){.name = "UBS_ERR_IPC_CONNECTION_FAILED", .message = "Failed to establish IPC connection"},
    [UBS_ERR_IPC_TIMEOUT] = &(ubs_error_info_t){.name = "UBS_ERR_IPC_TIMEOUT", .message = "IPC operation timed out"},
    [UBS_ERR_IPC_SERVICE_UNAVAILABLE] =
        &(ubs_error_info_t){.name = "UBS_ERR_IPC_SERVICE_UNAVAILABLE", .message = "Requested service is unavailable"},
    [UBS_ERR_IPC_CONNECTION_FAILED_PATH_LENGTH] =
        &(ubs_error_info_t){.name = "UBS_ERR_IPC_CONNECTION_FAILED",
                            .message = "Failed to establish IPC connection, socket path is too long"},
    // 权限错误 (30-39)
    [UBS_ERR_PERMISSION_DENIED] =
        &(ubs_error_info_t){.name = "UBS_ERR_PERMISSION_DENIED", .message = "Insufficient permissions"},
    [UBS_ERR_AUTHENTICATION_FAILED] =
        &(ubs_error_info_t){.name = "UBS_ERR_AUTHENTICATION_FAILED", .message = "Authentication credentials invalid"},
    [UBS_ERR_ACCESS_DENIED] =
        &(ubs_error_info_t){.name = "UBS_ERR_ACCESS_DENIED", .message = "Access to resource denied"},

    // 操作错误 (40-49)
    [UBS_ERR_NOT_IMPLEMENTED] =
        &(ubs_error_info_t){.name = "UBS_ERR_NOT_IMPLEMENTED", .message = "Functionality not implemented"},
    [UBS_ERR_NOT_SUPPORTED] =
        &(ubs_error_info_t){.name = "UBS_ERR_NOT_SUPPORTED", .message = "Operation not supported"},
    [UBS_ERR_OPERATION_FAILED] =
        &(ubs_error_info_t){.name = "UBS_ERR_OPERATION_FAILED", .message = "General operation failure"},
    [UBS_ERR_TIMED_OUT] = &(ubs_error_info_t){.name = "UBS_ERR_TIMED_OUT", .message = "Operation timed out"},

    // 守护进程错误 (50-59)
    [UBS_ERR_DAEMON_UNREACHABLE] =
        &(ubs_error_info_t){.name = "UBS_ERR_DAEMON_UNREACHABLE", .message = "Daemon process unreachable"},
    [UBS_ERR_DAEMON_BUSY] =
        &(ubs_error_info_t){.name = "UBS_ERR_DAEMON_BUSY", .message = "Daemon is busy, try again later"},
    [UBS_ERR_DAEMON_CRASHED] =
        &(ubs_error_info_t){.name = "UBS_ERR_DAEMON_CRASHED", .message = "Daemon process terminated unexpectedly"},
    [UBS_ERR_DAEMON_INTERNEL] =
        &(ubs_error_info_t){.name = "UBS_ERR_DAEMON_INTERNEL", .message = "Internal daemon error"}};

#ifdef __cplusplus
}
#endif
#endif // UBS_ERROR_INFO_H
