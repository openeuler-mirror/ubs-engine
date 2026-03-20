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

#include "ubs_virt_agent_mem_migrate.h"

#include <ubse_ipc_client.h>
#include <ubse_ipc_log.h>
#include "src/sdk/c/include/ubs_error.h"
#include "vm_sdk_def.h"
#include "mem_migrate_msg.h"


int32_t update_page_flow_and_status(const char *opt, const char *uuid)
{
    if (opt == nullptr || uuid == nullptr) {
        IPC_LOG_ERROR << "param invalid";
        return VA_ERROR_INVALID_PARAM;
    }
    if (strnlen(opt, SDK_NO_128) >= SDK_NO_128 || strnlen(uuid, SDK_NO_128) >= SDK_NO_128) {
        IPC_LOG_ERROR << "param invalid";
        return VA_ERROR_INVALID_PARAM;
    }
    vm::MemMigrateInputParams inputParams{.opt = opt, .uuid = uuid};
    vm::MemMigrateMsg memMigrateMsg{inputParams};
    auto ret = memMigrateMsg.Serialize();
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "HamMakeDecisionMsg Serialize failed";
        return VA_ERROR_SERIALIZE_FAILED;
    }
    ubse_api_buffer_t requestBuffer = {memMigrateMsg.SerializedData(), memMigrateMsg.SerializedDataSize()};
    ubse_api_buffer_t responseBuffer;
    ret = ubse_invoke_call(UBS_VA_VM_MIGRATE, UBS_VA_PAGE_FLOW_AND_UPDATE_STATUS, &requestBuffer, &responseBuffer);
    ubse_api_buffer_delete(&requestBuffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
        ubse_api_buffer_free(&responseBuffer);
        return VA_ERROR_BASE;
    }
    ubse_api_buffer_free(&responseBuffer);
    return ret;
}
