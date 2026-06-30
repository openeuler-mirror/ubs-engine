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

#ifndef UBSE_LOGGING_LOCK_GUARD_H
#define UBSE_LOGGING_LOCK_GUARD_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ubse::utils {

class UbseLoggingLockGuard {
public:
    explicit UbseLoggingLockGuard(const std::string &name);
    ~UbseLoggingLockGuard();

    // 禁止复制
    UbseLoggingLockGuard(const UbseLoggingLockGuard &) = delete;
    UbseLoggingLockGuard &operator=(const UbseLoggingLockGuard &) = delete;

private:
    std::shared_ptr<std::mutex> GetObjMutex(const std::string &objId);
    void RemoveObjMutex(const std::string &objId);

    std::shared_ptr<std::mutex> mutex_;
    std::string name_;

    static std::unordered_map<std::string, std::shared_ptr<std::mutex>> mutexMap;
    static std::mutex mapMutex;
};

} // namespace ubse::utils

#endif // UBSE_LOGGING_LOCK_GUARD_H
