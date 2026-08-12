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

#include <set>
#include <sstream>
#include <string>
#include "mp_error.h"

namespace mempooling {

/**
 * @brief PID粒度故障处理错误码聚合工具
 *
 * 一轮故障处理内可能同时出现多类失败，出口只能向RAS返回一个码。
 * 按运维排障优先级聚合: 数据面 > 通信面 > 资源面 > 执行面
 * （详见 docs/RMRS超分PID故障处理错误码对接方案.md §5）
 */

// 错误码排障优先级: 数值越大越优先上报
static inline int GetFaultErrorCodePriority(MpResult code) noexcept
{
    switch (code) {
        case MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR: // 数据面不可信，后续动作无意义，最先报
            return 6;
        case MEM_POOLING_FAULT_IPC_ERROR: // 通信面异常，影响所有后续动作
            return 5;
        case MEM_POOLING_FAULT_LACK_REMOTE_MEM_ERROR: // 腾内存动作
            return 4;
        case MEM_POOLING_FAULT_BORROW_MEM_ERROR: // 借用执行异常
            return 3;
        case MEM_POOLING_FAULT_MIGRATE_ERROR: // 查ubturbo
            return 2;
        case MEM_POOLING_FAULT_RETURN_MEM_ERROR: // 查ubse状态
            return 1;
        default:
            return 0;
    }
}

// 从本轮出现的错误码集合中取优先级最高者; 空集合返回MEM_POOLING_OK
static inline MpResult AggregateFaultErrorCodes(const std::set<MpResult>& codes) noexcept
{
    MpResult aggregated = MEM_POOLING_OK;
    for (const auto& code : codes) {
        if (GetFaultErrorCodePriority(code) > GetFaultErrorCodePriority(aggregated)) {
            aggregated = code;
        }
    }
    return aggregated;
}

// 把错误码集合拼成"[ 5 6 ]"形式，日志里一行看清本轮出现的全部码
// 注: 不加noexcept——ostringstream可能抛bad_alloc，标记后异常会直接terminate
static inline std::string JoinFaultErrorCodes(const std::set<MpResult>& codes)
{
    std::ostringstream oss;
    oss << "[";
    for (const auto& code : codes) {
        oss << " " << code;
    }
    oss << " ]";
    return oss.str();
}

} // namespace mempooling

#endif // MEMPOOLING_OVER_COMMIT_PID_FAULT_ERROR_UTIL_H
