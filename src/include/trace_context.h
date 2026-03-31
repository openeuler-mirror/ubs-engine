// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#ifndef UBSE_MANAGER_TRACE_CONTEXT_H
#define UBSE_MANAGER_TRACE_CONTEXT_H

#include <string>
#include <mutex>
#include "securec.h"

#ifdef __cplusplus
extern "C" {
#endif

extern thread_local char tls_traceId[37];
extern thread_local size_t tls_traceIdLen;

#ifdef __cplusplus
}
#endif

const size_t TRACE_ID_SIZE = 37;

using uuid_t = unsigned char[16];
using UuidGenerateRandom = void (*)(uuid_t);
using UuidUnparse = void (*)(const uuid_t uu, char *out);

class TraceContext {
public:
    // 获取当前线程的traceId
    static inline std::string GetTraceId()
    {
        if (tls_traceIdLen == 0 && IsEnable_) {  // 使用缓存长度
            generateTraceId();
        }
        return std::string(tls_traceId);
    }

    static inline char *GetTraceIdPtr()
    {
        if (tls_traceIdLen == 0 && IsEnable_) {  // 使用缓存长度
            generateTraceId();
        }
        return tls_traceId;
    }

    static inline const char* GetTraceIdCStr()
    {
        if (tls_traceIdLen == 0 && IsEnable_) {
            generateTraceId();
        }
        return tls_traceId;
    }

    // 设置traceId
    static inline void SetTraceId(const std::string &traceId)
    {
        if (traceId.empty() || !IsEnable_) {
            tls_traceId[0] = '\0';
            tls_traceIdLen = 0;
            return;
        }
        size_t copyLen = std::min(traceId.size(), TRACE_ID_SIZE - 1);
        auto ret = memcpy_s(tls_traceId, TRACE_ID_SIZE, traceId.c_str(), copyLen);
        if (ret == EOK) {
            tls_traceId[copyLen] = '\0';
            tls_traceIdLen = copyLen;
        } else {
            tls_traceId[0] = '\0';
            tls_traceIdLen = 0;
        }
    }

    // 清空
    static inline void Clear()
    {
        tls_traceId[0] = '\0';
        tls_traceIdLen = 0;
    }

    // 工具函数
    static bool IsValidTraceId(const char* traceId);
    static uint32_t InitUuid();
    static uint32_t Cleanup();
    static void SetEnable(bool enable) { IsEnable_ = enable; }
    static bool IsEnabled() { return IsEnable_; }

private:
    static void generateTraceId();
    static bool loadUuidLibrary();

    static UuidGenerateRandom uuidGenerateRandomFunc_;
    static UuidUnparse uuidUnparseFunc_;
    static void *uuidLib_;
    static bool IsEnable_;
    static std::mutex initMutex_;  // 初始化锁
};

#endif