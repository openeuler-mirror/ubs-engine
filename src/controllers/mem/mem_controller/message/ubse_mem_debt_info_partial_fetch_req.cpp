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

#include "ubse_mem_debt_info_partial_fetch_req.h"
#include "ubse_base_message.h"
#include "ubse_error.h"
#include "ubse_logger.h"       // for UBSE_DEFINE_THIS_MODULE, UbseLogge...
#include "ubse_pointer_process.h"
namespace ubse::mem::controller::message {
using namespace ubse::message;
using namespace ubse::serial;
UBSE_DEFINE_THIS_MODULE("ubse");
UbseResult UbseMemDebtInfoPartialFetchReq::Serialize()
{
    UbseSerialization out;
    if (!DebtFetchInfo::Serialize(out, debtFetchInfo_)) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemDebtInfoPartialFetchReq::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!DebtFetchInfo::Deserialize(in, debtFetchInfo_)) {
        UBSE_LOG_ERROR << "Deserialize failed.";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller::message