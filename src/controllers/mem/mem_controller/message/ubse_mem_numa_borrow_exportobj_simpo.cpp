/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
*/


#include "ubse_mem_numa_borrow_exportobj_simpo.h"
#include "ubse_mem_controller_serial.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"

namespace ubse::mem::controller::message {
using namespace ubse::message;
using namespace ubse::mem::obj;
using namespace ubse::mem::serial;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)
UbseResult UbseMemNumaBorrowExportobjSimpo::Serialize()
{
    UbseSerialization out;
    UbseMemNumaBorrowExportObjSerialization(out, exportObj);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemNumaBorrowExportobjSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (UbseMemNumaBorrowExportObjDeserialization(in, exportObj) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

}
