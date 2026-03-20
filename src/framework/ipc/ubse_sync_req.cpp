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

#include "ubse_sync_req.h"
#include <iostream>

#include "ubse_ipc_common.h"
#include "ubse_error.h"

namespace ubse::ipc {
UbseSyncReq::~UbseSyncReq()
{
    std::unique_lock<std::mutex> lock(mtx_);
    waitList_.clear();
    for (auto &resp : responses_) {
        if (resp.second.body != nullptr) {
            delete[] resp.second.body;
            resp.second.body = nullptr;
        }
    }
    responses_.clear();
}

void UbseSyncReq::RegisterRequest(uint64_t reqId)
{
    std::unique_lock<std::mutex> lock(mtx_);
    waitList_.emplace(reqId);
}

bool UbseSyncReq::IsReqIdRegister(uint64_t reqId)
{
    std::unique_lock<std::mutex> lock(mtx_);
    return waitList_.find(reqId) != waitList_.end();
}

uint32_t UbseSyncReq::WaitForResp(uint64_t reqId, int timeout, UbseResponseMessage &msg)
{
    const auto start = std::chrono::steady_clock::now();
    const auto duration = std::chrono::milliseconds(timeout);
    while (std::chrono::steady_clock::now() - start < duration) {
        mtx_.lock();
        auto respIter = responses_.find(reqId);
        mtx_.unlock();
        if (respIter == responses_.end()) {
            continue;
        }
        msg = respIter->second;
        mtx_.lock();
        responses_.erase(respIter);
        waitList_.erase(reqId);
        mtx_.unlock();
        return UBSE_OK;
    }
    mtx_.lock();
    waitList_.erase(reqId);
    mtx_.unlock();
    return UBSE_IPC_ERROR_RESP_NOT_FOUND;
}

void UbseSyncReq::StoreResp(uint64_t reqId, UbseResponseMessage msg)
{
    std::unique_lock<std::mutex> lock(mtx_);
    // 存储响应
    responses_[reqId] = msg;
}
} // namespace ubse::ipc