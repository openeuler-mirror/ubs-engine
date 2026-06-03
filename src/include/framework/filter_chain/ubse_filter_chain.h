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
#ifndef UBSE_FILTER_CHAIN_H
#define UBSE_FILTER_CHAIN_H

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <vector>

namespace ubse::filter_chain {

template <typename Ctx>
// 过滤器节点
class UbseFilter {
public:
    virtual ~UbseFilter() = default;
    // 返回 true 继续链，返回 false 终止链
    virtual bool Handle(Ctx &context) = 0;
};

template <typename Ctx>
class UbseFilterChain {
public:
    UbseFilterChain() = default;
    ~UbseFilterChain() = default;

    UbseFilterChain(const UbseFilterChain &) = delete;
    UbseFilterChain &operator=(const UbseFilterChain &) = delete;

    void AddFilter(uint32_t groupId, std::shared_ptr<UbseFilter<Ctx>> filter)
    {
        if (!filter) {
            return;
        }
        std::unique_lock<std::shared_mutex> lock(rwLock_);
        filterMap_[groupId].push_back(std::move(filter));
    }

    // 返回 true 表示所有 Filter 均返回 true（链完整执行），false 表示被某个 Filter 短路
    bool Execute(Ctx &context)
    {
        std::shared_lock<std::shared_mutex> lock(rwLock_);
        // 按照groupId升序执行
        for (const auto &group : filterMap_) {
            // 组内按照vector添加顺序执行
            for (const auto &filter : group.second) {
                if (!filter->Handle(context)) {
                    return false;
                }
            }
        }
        return true;
    }

    void Clear()
    {
        std::unique_lock<std::shared_mutex> lock(rwLock_);
        filterMap_.clear();
    }

private:
    std::map<uint32_t, std::vector<std::shared_ptr<UbseFilter<Ctx>>>> filterMap_;
    mutable std::shared_mutex rwLock_;
};

} // namespace ubse::filter_chain

#endif // UBSE_FILTER_CHAIN_H
