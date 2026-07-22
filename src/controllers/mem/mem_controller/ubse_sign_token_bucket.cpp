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

#include "ubse_sign_token_bucket.h"

namespace ubse::mem::controller {

constexpr size_t MAX_TOKENS = 15;

UbseSignTokenBucket::UbseSignTokenBucket() : maxTokens_(MAX_TOKENS), availableTokens_(MAX_TOKENS) {}

UbseSignTokenBucket::TokenGuard::TokenGuard(UbseSignTokenBucket* bucket, bool valid) : bucket_(bucket), valid_(valid) {}

UbseSignTokenBucket::TokenGuard::TokenGuard(TokenGuard&& other) noexcept : bucket_(other.bucket_), valid_(other.valid_)
{
    other.valid_ = false;
}

UbseSignTokenBucket::TokenGuard& UbseSignTokenBucket::TokenGuard::operator=(TokenGuard&& other) noexcept
{
    if (this != &other) {
        if (valid_) {
            bucket_->ReleaseToken();
        }
        bucket_ = other.bucket_;
        valid_ = other.valid_;
        other.valid_ = false;
    }
    return *this;
}

UbseSignTokenBucket::TokenGuard::~TokenGuard()
{
    if (valid_) {
        bucket_->ReleaseToken();
    }
}

UbseSignTokenBucket::TokenGuard UbseSignTokenBucket::Acquire()
{
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]() { return availableTokens_ > 0; });
    availableTokens_--;
    return TokenGuard(this, true);
}

void UbseSignTokenBucket::ReleaseToken()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (availableTokens_ < maxTokens_) {
        availableTokens_++;
    }
    cv_.notify_one();
}
} // namespace ubse::mem::controller