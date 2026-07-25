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

#include "ubse_storage_req_simpo.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_storage_data_conversion.h"
#include "securec.h"

namespace ubse::storage::message {
using namespace ubse::log;
using namespace ubse::storage::data::conversion;

UBSE_DEFINE_THIS_MODULE("ubse");

UbseStorageReqSimpo::UbseStorageReqSimpo(const UbseStorageReq& req)
{
    storageReq_ = req;
}

UbseStorageReq UbseStorageReqSimpo::GetStorageReq()
{
    return storageReq_;
}

UbseResult UbseStorageReqSimpo::Serialize()
{
    UbseSerialization out;
    UbseStorageReqSerialize(out, storageReq_);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    };
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseStorageReqSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    UbseStorageReqDeserialize(in, storageReq_);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

std::string ReqCmdTypeToString(const UbseStorageReqCmdType type)
{
    switch (type) {
        case UbseStorageReqCmdType::GET:
            return "Get";
        case UbseStorageReqCmdType::GET_WITH_PREFIX:
            return "GetWithPrefix";
        default:
            return "Unknown";
    }
}

std::string UbseStorageReqSimpo::ToString() const
{
    std::ostringstream str;
    str << "UbseStorageReqSimpo(" << ReqCmdTypeToString(storageReq_.cmdType) << "," << storageReq_.dbName << ","
        << storageReq_.key << ")";
    return str.str();
}
} // namespace ubse::storage::message