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

#include "request_helper.h"

#include <any>
#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_map>
#include "ubse_error.h"
#include "ubse_logger_module.h"

namespace ubse::mem_controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
std::shared_ptr<FutureMgr> FutureMgr::CreateInstance(const std::string& requestId)
{
    std::unique_lock<std::mutex> mapLock(mapMutex);

    // 如果存在相同requestId未完成，则等待其完成.
    while (mgrInstanceMap.find(requestId) != mgrInstanceMap.end()) {
        waitRequestFinishedCv.wait(mapLock);
    }

    auto newFutureObject = SafeMakeShared<FutureMgr>(requestId);
    if (!newFutureObject) {
        UBSE_LOG_ERROR << "make shared ptr failed, newFutureObject is nullptr";
        return newFutureObject;
    }
    mgrInstanceMap[requestId] = newFutureObject;
    return newFutureObject;
}

bool FutureMgr::SetResult(const std::string& requestId, const std::any& result)
{
    std::unique_lock<std::mutex> lock(mapMutex);
    auto it = mgrInstanceMap.find(requestId);
    if (it == mgrInstanceMap.end()) {
        return false;
    }
    auto futureMgr = it->second.lock();
    if (!futureMgr) {
        return false;
    }
    lock.unlock();
    return futureMgr->SetResult(result);
}

bool FutureMgr::Find(const std::string& requestId)
{
    std::unique_lock<std::mutex> lock(mapMutex);
    auto it = mgrInstanceMap.find(requestId);
    if (it == mgrInstanceMap.end()) {
        return false;
    }
    auto futureMgr = it->second.lock();
    if (!futureMgr) {
        return false;
    }
    return true;
}

size_t FutureMgr::GetSize()
{
    std::unique_lock<std::mutex> lock(mapMutex);
    return mgrInstanceMap.size();
}

bool FutureMgr::SetResult(const std::any& result)
{
    std::unique_lock<std::mutex> lock(mtx_);
    bool match = false;
    if (curObjPromise_) {
        match = curObjPromise_->SetResult(result);
        curObjPromise_.reset();
    }
    return match;
}

FutureMgr::~FutureMgr()
{
    // 析构时，清理objMgrInstanceMap，并通知有请求完成.
    std::unique_lock<std::mutex> lockMap(mapMutex);
    mgrInstanceMap.erase(requestIdInner_);
    waitRequestFinishedCv.notify_all();
}

std::mutex FutureMgr::mapMutex;
std::condition_variable FutureMgr::waitRequestFinishedCv;
std::unordered_map<std::string, std::weak_ptr<FutureMgr>> FutureMgr::mgrInstanceMap;
} // namespace ubse::mem_controller
