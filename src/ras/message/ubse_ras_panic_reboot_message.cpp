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

#include "ubse_ras_panic_reboot_message.h"
#include "ubse_logger.h"

namespace ubse::ras {
using namespace ubse::utils;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseRasPanicRebootMessage::Serialize()
{
    UbseSerialization out;
    out << mErrCode;
    Serialization(out, data);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseRasPanicRebootMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> mErrCode;
    if (Deserialization(in, data) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseRasPanicRebootMessage::Serialization(UbseSerialization& out, RasPanicRebootData& serialData)
{
    out << serialData.faultEid << serialData.msgId << serialData.faultType << serialData.result
        << serialData.forwardNodeId;
}

UbseResult UbseRasPanicRebootMessage::Deserialization(UbseDeSerialization& in, RasPanicRebootData& deSerialData)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check RasPanicRebootData during deserialization.";
        return UBSE_ERROR;
    }
    in >> deSerialData.faultEid >> deSerialData.msgId >> deSerialData.faultType >> deSerialData.result >>
        deSerialData.forwardNodeId;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::ras
