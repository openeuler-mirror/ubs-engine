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

#ifndef UBSE_ERROR_INFO_H
#define UBSE_ERROR_INFO_H

#include "ubs_error.h"
#include "ubs_error_info.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UBSE_ERR_CODE 1000

// UBSE错误码信息数组
static const ubs_error_info_t* const ubse_error_infos[] = {
    [UBS_ENGINE_ERR_OUT_OF_RANGE - UBSE_ERR_CODE] =
        &(ubs_error_info_t){"UBSE_ERR_OUT_OF_RANGE", "Parameter out of range"},
    [UBS_ENGINE_ERR_RESOURCE - UBSE_ERR_CODE] = &(ubs_error_info_t){"UBSE_ERR_RESOURCE", "Resource allocation error"},
    [UBS_ENGINE_ERR_CONNECTION_FAILED - UBSE_ERR_CODE] =
        &(ubs_error_info_t){"UBSE_ERR_CONNECTION_FAILED", "Connection to UBSE server failed"},
    [UBS_ENGINE_ERR_AUTH_FAILED - UBSE_ERR_CODE] =
        &(ubs_error_info_t){"UBSE_ERR_AUTH_FAILED", "UBSE server authentication failed"},
    [UBS_ENGINE_ERR_TIMEOUT - UBSE_ERR_CODE] =
        &(ubs_error_info_t){"UBSE_ERR_TIMEOUT", "UBSE server processing timeout"},
    [UBS_ENGINE_ERR_INTERNAL - UBSE_ERR_CODE] = &(ubs_error_info_t){"UBSE_ERR_INTERNAL", "UBSE server internal error"},
    [UBS_ENGINE_ERR_EXISTED - UBSE_ERR_CODE] =
        &(ubs_error_info_t){"UBSE_ERR_EXISTED", "Borrow relationship already exists"},
    [UBS_ENGINE_ERR_NOT_EXIST - UBSE_ERR_CODE] =
        &(ubs_error_info_t){"UBSE_ERR_NOT_EXIST", "Borrow relationship does not exist"},
    [UBS_ENGINE_ERR_UDSINFO_MISMATCH - UBSE_ERR_CODE] =
        &(ubs_error_info_t){"UBSE_ERR_UDSINFO_MISMATCH", "UDS INFO mismatch"},
    [UBS_ENGINE_ERR_IMPORT_ABSENT - UBSE_ERR_CODE] =
        &(ubs_error_info_t){"UBSE_ERR_IMPORT_ABSENT", "IMPORT not present"},
    [UBS_ENGINE_ERR_CREATING - UBSE_ERR_CODE] = &(ubs_error_info_t){"UBSE_ERR_CREATING", "Creation in progress"},
    [UBS_ENGINE_ERR_DELETING - UBSE_ERR_CODE] = &(ubs_error_info_t){"UBSE_ERR_DELETING", "Deletion in progress"},
    [UBS_ENGINE_ERR_UNIMPORT_SUCCESS - UBSE_ERR_CODE] =
        &(ubs_error_info_t){"UBSE_ERR_UNIMPORT_SUCCESS", "Unimport succeeded but unexport failed"},
    [UBS_ENGINE_ERR_ALLOCATE - UBSE_ERR_CODE] = &(ubs_error_info_t){"UBSE_ERR_ALLOCATE", "Failed to Allocate."},
    [UBS_ENGINE_ERR_NODE_NOT_EXIST - UBSE_ERR_CODE] =
        &(ubs_error_info_t){"UBS_ENGINE_ERR_NODE_NOT_EXIST", "UBSE node does not exist"},
    [UBS_ENGINE_ERR_NODE_FAULT - UBSE_ERR_CODE] = &(ubs_error_info_t){"UBS_ENGINE_ERR_NODE_FAULT", "UBSE node fault"},
};

#ifdef __cplusplus
}
#endif
#endif // UBSE_ERROR_INFO_H
