// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#include "trace_context.h"
#include <dlfcn.h>

thread_local char tls_traceId[TRACE_ID_SIZE] = {'\0'};

std::atomic<void*> TraceContext::uuidLib_{nullptr};
std::atomic<bool> TraceContext::IsEnable_{false};
std::atomic<UuidGenerateRandom> TraceContext::uuidGenerateRandomFunc_{nullptr};
std::atomic<UuidUnparse> TraceContext::uuidUnparseFunc_{nullptr};
std::atomic<bool> TraceContext::IsInitialized_{false};

uint32_t TraceContext::InitUuid()
{
    // 防止重复初始化
    if (IsInitialized_.load(std::memory_order_acquire)) {
        return IsEnable_.load(std::memory_order_acquire) ? 0 : 1;
    }
    uuidLib_ = dlopen("libuuid.so.1", RTLD_LAZY);
    if (uuidLib_ == nullptr) {
        IsEnable_.store(false, std::memory_order_release);
        return 1;
    }
    uuidGenerateRandomFunc_ = (UuidGenerateRandom)dlsym(uuidLib_, "uuid_generate_random");
    if (uuidGenerateRandomFunc_ == nullptr) {
        IsEnable_.store(false, std::memory_order_release);
        dlclose(uuidLib_);
        return 1;
    }
    uuidUnparseFunc_ = (UuidUnparse)dlsym(uuidLib_, "uuid_unparse");
    if (uuidUnparseFunc_ == nullptr) {
        IsEnable_.store(false, std::memory_order_release);
        dlclose(uuidLib_);
        return 1;
    }
    IsEnable_.store(true, std::memory_order_release);
    return 0;
}