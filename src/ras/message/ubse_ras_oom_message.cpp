/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#include "ubse_ras_oom_message.h"
#include "ubse_logger_module.h"

namespace ubse::ras {
using namespace ubse::utils;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseRasOomMessage::Serialize()
{
    UbseSerialization out;
    Serialization(out);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}
UbseResult UbseRasOomMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (Deserialization(in) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
void UbseRasOomMessage::Serialization(UbseSerialization& out)
{
    out << nodeId << memNeed << oomNumaId;
    return;
}

UbseResult UbseRasOomMessage::Deserialization(UbseDeSerialization& in)
{
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to check UbseMemDebtNumaInfo during deserialization";
        return UBSE_ERROR;
    }
    in >> nodeId >> memNeed >> oomNumaId;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::ras