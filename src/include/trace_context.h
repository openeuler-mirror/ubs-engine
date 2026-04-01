// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#ifndef UBSE_MANAGER_TRACE_CONTEXT_H
#define UBSE_MANAGER_TRACE_CONTEXT_H

#include <string>
#include <atomic>
#include <mutex>
#include <random>
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

constexpr size_t TRACE_ID_SIZE = 37;

using uuid_t = unsigned char[16];
using UuidGenerateRandom = void (*)(uuid_t);
using UuidUnparse = void (*)(const uuid_t uu, char *out);

class TraceContext {
public:
    // 获取当前线程的traceId（不存在则生成）
    static inline std::string GetTraceId()
    {
        ensureInitialized();
        if (!IsEnable_.load(std::memory_order_acquire)) {
            return "";
        }

        if (tls_traceId[0] == '\0') {  // 优化：直接用字符判断，避免strlen
            generateTraceId();
        }
        return std::string(tls_traceId);
    }

    static inline char* GetTraceIdPtr()
    {
        ensureInitialized();
        if (!IsEnable_.load(std::memory_order_acquire)) {
            return tls_traceId;
        }

        if (tls_traceId[0] == '\0') {
            generateTraceId();
        }
        return tls_traceId;
    }

    // 手动设置traceId（用于接收外部传递的ID）
    static inline void SetTraceId(const std::string &traceId)
    {
        ensureInitialized();
        if (!IsEnable_.load(std::memory_order_acquire)) {
            return;
        }

        if (traceId.empty()) {
            return;
        }

        // 修复：memcpy_s的目标缓冲区大小应该是整个缓冲区大小
        size_t copyLen = std::min(traceId.size(), TRACE_ID_SIZE - 1);
        auto ret = memcpy_s(tls_traceId, TRACE_ID_SIZE, traceId.c_str(), copyLen);
        if (ret == EOK) {
            tls_traceId[copyLen] = '\0';
        } else {
            tls_traceId[0] = '\0';
        }
    }

    // 清空当前线程的traceId（请求处理结束时清理）
    static inline void Clear()
    {
        tls_traceId[0] = '\0';
    }

    static uint32_t InitUuid();

private:
    // 确保初始化只执行一次（线程安全）
    static inline void ensureInitialized()
    {
        static std::once_flag initFlag;
        std::call_once(initFlag, []() {
            if (!IsInitialized_.load(std::memory_order_acquire)) {
                InitUuid();
                IsInitialized_.store(true, std::memory_order_release);
            }
        });
    }

    // 生成UUID作为traceId
    static inline void generateTraceId()
    {
        // 缓存函数指针，避免重复检查静态变量
        auto generateFunc = uuidGenerateRandomFunc_.load(std::memory_order_relaxed);
        auto unparseFunc = uuidUnparseFunc_.load(std::memory_order_relaxed);
        // 优先使用UUID库
        if (generateFunc && unparseFunc) {
            uuid_t uuid;
            generateFunc(uuid);
            unparseFunc(uuid, tls_traceId);
            tls_traceId[TRACE_ID_SIZE - 1] = '\0';  // 确保终止符
            return;
        }

        // Fallback: 使用备用方案生成唯一ID
        fallbackGenerateTraceId();
    }

    // 备用方案：当UUID库不可用时生成唯一ID
    static inline void fallbackGenerateTraceId()
    {
        // 使用线程局部随机数生成器，避免锁竞争
        thread_local std::mt19937_64 rng(std::random_device{}() ^
                                         std::hash<std::thread::id>{}(std::this_thread::get_id()));
        thread_local std::uniform_int_distribution<uint64_t> dist;
        // 组合时间戳、线程ID和随机数确保唯一性
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
        auto random = dist(rng);
        // 生成类似UUID格式的字符串（32位十六进制）
        uint64_t high = static_cast<uint64_t>(now) ^ tid;
        uint64_t low = random;
        errno_t ret = snprintf_s(tls_traceId,
                                 TRACE_ID_SIZE,           // destMax: 缓冲区总大小
                                 TRACE_ID_SIZE - 1,       // count: 最多写入的字符数（不包括终止符）
                                 "%016lx-%016lx",
                                 static_cast<unsigned long>(high),
                                 static_cast<unsigned long>(low));
        if (ret < 0) {
            // 格式化失败，设置为空字符串
            tls_traceId[0] = '\0';
        } else {
            // 确保字符串以空字符结尾
            tls_traceId[TRACE_ID_SIZE - 1] = '\0';
        }
    }

    static std::atomic<void*> uuidLib_;
    static std::atomic<UuidGenerateRandom> uuidGenerateRandomFunc_;
    static std::atomic<UuidUnparse> uuidUnparseFunc_;
    static std::atomic<bool> IsEnable_;
    static std::atomic<bool> IsInitialized_;
};

#endif // UBSE_MANAGER_TRACE_CONTEXT_H