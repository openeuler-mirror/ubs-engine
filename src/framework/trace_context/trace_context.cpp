// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#include "trace_context.h"
#include <dlfcn.h>
#include <mutex>

thread_local char tls_traceId[TRACE_ID_SIZE] = {0};
thread_local size_t tls_traceIdLen = 0;  // 新增

void *TraceContext::uuidLib_ = nullptr;
bool TraceContext::IsEnable_ = false;
UuidGenerateRandom TraceContext::uuidGenerateRandomFunc_ = nullptr;
UuidUnparse TraceContext::uuidUnparseFunc_ = nullptr;
std::mutex TraceContext::initMutex_;

bool TraceContext::IsValidTraceId(const char* traceId)
{
    if (!traceId || strlen(traceId) != 36) {
        return false;
    }
    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (traceId[i] != '-') return false;
        } else {
            if (!isxdigit(traceId[i])) return false;
        }
    }
    return true;
}

uint32_t TraceContext::InitUuid()
{
    if (IsEnable_ && uuidLib_ != nullptr) {
        return 0;  // 已初始化
    }

    std::lock_guard<std::mutex> lock(initMutex_);
    return loadUuidLibrary() ? 0 : 1;
}

bool TraceContext::loadUuidLibrary()
{
    if (uuidLib_ != nullptr) {
        return IsEnable_;
    }
    uuidLib_ = dlopen("libuuid.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (uuidLib_ == nullptr) {
        uuidLib_ = dlopen("libuuid.so", RTLD_LAZY | RTLD_LOCAL);
    }
    if (uuidLib_ == nullptr) {
        IsEnable_ = false;
        return false;
    }
    uuidGenerateRandomFunc_ = (UuidGenerateRandom)dlsym(uuidLib_, "uuid_generate_random");
    uuidUnparseFunc_ = (UuidUnparse)dlsym(uuidLib_, "uuid_unparse");
    if (uuidGenerateRandomFunc_ == nullptr || uuidUnparseFunc_ == nullptr) {
        dlclose(uuidLib_);
        uuidLib_ = nullptr;
        IsEnable_ = false;
        return false;
    }
    IsEnable_ = true;
    return true;
}

void TraceContext::generateTraceId()
{
    if (!IsEnable_ || uuidGenerateRandomFunc_ == nullptr || uuidUnparseFunc_ == nullptr) {
        tls_traceId[0] = '\0';
        tls_traceIdLen = 0;
        return;
    }
    uuid_t uuid;
    uuidGenerateRandomFunc_(uuid);
    char buf[TRACE_ID_SIZE];
    uuidUnparseFunc_(uuid, buf);
    buf[TRACE_ID_SIZE - 1] = '\0';
    if (strcpy_s(tls_traceId, TRACE_ID_SIZE, buf) == EOK) {
        tls_traceIdLen = 36;  // UUID固定长度
    } else {
        tls_traceId[0] = '\0';
        tls_traceIdLen = 0;
    }
}

uint32_t TraceContext::Cleanup()
{
    std::lock_guard<std::mutex> lock(initMutex_);
    if (uuidLib_ != nullptr) {
        dlclose(uuidLib_);
        uuidLib_ = nullptr;
    }
    uuidGenerateRandomFunc_ = nullptr;
    uuidUnparseFunc_ = nullptr;
    IsEnable_ = false;
    return 0;
}