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

#include "framework/misc/ubse_future_mgr.h"

namespace ubse::misc::future {
using namespace ubse::log;

std::shared_ptr<UbseFutureMgr> UbseFutureMgr::CreateInstance(const std::string &requestId)
{
    std::unique_lock<std::mutex> mapLock(mapMutex_);

    // 如果存在相同requestId未完成，则等待其完成.
    while (mgrInstanceMap_.find(requestId) != mgrInstanceMap_.end()) {
        waitRequestFinishedCv_.wait(mapLock);
    }

    auto newFutureObject = SafeMakeShared<UbseFutureMgr>(requestId);
    if (!newFutureObject) {
        UBSE_LOG_ERROR << "make shared ptr failed, newFutureObject is nullptr";
        return newFutureObject;
    }
    mgrInstanceMap_[requestId] = newFutureObject;
    return newFutureObject;
}

bool UbseFutureMgr::SetResult(const std::string &requestId, const std::any &result)
{
    std::unique_lock<std::mutex> lock(mapMutex_);
    auto it = mgrInstanceMap_.find(requestId);
    if (it == mgrInstanceMap_.end()) {
        return false;
    }
    auto futureMgr = it->second.lock();
    if (!futureMgr) {
        return false;
    }
    lock.unlock();
    return futureMgr->SetResult(result);
}

bool UbseFutureMgr::Find(const std::string &requestId)
{
    std::unique_lock<std::mutex> lock(mapMutex_);
    auto it = mgrInstanceMap_.find(requestId);
    if (it == mgrInstanceMap_.end()) {
        return false;
    }
    auto futureMgr = it->second.lock();
    if (!futureMgr) {
        return false;
    }
    return true;
}

size_t UbseFutureMgr::GetSize()
{
    std::unique_lock<std::mutex> lock(mapMutex_);
    return mgrInstanceMap_.size();
}

bool UbseFutureMgr::SetResult(const std::any &result)
{
    std::unique_lock<std::mutex> lock(mtx_);
    bool match = false;
    if (curObjPromise_) {
        match = curObjPromise_->SetResult(result);
        curObjPromise_.reset();
    }
    return match;
}

UbseFutureMgr::~UbseFutureMgr()
{
    // 析构时，清理objMgrInstanceMap，并通知有请求完成.
    std::unique_lock<std::mutex> lockMap(mapMutex_);
    mgrInstanceMap_.erase(requestIdInner_);
    waitRequestFinishedCv_.notify_all();
}

std::mutex UbseFutureMgr::mapMutex_;
std::condition_variable UbseFutureMgr::waitRequestFinishedCv_;
std::unordered_map<std::string, std::weak_ptr<UbseFutureMgr>> UbseFutureMgr::mgrInstanceMap_;
} // namespace ubse::misc::future
