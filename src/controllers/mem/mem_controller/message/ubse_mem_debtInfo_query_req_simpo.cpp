/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "ubse_mem_debtInfo_query_req_simpo.h"
#include "src/framework/serde/ubse_serial_util.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"

namespace ubse::mem::controller::message {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)

using namespace ubse::serial;
UbseResult NodeMemDebtInfoQueryReqSimpo::Serialize()
{
    UbseSerialization out;
    out << nodeId;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult NodeMemDebtInfoQueryReqSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> nodeId;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller::message