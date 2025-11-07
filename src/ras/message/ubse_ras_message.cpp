// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#include "ubse_ras_message.h"
#include "src/include/ubse_logger_inner.h"

namespace ubse::ras {
using namespace ubse::utils;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_RAS)

UbseResult UbseRasMessage::Serialize()
{
    UbseSerialization out;
    Serialization(out, data);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}
UbseResult UbseRasMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (Deserialization(in, data) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
void UbseRasMessage::Serialization(UbseSerialization &out, RasData &serialData)
{
    out << serialData.data << serialData.result;
    return;
}

UbseResult UbseRasMessage::Deserialization(UbseDeSerialization &in, RasData &deSerialData)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemDebtNumaInfo during deserialization";
        return UBSE_ERROR;
    }
    in >> deSerialData.data >> deSerialData.result;
    return UBSE_OK;
}
} // namespace ubse::ras