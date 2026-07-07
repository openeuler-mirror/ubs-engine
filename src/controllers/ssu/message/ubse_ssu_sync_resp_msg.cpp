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

#include "ubse_ssu_sync_resp_msg.h"

#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_serial_util.h"

namespace ubse::ssu::message {

using namespace ubse::log;

UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::serial;

UbseSsuSyncRespMsg::UbseSsuSyncRespMsg(uint32_t errCode) : errorCode(errCode)
{
}

uint32_t UbseSsuSyncRespMsg::GetErrorCode() const
{
    return errorCode;
}

uint32_t UbseSsuSyncRespMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    UbseSerialization out;
    out << errorCode;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU sync resp serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuSyncRespMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    UbseDeSerialization in(data, size);
    in >> errorCode;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "SSU sync resp deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

} // namespace ubse::ssu::message
