/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "ubse_mem_operation_resp_simpo.h"
#include "ubse_mem_controller_conversion.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
namespace ubse::mem::controller::message {
using namespace serial;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)

UbseResult UbseMemOperationRespSimpo::Serialize()
{
    UbseSerialization out;
    UbseMemOperationRespSerialize(out, resp);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemOperationRespSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!UbseMemOperationRespDeserialize(in, resp)) {
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