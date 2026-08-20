/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MEMPOOLING_OVER_COMMIT_PID_FAULT_ERROR_UTIL_H
#define MEMPOOLING_OVER_COMMIT_PID_FAULT_ERROR_UTIL_H

#include <sstream>
#include <string>
#include <vector>
#include "mp_error.h"

namespace mempooling {

/**
 * @brief PID粒度故障处理错误记录与透传工具
 *
 * 一轮故障处理内可能同时出现多个错误（多借入节点/多task/多borrowId各有不同失败原因）。
 * 处理原则:
 * 1. 每个失败点都记录一条带上下文的FaultErrorRecord（code+detail），全部明细经
 *    JoinFaultErrorRecords拼入带关键字"[PidFaultErr]"的日志，供grep一键检索;
 * 2. 出口只能向RAS返回一个码，透传按时间序取最早记录的一条（先发生的错误往往是
 *    后续级联失败的根因，如借用失败早于迁移失败）。
 */

// 统一日志关键字: grep "PidFaultErr" 可检索一轮处理的全部错误明细
constexpr const char* kPidFaultErrTag = "[OvercommitFaultErr]";

// 单条错误记录: code为mp_error.h定义的故障码，detail携带失败主体与原因上下文
struct FaultErrorRecord {
    MpResult code = MEM_POOLING_OK;
    std::string detail;
};

// 透传规则: 取时间上最早记录的一条错误码; 空列表返回MEM_POOLING_OK
static inline MpResult EarliestFaultErrorCode(const std::vector<FaultErrorRecord>& records) noexcept
{
    return records.empty() ? MEM_POOLING_OK : records.front().code;
}

// 判断是否为已定义的故障特殊码（priority==0表示未归类，调用方需归一到故障码再记录）
static inline bool IsDefinedFaultErrorCode(MpResult code) noexcept
{
    switch (code) {
        case MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR:
        case MEM_POOLING_FAULT_IPC_ERROR:
        case MEM_POOLING_FAULT_LACK_REMOTE_MEM_ERROR:
        case MEM_POOLING_FAULT_BORROW_MEM_ERROR:
        case MEM_POOLING_FAULT_MIGRATE_ERROR:
        case MEM_POOLING_FAULT_RETURN_MEM_ERROR:
            return true;
        default:
            return false;
    }
}

// 把错误记录列表拼成"[PidFaultErr] total=2 | {code=5 ...} | {code=1 ...}"形式，
// 一行日志看清本轮全部错误明细（含关键字与总数，供检索与快速定位）
static inline std::string JoinFaultErrorRecords(const std::vector<FaultErrorRecord>& records)
{
    std::ostringstream oss;
    oss << kPidFaultErrTag << " total=" << records.size();
    for (const auto& rec : records) {
        oss << " | {code=" << rec.code << " " << rec.detail << "}";
    }
    return oss.str();
}

} // namespace mempooling

#endif // MEMPOOLING_OVER_COMMIT_PID_FAULT_ERROR_UTIL_H
