// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#ifndef UBSE_MANAGER_TRACE_CONTEXT_H
#define UBSE_MANAGER_TRACE_CONTEXT_H

#include <string>
#include <algorithm>
#include <mutex>
#include <chrono>
#include <thread>
#include "securec.h"

#ifdef __cplusplus
extern "C" {
#endif

// 线程局部存储的traceId（每个线程独立副本）
extern thread_local char tls_traceId[37];

#ifdef __cplusplus
}
#endif

const size_t TRACE_ID_SIZE = 37;

using uuid_t = unsigned char[16];
using UuidGenerateRandom = void (*)(uuid_t);
using UuidUnparse = void (*)(const uuid_t uu, char *out);

class TraceContext {
public:
    // 获取当前线程的traceId（不存在则生成）
    static inline std::string GetTraceId()
    {
        if (tls_traceId[0] == '\0') {
            if (!IsEnable_) {
                return "";
            }
            generateTraceId();
        }
        return std::string(tls_traceId);
    }

    static inline char *GetTraceIdPtr()
    {
        return tls_traceId;
    }

    // 手动设置traceId（用于接收外部传递的ID）
    static inline void SetTraceId(const std::string &traceId)
    {
        if (traceId.empty() || !IsEnable_) {
            return;
        }

        size_t copyLen = std::min(traceId.size(), TRACE_ID_SIZE - 1);

        errno_t ret = memcpy_s(tls_traceId, TRACE_ID_SIZE,
                               traceId.c_str(), copyLen);
        if (ret != EOK) {
            tls_traceId[0] = '\0';
            return;
        }

        tls_traceId[copyLen] = '\0';
    }

    // 清空当前线程的traceId（请求处理结束时清理）
    static inline void Clear()
    {
        tls_traceId[0] = '\0';
    }

    static uint32_t InitUuid();

private:
    // 生成UUID作为traceId
    static inline void generateTraceId()
    {
        // 优先使用 libuuid
        if (uuidGenerateRandomFunc_ != nullptr &&
            uuidUnparseFunc_ != nullptr) {

            uuid_t uuid;
            char buf[TRACE_ID_SIZE] = {0};

            uuidGenerateRandomFunc_(uuid);
            uuidUnparseFunc_(uuid, buf);

            buf[TRACE_ID_SIZE - 1] = '\0';

            errno_t ret = strcpy_s(tls_traceId, TRACE_ID_SIZE, buf);
            if (ret != EOK) {
                tls_traceId[0] = '\0';
            }
            return;
        }

        // fallback（避免完全不可用）
        auto now = std::chrono::high_resolution_clock::now()
                       .time_since_epoch()
                       .count();
        auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());

        std::string fallback =
            std::to_string(now) + std::to_string(tid);

        size_t copyLen = std::min(fallback.size(), TRACE_ID_SIZE - 1);

        errno_t ret = memcpy_s(tls_traceId, TRACE_ID_SIZE,
                               fallback.c_str(), copyLen);
        if (ret != EOK) {
            tls_traceId[0] = '\0';
            return;
        }

        tls_traceId[copyLen] = '\0';
    }

private:
    static UuidGenerateRandom uuidGenerateRandomFunc_;
    static UuidUnparse uuidUnparseFunc_;
    static void *uuidLib_;
    static bool IsEnable_;

    static std::once_flag initFlag_;
};

#endif // UBSE_MANAGER_TRACE_CONTEXT_H
