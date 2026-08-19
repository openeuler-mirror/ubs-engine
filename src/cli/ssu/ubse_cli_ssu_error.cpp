/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_cli_ssu_error.h"

#include "ubse_error.h"

namespace ubse::cli::reg {
std::string GetSsuErrorMessage(uint32_t errorCode)
{
    switch (errorCode) {
        case UBSE_ERR_INVALID_ARG:
            return "ERROR: The SSU request contains invalid arguments.";
        case UBSE_ERR_ACCESS_DENIED:
            return "ERROR: Access to the SSU allocation is denied.";
        case UBSE_ERR_TIMED_OUT:
            return "ERROR: The SSU operation timed out.";
        case UBSE_ERR_ALREADY_ALLOCATED:
            return "ERROR: The SSU allocation already exists.";
        case UBSE_ERR_ALREADY_ATTACHED:
            return "ERROR: The SSU allocation is already attached.";
        case UBSE_ERR_NO_NEED_FREE:
            return "ERROR: The SSU allocation does not exist or has already been deleted.";
        case UBSE_ERR_NO_NEED_DETACH:
            return "ERROR: The SSU allocation is already detached or has not been attached.";
        case UBSE_SSU_ERROR_SPACE_NOT_FOUND:
            return "INFO: The SSU allocation does not exist.";
        case UBSE_SSU_ERROR_NEED_DETACH_BEFORE_FREE:
            return "ERROR: The SSU allocation is attached. Detach it before deleting it.";
        case UBSE_SSU_ERROR_STRATEGY_MISMATCH:
            return "ERROR: The SSU allocation strategy does not match the attach or detach type.";
        default:
            return "ERROR: Internal error with error code " + std::to_string(errorCode) + ".";
    }
}
} // namespace ubse::cli::reg
