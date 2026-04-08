// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#include "trace_context.h"
#include <dlfcn.h>

thread_local char tls_traceId[TRACE_ID_SIZE] = {'\0'};

std::atomic<void*> TraceContext::uuidLib_{nullptr};
std::atomic<UuidGenerateRandom> TraceContext::uuidGenerateRandomFunc_{nullptr};
std::atomic<UuidUnparse> TraceContext::uuidUnparseFunc_{nullptr};
std::atomic<bool> TraceContext::IsEnabled_{false};

uint32_t TraceContext::InitUuid()
{
    // 防止重复初始化
    bool expected = false;
    if (!IsEnabled_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return 0;  // 已初始化，返回成功
    }
    void* lib = dlopen("libuuid.so.1", RTLD_LAZY);
    if (lib == nullptr) {
        IsEnabled_.store(false, std::memory_order_release);
        return 1;
    }
    auto generateFunc = (UuidGenerateRandom)dlsym(lib, "uuid_generate_random");
    if (generateFunc == nullptr) {
        IsEnabled_.store(false, std::memory_order_release);
        dlclose(lib);
        return 1;
    }
    auto unparseFunc = (UuidUnparse)dlsym(lib, "uuid_unparse");
    if (unparseFunc == nullptr) {
        IsEnabled_.store(false, std::memory_order_release);
        dlclose(lib);
        return 1;
    }
    uuidLib_.store(lib, std::memory_order_release);
    uuidGenerateRandomFunc_.store(generateFunc, std::memory_order_release);
    uuidUnparseFunc_.store(unparseFunc, std::memory_order_release);
    return 0;
}