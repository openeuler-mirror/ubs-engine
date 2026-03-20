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

#include <securec.h>           // for memcpy_s, EOK, errno_t
#include <cstdint>             // for uint8_t, uint32_t
#include <new>                 // for nothrow

#include "ubse_error.h"        // for UBSE_ERROR, UBSE_OK, UBSE_ERROR_NU...
#include "ubse_logger.h"       // for UBSE_DEFINE_THIS_MODULE, UbseLogge...
#include "ubse_pointer_process.h"
#include "ubse_base_message.h"

namespace ubse::message {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

UbseBaseMessagePtr UbseBaseMessage::gNullPtr(nullptr);
constexpr int MAX_IPC_DATA_PACKAGE_LEN = 10485760;
UbseResult UbseBaseMessage::SetInputRawDataFromShared(std::shared_ptr<uint8_t[]> data, uint32_t size)
{
    if (!data || size == 0 || size > MAX_IPC_DATA_PACKAGE_LEN) {
        UBSE_LOG_ERROR << "Invalid ipc size, which is " << size;
        return UBSE_ERROR;
    }
    mInputRawData = std::move(data);
    mInputRawDataSize = size;
    return UBSE_OK;
}

UbseResult UbseBaseMessage::SetInputRawData(uint8_t *rawData, uint32_t size, bool copy)
{
    if (rawData == nullptr) {
        UBSE_LOG_ERROR << "set_input_raw_data input rawData is null.";
        return UBSE_ERROR;
    }
    if (size == 0 || size > MAX_IPC_DATA_PACKAGE_LEN) {
        UBSE_LOG_ERROR << "Invalid ipc size, which is " << size;
        return UBSE_ERROR;
    }
    // 深拷贝
    auto buffer = SafeMakeShared(size);
    if (!buffer) {
        UBSE_LOG_ERROR << "make shared ptr failed";
        return UBSE_ERROR;
    }
    errno_t err = memcpy_s(buffer.get(), size, rawData, size);
    if (err != EOK) {
        UBSE_LOG_ERROR << "memcpy_s in set_input_raw_data failed.";
        return UBSE_ERROR;
    }
    return SetInputRawDataFromShared(std::move(buffer), size);
}
} // namespace ubse::message