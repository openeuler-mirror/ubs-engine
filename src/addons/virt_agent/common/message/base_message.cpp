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

#include "base_message.h"

#include <ubse_logger.h>
#include "vm_def.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;

BaseMessagePtr BaseMessage::gNullPtr(nullptr);

VmResult BaseMessage::SetInputRawData(uint8_t *rawData, uint32_t size, bool copy)
{
    if (rawData == nullptr) {
        UBSE_LOG_ERROR << "set_input_raw_data input rawData is null.";
        return VM_ERROR_SERIALIZE_DESERIALIZE_COMMON_ERROR;
    }
    if (size == 0 || size > MAX_IPC_DATA_PACKAGE_LEN) {
        UBSE_LOG_ERROR << "Invalid ipc size, which is " << size;
        return VM_ERROR_SERIALIZE_DESERIALIZE_COMMON_ERROR;
    }
    if (mInputRawDataOwned && mInputRawData != nullptr) {
        SafeDeleteArray(mInputRawData, mInputRawDataSize);
    }
    if (!copy) {
        mInputRawData = rawData;
        mInputRawDataSize = size;
        mInputRawDataOwned = false; // Not managing the lifecycle of external data
    } else {
        mInputRawData = new (std::nothrow) uint8_t[size];
        if (mInputRawData == nullptr) {
            UBSE_LOG_ERROR << "mInputRawData new failed.";
            return VM_ERROR_NULLPTR;
        }
        errno_t err = memcpy_s(mInputRawData, size, rawData, size);
        if (err != EOK) {
            SafeDeleteArray(mInputRawData, size);
            UBSE_LOG_ERROR << "memcpy_s in set_input_raw_data failed.";
            return VM_ERROR_SERIALIZE_DESERIALIZE_COMMON_ERROR;
        }
        mInputRawDataSize = size;
        mInputRawDataOwned = true; // Manage the lifecycle of internal data
    }
    return VM_OK;
}
} // namespace vm
