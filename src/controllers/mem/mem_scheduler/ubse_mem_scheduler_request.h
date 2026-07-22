/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBS_ENGINE_UBSE_MEM_SCHEDULER_REQUEST_H
#define UBS_ENGINE_UBSE_MEM_SCHEDULER_REQUEST_H

#include <any>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ubse_mem_types.h"
#include "ubse_mmi_def.h"
#include "adapter_plugins/mmi/ubse_mmi_def.h"
#include "scheduler_score/ubse_mem_scheduler_score_weight.h"

namespace ubse::mem::scheduler {

enum class RequestMode
{
    BORROW,
    SHARE,
};

class SchedulerRequest {
public:
    std::string name_{};
    RequestMode requestMode_{RequestMode::BORROW};
    NodeId requestNodeId_{};
    NodeId importNodeId_{};
    uint64_t requestSize_{0};
    std::vector<NodeId> provideNodes_;
    bool checkMode_{false}; // true: 单节点校验模式 (Filter 链输出空列表即失败)
    std::unordered_map<std::string, std::any> params_;
    std::vector<std::string> filterNames_; // 本请求需要执行的过滤器名称列表 (按顺序)
    std::vector<std::string> scoreNames_;  // 本请求需要执行的评分器名称列表 (按顺序)
    ScoreWeights weights_{};               // 评分权重配置

    template <typename T>
    std::optional<T> GetParamOpt(const std::string& key) const
    {
        auto it = params_.find(key);
        if (it == params_.end()) {
            return std::nullopt;
        }
        if (it->second.type() != typeid(T)) {
            return std::nullopt;
        }
        return std::any_cast<T>(it->second);
    }

    template <typename T>
    void SetParam(const std::string& key, T value)
    {
        params_[key] = value;
    }

    static SchedulerRequest FromFdBorrowReq(const adapter_plugins::mmi::UbseMemFdBorrowReq& req);
    static SchedulerRequest FromNumaBorrowReq(const adapter_plugins::mmi::UbseMemNumaBorrowReq& req);
    static SchedulerRequest FromShareBorrowReq(const adapter_plugins::mmi::UbseMemShareBorrowReq& req);
    static SchedulerRequest FromAddrBorrowReq(const adapter_plugins::mmi::UbseMemAddrBorrowReq& req);
};

// Convert MemObj → SchedulerRequest (for template MemoryBorrowHandler)
template <typename MemObj>
SchedulerRequest ConvertMemObjToRequest(MemObj& mem_obj);

template <>
inline SchedulerRequest ConvertMemObjToRequest(adapter_plugins::mmi::UbseMemFdBorrowImportObj& obj)
{
    return SchedulerRequest::FromFdBorrowReq(obj.req);
}
template <>
inline SchedulerRequest ConvertMemObjToRequest(adapter_plugins::mmi::UbseMemFdBorrowExportObj& obj)
{
    return SchedulerRequest::FromFdBorrowReq(obj.req);
}

template <>
inline SchedulerRequest ConvertMemObjToRequest(adapter_plugins::mmi::UbseMemNumaBorrowImportObj& obj)
{
    return SchedulerRequest::FromNumaBorrowReq(obj.req);
}
template <>
inline SchedulerRequest ConvertMemObjToRequest(adapter_plugins::mmi::UbseMemNumaBorrowExportObj& obj)
{
    return SchedulerRequest::FromNumaBorrowReq(obj.req);
}

template <>
inline SchedulerRequest ConvertMemObjToRequest(adapter_plugins::mmi::UbseMemAddrBorrowImportObj& obj)
{
    return SchedulerRequest::FromAddrBorrowReq(obj.req);
}

template <>
inline SchedulerRequest ConvertMemObjToRequest(adapter_plugins::mmi::UbseMemAddrBorrowExportObj& obj)
{
    return SchedulerRequest::FromAddrBorrowReq(obj.req);
}

template <>
inline SchedulerRequest ConvertMemObjToRequest(adapter_plugins::mmi::UbseMemShareBorrowImportObj& obj)
{
    return SchedulerRequest::FromShareBorrowReq(obj.req);
}
template <>
inline SchedulerRequest ConvertMemObjToRequest(adapter_plugins::mmi::UbseMemShareBorrowExportObj& obj)
{
    return SchedulerRequest::FromShareBorrowReq(obj.req);
}

} // namespace ubse::mem::scheduler

#endif //UBS_ENGINE_UBSE_MEM_SCHEDULER_REQUEST_H
