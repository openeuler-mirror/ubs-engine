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

#ifndef LOGGING_LOCK_GUARD_H
#define LOGGING_LOCK_GUARD_H

#include <mutex>
#include <memory>
#include <unordered_map>

namespace ubse::mem::controller {

class LoggingLockGuard {
public:
    explicit LoggingLockGuard(const std::string &name);

    ~LoggingLockGuard();

    // 禁止复制
    LoggingLockGuard(const LoggingLockGuard &) = delete;
    LoggingLockGuard &operator=(const LoggingLockGuard &) = delete;

private:
    std::shared_ptr<std::mutex> GetObjMutex(const std::string objId);
    void RemoveObjMutex(const std::string objId);

    std::shared_ptr<std::mutex> mutex;
    std::string name;

    static std::unordered_map<std::string, std::shared_ptr<std::mutex>> mutexMap;
    static std::mutex mapMutex;
};
} // namespace ubse::mem::controller

#endif // LOGGING_LOCK_GUARD_H
