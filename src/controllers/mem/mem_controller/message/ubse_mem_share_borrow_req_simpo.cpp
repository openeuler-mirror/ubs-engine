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

#include "ubse_mem_share_borrow_req_simpo.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_mem_controller_serial.h"
namespace ubse::mem::controller::message {
using namespace serial;

UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseMemShareBorrowReqSimpo::Serialize()
{
    UbseSerialization out;
    if (!UbseMemShareBorrowReqSerialization(out, req_)) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemShareBorrowReqSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!UbseMemShareBorrowReqDeserialization(in, req_)) {
        UBSE_LOG_ERROR << "Deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller::message