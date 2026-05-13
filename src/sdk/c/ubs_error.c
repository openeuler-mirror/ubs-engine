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

#include "ubs_error.h"

#include <string.h>

#include "ubse_error_info.h"
#include "ubs_error_info.h"

// 公共查找函数
static const ubs_error_info_t* ubs_get_error_info(int32_t error)
{
    // 检查公共错误码范围
    static const size_t common_error_info_count = sizeof(common_error_infos) / sizeof(common_error_infos[0]);
    if (error >= 0 && (size_t)error < common_error_info_count) {
        return common_error_infos[error];
    }

    // 检查ubse错误码范围
    static const size_t ubse_error_info_count = sizeof(ubse_error_infos) / sizeof(ubse_error_infos[0]);
    if (error >= UBSE_ERR_CODE && (size_t)error < UBSE_ERR_CODE + ubse_error_info_count) {
        return ubse_error_infos[error - UBSE_ERR_CODE];
    }
    return NULL;
}

// API实现
const char* ubs_error_name(int32_t error)
{
    const ubs_error_info_t* info = ubs_get_error_info(error);
    if (info != NULL) {
        return info->name;
    }
    return "UBS_UNKNOWN_ERROR";
}

const char* ubs_error_string(int32_t error)
{
    const ubs_error_info_t* info = ubs_get_error_info(error);
    if (info != NULL) {
        return info->message;
    }
    return "Unrecognized error code";
}
