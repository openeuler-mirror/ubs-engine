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

#include "framework/misc/ubse_logging_lock_guard.h"

#include <mutex>
#include <unordered_map>
#include "ubse_error.h"
#include "ubse_logger.h"

namespace ubse::utils {

std::unordered_map<std::string, std::shared_ptr<std::mutex>> UbseLoggingLockGuard::mutexMap{};
std::mutex UbseLoggingLockGuard::mapMutex;
// 当引用数为2时(1个是mutexMap引用 + 1个是当前的UbseLoggingLockGuard实例引用)，意味着只有一个使用方，此时UbseLoggingLockGuard析构，可以清理mutexMap
const uint32_t USE_COUNT_WHEN_ONLY_ONE_USER = 2;

UBSE_DEFINE_THIS_MODULE("ubse");

std::shared_ptr<std::mutex> UbseLoggingLockGuard::GetObjMutex(const std::string &objId)
{
    std::unique_lock<std::mutex> lock(mapMutex);
    if (mutexMap.find(objId) == mutexMap.end()) {
        mutexMap.emplace(objId, std::make_shared<std::mutex>());
    }
    return mutexMap[objId];
}

void UbseLoggingLockGuard::RemoveObjMutex(const std::string &objId)
{
    std::unique_lock<std::mutex> lock(mapMutex);
    if (mutexMap.find(objId) != mutexMap.end()) {
        if (mutexMap[objId].use_count() == USE_COUNT_WHEN_ONLY_ONE_USER) {
            mutexMap.erase(objId);
        }
    }
}

UbseLoggingLockGuard::UbseLoggingLockGuard(const std::string &name) : mutex_(GetObjMutex(name)), name_(name)
{
    (*mutex_).lock();
    UBSE_LOG_INFO << "Locked name=" << name;
}

UbseLoggingLockGuard::~UbseLoggingLockGuard()
{
    (*mutex_).unlock();
    UBSE_LOG_INFO << "UnLocked name=" << name_;
    RemoveObjMutex(name_);
}

} // namespace ubse::utils
