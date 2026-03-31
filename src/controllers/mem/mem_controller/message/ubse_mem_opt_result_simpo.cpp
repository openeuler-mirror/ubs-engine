/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "ubse_mem_opt_result_simpo.h"

#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_serial_util.h"

namespace ubse::mem::controller::message {
using namespace ubse::serial;
using namespace ubse::message;
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseMemOptResultSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> resp_.name >> resp_.realSize >> enum_v(resp_.stage) >> resp_.importNodeId >> result_;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemOptResultSimpo::Serialize()
{
    UbseSerialization out;
    out << resp_.name << resp_.realSize << enum_v(resp_.stage) << resp_.importNodeId << result_;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}
} // namespace ubse::mem::controller::message