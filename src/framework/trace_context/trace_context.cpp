// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#include "trace_context.h"
#include <dlfcn.h>
#include <chrono>
#include <thread>

thread_local char tls_traceId[TRACE_ID_SIZE] = {'\0'};
void *TraceContext::uuidLib_ = nullptr;
bool TraceContext::IsEnable_ = false;
UuidGenerateRandom TraceContext::uuidGenerateRandomFunc_ = nullptr;
UuidUnparse TraceContext::uuidUnparseFunc_ = nullptr;
std::once_flag TraceContext::initFlag_;

// 进程退出时释放资源
static void CleanupUuidLib()
{
    if (TraceContext::uuidLib_ != nullptr) {
        dlclose(TraceContext::uuidLib_);
        TraceContext::uuidLib_ = nullptr;
    }
}

uint32_t TraceContext::InitUuid()
{
    std::call_once(initFlag_, []() {
        uuidLib_ = dlopen("libuuid.so.1", RTLD_LAZY);
        if (uuidLib_ == nullptr) {
            IsEnable_ = false;
            return;
        }
        uuidGenerateRandomFunc_ =
            (UuidGenerateRandom)dlsym(uuidLib_, "uuid_generate_random");
        uuidUnparseFunc_ =
            (UuidUnparse)dlsym(uuidLib_, "uuid_unparse");
        if (uuidGenerateRandomFunc_ == nullptr ||
            uuidUnparseFunc_ == nullptr) {
            IsEnable_ = false;
            dlclose(uuidLib_);
            uuidLib_ = nullptr;
            return;
        }
        IsEnable_ = true;
        // 注册退出清理
        atexit(CleanupUuidLib);
    });
    return IsEnable_ ? 0 : 1;
}