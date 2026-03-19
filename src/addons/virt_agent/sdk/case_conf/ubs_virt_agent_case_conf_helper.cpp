/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubs_virt_agent_case_conf_helper.h"

#include <ubse_ipc_log.h>

virt_agent_ret_t ubse_case_conf_info_unpack(uint8_t *buffer, uint32_t len, case_conf_info_t *case_conf_info)
{
    vm::CaseConfGetMsg msg{buffer, len};
    auto ret = msg.Deserialize();
    if (ret != 0) {
        IPC_LOG_ERROR << "Failed to deserialize CaseConfGetMsg. Error code: " << ret;
        return VA_ERROR_DESERIALIZE_FAILED;
    }
    *case_conf_info = msg.GetCaseConf();
    return VA_SUCCESS;
}

virt_agent_ret_t ubse_case_conf_set_unpack(uint8_t *buffer, uint32_t len, case_conf_set_info_t *case_conf_info)
{
    vm::CaseConfSetMsg msg{buffer, len};
    auto ret = msg.Deserialize();
    if (ret != 0) {
        IPC_LOG_ERROR << "Failed to deserialize CaseConfGetMsg. Error code: " << ret;
        return VA_ERROR_DESERIALIZE_FAILED;
    }
    *case_conf_info = msg.GetCaseConf();
    return VA_SUCCESS;
}
