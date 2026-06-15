/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_MEM_UTILS_H
#define UBSE_MEM_UTILS_H

#include <optional>
#include <vector>
#include "ubse_api_server_def.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_controller.h"
#include "ubse_thread_pool_module.h"

namespace ubse::mem::util {
using ubse::adapter_plugins::mmi::NodeMemDebtInfo;
using ubse::adapter_plugins::mmi::NodeMemDebtInfoMap;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseUdsInfo;
using ubse::common::def::UbseResult;
using ubse::task_executor::UbseTaskExecutorPtr;

std::string GetCurNodeId();
UbseTaskExecutorPtr GetExecutor(const std::string& name);
void FilterFdAndNumaObjs(NodeMemDebtInfoMap& filterData, const NodeMemDebtInfoMap& data);
void FilterAddrAndShareObjs(NodeMemDebtInfoMap& filterData, const NodeMemDebtInfoMap& data);

bool CheckName(const std::string& name);

bool isNumericString(const std::string& str);

template <typename Obj>
UbseResult MemoryBorrowRpcObjCheck(const Obj& obj)
{
    if (!CheckName(obj.req.name)) {
        return UBSE_ERROR_INVAL;
    }
    if (!isNumericString(obj.req.requestNodeId) && !isNumericString(obj.req.importNodeId)) {
        return UBSE_ERROR_INVAL;
    }
    if (obj.algoResult.exportNumaInfos.empty() || obj.algoResult.importNumaInfos.empty()) {
        return UBSE_ERROR_INVAL;
    }
    if (obj.algoResult.exportNumaInfos.size() != obj.algoResult.importNumaInfos.size()) {
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

template <typename Obj>
UbseResult ShareBorrowRpcObjCheck(const Obj& obj)
{
    if (!CheckName(obj.req.name)) {
        return UBSE_ERROR_INVAL;
    }
    if (!isNumericString(obj.req.requestNodeId)) {
        return UBSE_ERROR_INVAL;
    }
    if constexpr (std::is_same_v<Obj, UbseMemShareBorrowExportObj>) {
        if (obj.algoResult.exportNumaInfos.empty()) {
            return UBSE_ERROR_INVAL;
        }
    }
    return UBSE_OK;
}

template <typename T>
bool SafeAdd(T a, T b, T& result)
{
    const auto ret = __builtin_add_overflow(a, b, &result);
    return ret;
}

template <typename T>
bool SafeSub(T a, T b, T& result)
{
    const auto ret = __builtin_sub_overflow(a, b, &result);
    return ret;
}

template <typename Key = std::string, typename Value = NodeMemDebtInfo>
class SafeMap {
public:
    explicit SafeMap() = default;

    // 插入或更新（写操作）
    void insert_or_assign(const Key& key, const Value& value)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        // 检查是否已处于 moved-from 状态（防止 extract 后误用）
        if (map_.empty() && map_.bucket_count() == 0) {
            return;
        }
        map_[key] = value;
    }

    void insert_or_assign(Key&& key, Value&& value)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        map_.emplace(std::move(key), std::move(value));
    }

    // 查找（读操作，支持并发读）
    std::optional<Value> get(const Key& key) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    // 判断是否存在
    bool contains(const Key& key) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return map_.find(key) != map_.end();
    }

    // 删除
    bool erase(const Key& key)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        return map_.erase(key) > 0;
    }

    // 清空
    void clear()
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        map_.clear();
    }

    // 获取当前大小
    size_t size() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return map_.size();
    }

    std::unordered_map<Key, Value> to_unordered_map() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return map_;
    }

    std::unordered_map<Key, Value> extract_unordered_map()
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto result = std::move(map_);
        map_.clear(); // 可选：显式清空
        return result;
    }

    template <typename Func>
    void with_write_lock(Func&& func)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        func(map_);
    }

    template <typename Func>
    void with_read_lock(Func&& func) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        func(map_);
    }

private:
    mutable std::shared_mutex mutex_; // 读写锁：允许多读、独占写
    std::unordered_map<Key, Value> map_;
};

UbseUdsInfo GenUdsInfo(const ::api::server::UbseRequestContext& context);
} // namespace ubse::mem::util
#endif // UBSE_MEM_UTILS_H
