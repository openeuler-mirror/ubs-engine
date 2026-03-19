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

#include "ubse_storage_resp_simpo.h"

#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_pointer_process.h"
#include "ubse_storage_data_conversion.h"

namespace ubse::storage::message {
using namespace ubse::log;
using namespace ubse::storage::data::conversion;

UBSE_DEFINE_THIS_MODULE("ubse");

UbseStorageRespSimpo::~UbseStorageRespSimpo()
{
    for (auto &kv : storageResp_.kvs) {
        kv.key.clear();
        SafeDeleteArray(kv.value);
    }
    storageResp_.kvs.clear();
    storageResp_.kvs.shrink_to_fit();
}

UbseStorageRespSimpo::UbseStorageRespSimpo(const UbseStorageResp &resp)
{
    storageResp_ = resp;
}

void UbseStorageRespSimpo::SetStorageResp(const UbseStorageResp &resp)
{
    storageResp_ = resp;
}

UbseStorageResp UbseStorageRespSimpo::GetStorageResp()
{
    return storageResp_;
}

UbseResult UbseStorageRespSimpo::Serialize()
{
    UbseSerialization out;
    UbseStorageRespSerialize(out, storageResp_);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    };
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

bool UbseStorageRespDeserialize(UbseDeSerialization &in,
    ubse::storage::message::UbseStorageResp &ubseStorageResp)
{
    size_t ubseStorageRespSize;
    in >> ubseStorageRespSize;
    if (!in.Check()) {
        return false;
    }
    ubseStorageResp.kvs.reserve(ubseStorageRespSize);
    for (size_t i = 0; i < ubseStorageRespSize; ++i) {
        ubse::storage::KV kv;
        if (!KVDeserialize(in, kv)) {
            for (auto &item : ubseStorageResp.kvs) {
                UbseSerialFreeFunc(item.value);
            }
            ubseStorageResp.kvs.clear();
            return false;
        }
        ubseStorageResp.kvs.push_back(kv);
    }
    return true;
}

UbseResult UbseStorageRespSimpo::Deserialize()
{
    if (hasObj_) {
        return UBSE_ERROR;
    }
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!UbseStorageRespDeserialize(in, storageResp_)) {
        UBSE_LOG_ERROR << "Deserialize failed.";
        return UBSE_ERROR;
    }
    hasObj_ = true;
    return UBSE_OK;
}

std::string UbseStorageRespSimpo::ToString() const
{
    return UbseBaseMessage::ToString();
}
}