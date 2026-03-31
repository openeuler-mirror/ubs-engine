// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#include "trace_context.h"
#include <dlfcn.h>

thread_local char tls_traceId[TRACE_ID_SIZE] = {'\0'};

void *TraceContext::uuidLib_ = nullptr;
bool TraceContext::IsEnable_ = false;
UuidGenerateRandom TraceContext::uuidGenerateRandomFunc_ = nullptr;
UuidUnparse TraceContext::uuidUnparseFunc_ = nullptr;

uint32_t TraceContext::InitUuid()
{
    uuidLib_ = dlopen("libuuid.so.1", RTLD_LAZY);
    if (uuidLib_ == nullptr) {
        IsEnable_ = false;
        return 1;
    }
    uuidGenerateRandomFunc_ = (UuidGenerateRandom)dlsym(uuidLib_, "uuid_generate_random");
    if (uuidGenerateRandomFunc_ == nullptr) {
        IsEnable_ = false;
        dlclose(uuidLib_);
        return 1;
    }
    uuidUnparseFunc_ = (UuidUnparse)dlsym(uuidLib_, "uuid_unparse");
    if (uuidUnparseFunc_ == nullptr) {
        IsEnable_ = false;
        dlclose(uuidLib_);
        return 1;
    }
    IsEnable_ = true;
    return 0;
}