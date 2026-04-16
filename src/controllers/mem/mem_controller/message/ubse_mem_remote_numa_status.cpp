/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */


#include "ubse_mem_remote_numa_status.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_mem_controller_serial.h"
namespace ubse::mem::controller::message {
using namespace serial;

UBSE_DEFINE_THIS_MODULE("ubse");
UbseResult UbseMemRemoteNumaStatus::Serialize()
{
    UbseSerialization out;
    out << array_len_insert(numaStatus_.size());
    for (size_t i = 0; i < numaStatus_.size(); ++i) {
        out << numaStatus_[i].first << numaStatus_[i].second;
    }
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize numaStatus failed";
        return UBSE_ERROR;
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemRemoteNumaStatus::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    numaStatus_.clear();
    uint64_t len = 1;
    in >> array_len_capture(len);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Deserialize numaStatus failed, len=" << len;
        return UBSE_ERROR;
    }
    for (uint64_t i = 0; i < len; ++i) {
        std::pair<int64_t, int> pair;
        in >> pair.first >> pair.second;
        numaStatus_.push_back(pair);
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Deserialize numaStatus failed, len=" << len;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller::message