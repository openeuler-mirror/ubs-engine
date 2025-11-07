/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "ubse_mem_numa_borrow_req_simpo.h"

#include "ubse_mem_controller_conversion.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
namespace ubse::mem::controller::message {
using namespace serial;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)

UbseResult UbseMemNumaBorrowReqSimpo::Serialize()
{
    UbseSerialization out;
    UbseMemNumaBorrowReqSerialize(out, req);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemNumaBorrowReqSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!UbseMemNumaBorrowReqDeserialize(in, req)) {
        UBSE_LOG_ERROR << "Deserialize failed.";
        return UBSE_ERROR;
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
}