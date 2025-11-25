/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "node_mem_debt_info_simpo.h"
#include "ubse_mem_controller_conversion.h"
#include "ubse_mem_node_debt_info_conversion.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
namespace ubse::mem::controller::message {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)
using namespace ubse::mem::serial;
UbseResult NodeMemDebtInfoSimpo::Serialize()
{
    UbseSerialization out;
    NodeMemDebtInfoSerialize(out, data);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult NodeMemDebtInfoSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!NodeMemDebtInfoDeserialize(in, data)) {
        UBSE_LOG_ERROR << "Deserialize failed.";
        return UBSE_ERROR;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller::message