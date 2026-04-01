// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#ifndef UBSE_MANAGER_TRACE_CONTEXT_H
#define UBSE_MANAGER_TRACE_CONTEXT_H

#include <string>
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
        if (strlen(tls_traceId) == 0) { // 检查是否为空
            if (!IsEnable_) {
                return "";
            }
            generateTraceId();          // 首次调用时生成
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
        // 确保不超过缓冲区大小，截断过长的ID
        size_t copyLen = std::min(traceId.size(), TRACE_ID_SIZE - 1);
        auto ret = memcpy_s(tls_traceId, copyLen, traceId.c_str(), traceId.size());
        if (ret != EOK) {
            return;
        }
        tls_traceId[copyLen] = '\0'; // 确保字符串终止
    }

    // 清空当前线程的traceId（请求处理结束时清理）
    static inline void Clear()
    {
        tls_traceId[0] = '\0';


    }
    static uint32_t InitUuid();

private:
    // 生成UUID作为traceId（例如：a1b2c3d4-5678-90ef-ghij-klmnopqrstuv）
    static inline void generateTraceId()
    {
        uuid_t uuid;
        if (uuidGenerateRandomFunc_ == nullptr) {
            tls_traceId[0] = '\0';
            return;
        }
        uuidGenerateRandomFunc_(uuid);
        char buf[TRACE_ID_SIZE]; // UUID字符串长度固定36，加终止符
        if (uuidUnparseFunc_ == nullptr) {
            tls_traceId[0] = '\0';
            return;
        }
        uuidUnparseFunc_(uuid, buf);
        buf[TRACE_ID_SIZE - 1] = '\0';
        auto ret = strcpy_s(tls_traceId, TRACE_ID_SIZE, buf);
        if (ret != EOK) {
            tls_traceId[0] = '\0';
            return;
        }
    }
    static UuidGenerateRandom uuidGenerateRandomFunc_;
    static UuidUnparse uuidUnparseFunc_;
    static void *uuidLib_;
    static bool IsEnable_;
};
#endif // UBSE_MANAGER_TRACE_CONTEXT_H