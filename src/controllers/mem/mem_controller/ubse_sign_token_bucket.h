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

#ifndef CONCURRENT_SIGN_TOKEN_BUCKET_H
#define CONCURRENT_SIGN_TOKEN_BUCKET_H

#include <condition_variable>
#include <mutex>

namespace ubse::mem::controller {
class UbseSignTokenBucket {
public:
    static UbseSignTokenBucket& GetInstance()
    {
        static UbseSignTokenBucket instance;
        return instance;
    }

    UbseSignTokenBucket(const UbseSignTokenBucket&) = delete;
    UbseSignTokenBucket& operator=(const UbseSignTokenBucket&) = delete;
    UbseSignTokenBucket(UbseSignTokenBucket&&) = delete;
    UbseSignTokenBucket& operator=(UbseSignTokenBucket&&) = delete;

    class TokenGuard {
    public:
        TokenGuard(const TokenGuard&) = delete;
        TokenGuard& operator=(const TokenGuard&) = delete;

        TokenGuard(TokenGuard&& other) noexcept;
        TokenGuard& operator=(TokenGuard&& other) noexcept;
        ~TokenGuard();

        bool IsValid() const
        {
            return valid_;
        }

    private:
        friend class UbseSignTokenBucket;
        TokenGuard(UbseSignTokenBucket* bucket, bool valid);

        UbseSignTokenBucket* bucket_;
        bool valid_;
    };

    TokenGuard Acquire();

private:
    UbseSignTokenBucket();
    ~UbseSignTokenBucket() = default;
    void ReleaseToken();

    const size_t maxTokens_;
    size_t availableTokens_;
    std::mutex mutex_;
    std::condition_variable cv_;
};
} // namespace ubse::mem::controller

#endif