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

#ifndef UBSE_STORAGE_DATA_CONVERSION_H
#define UBSE_STORAGE_DATA_CONVERSION_H

#include "ubse_serial_util.h"
#include "ubse_storage.h"
#include "ubse_storage_req_simpo.h"
#include "ubse_storage_resp_simpo.h"

namespace ubse::storage::data::conversion {
using namespace ubse::serial;
using namespace ubse::storage::message;

inline void UbseStorageReqSerialize(UbseSerialization &out, UbseStorageReq &ubseStorageReq)
{
    out << enum_v(ubseStorageReq.cmdType) << ubseStorageReq.dbName << ubseStorageReq.key;
}

inline void UbseStorageReqDeserialize(UbseDeSerialization &in, UbseStorageReq &ubseStorageReq)
{
    in >> enum_v(ubseStorageReq.cmdType) >> ubseStorageReq.dbName >> ubseStorageReq.key;
}

inline void KVReqSerialize(UbseSerialization &out, KV &kv)
{
    out << kv.key;
    out << addr_len(kv.value, kv.valueLen);
}

inline bool KVDeserialize(UbseDeSerialization &in, KV &kv)
{
    in >> kv.key;
    alloc_addr_len_<uint8_t> allocAddrLen;
    in >> allocAddrLen;
    if (!in.Check()) {
        UbseSerialFreeFunc(allocAddrLen.ptr);
        return false;
    }
    kv.valueLen = allocAddrLen.len;
    kv.value = allocAddrLen.ptr;
    return true;
}

inline void UbseStorageRespSerialize(UbseSerialization &out, UbseStorageResp &ubseStorageResp)
{
    out << (right_v<size_t>(ubseStorageResp.kvs.size()));
    for (auto kv : ubseStorageResp.kvs) {
        KVReqSerialize(out, kv);
    }
}
} // namespace ubse::storage::data::conversion
#endif // UBSE_STORAGE_DATA_CONVERSION_H
