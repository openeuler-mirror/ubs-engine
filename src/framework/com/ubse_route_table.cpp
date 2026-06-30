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

#include "ubse_route_table.h"

#include <algorithm>
#include <bit>
#include <climits>
#include <cstdint>
#include <cstdlib>

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_str_util.h"

namespace ubse::com {
using namespace ubse::log;
#define MODULE_LOG_NAME "ubse"

/**
 * @brief 模糊匹配的分组ID计算
 *        group = (nodeIdNum - 1) >> __builtin_ctz(capacity)
 * @param nodeIdNum 节点ID的数值表示
 * @param capacity 路由表项的capacity值（>0，必须为2的幂）
 * @return 计算得到的分组ID
 */
uint32_t ComputeGroup(uint32_t nodeIdNum, uint32_t capacity)
{
    uint32_t shift = __builtin_ctz(capacity);
    return (nodeIdNum - 1) >> shift;
}

std::string UbseRouteTable::Lookup(const std::string &dstNodeId) const
{
    ubse::utils::ReadLocker<ubse::utils::ReadWriteLock> locker(&lock_);
    for (const auto &entry : entries_) {
        if (entry.capacity == 0) {
            // 精确匹配路由项，直接返回nextHopNodeId
            if (entry.dstNodeId == dstNodeId) {
                return entry.nextHopNodeId;
            }
        } else if (entry.dstNodeId != "0") {
            // 模糊匹配路由项，根据分组ID判断是否匹配
            uint32_t dstNodeIdNum;
            if (utils::ConvertStrToUint32(dstNodeId, dstNodeIdNum) != UBSE_OK) {
                UBSE_LOG_WARN << "Failed to convert dstNodeId=" << dstNodeId << " to uint32_t";
                return "";
            }
            uint32_t entryNodeIdNum;
            if (utils::ConvertStrToUint32(entry.dstNodeId, entryNodeIdNum) != UBSE_OK) {
                UBSE_LOG_WARN << "Failed to convert dstNodeId=" << entry.dstNodeId << " to uint32_t";
                continue;
            }
            uint32_t targetGroup = ComputeGroup(dstNodeIdNum, entry.capacity);
            uint32_t entryGroup = ComputeGroup(entryNodeIdNum, entry.capacity);
            if (targetGroup == entryGroup) {
                return entry.nextHopNodeId;
            }
        } else {
            // 默认路由，匹配任意目的地
            if (entry.capacity == DEFAULT_ROUTE_MAX_CAPACITY) {
                return entry.nextHopNodeId;
            }
        }
    }
    UBSE_LOG_WARN << "No route found for dstNodeId=" << dstNodeId;
    return "";
}

UbseResult UbseRouteTable::AddRoute(const RouteEntry &entry)
{
    // 对外接口：priority必须>=1（0预留给直连链路）
    if (entry.priority == 0) {
        UBSE_LOG_ERROR << "Cannot add route with priority 0 via external API, dstNodeId=" << entry.dstNodeId;
        return UBSE_COM_ERROR_ROUTE_INVAL;
    }

    // 容量校验：精确匹配为0，模糊匹配必须为2的幂，默认路由最大容量为DEFAULT_ROUTE_MAX_CAPACITY
    if (entry.capacity != 0 && entry.capacity != DEFAULT_ROUTE_MAX_CAPACITY &&
            ((entry.capacity & (entry.capacity - 1)) != 0) ||
        (entry.dstNodeId == "0" && entry.capacity != DEFAULT_ROUTE_MAX_CAPACITY) || entry.dstNodeId.empty()) {
        UBSE_LOG_ERROR << "Fuzzy-match capacity must be 0 or a power of 2, got " << entry.capacity
                       << " for dstNodeId=" << entry.dstNodeId
                       << ", capacity must be DEFAULT_ROUTE_MAX_CAPACITY(4294967295), "
                       << "or capacity must be 0 for dstNodeId=0";
        return UBSE_COM_ERROR_ROUTE_INVAL;
    }
    ubse::utils::WriteLocker<ubse::utils::ReadWriteLock> locker(&lock_);
    for (const auto &existing : entries_) {
        if (existing.dstNodeId == entry.dstNodeId && existing.priority == entry.priority) {
            if (existing.nextHopNodeId == entry.nextHopNodeId && existing.capacity == entry.capacity) {
                return UBSE_OK;
            }
            UBSE_LOG_ERROR << "Route entry with same key (dstNodeId=" << entry.dstNodeId
                           << ", priority=" << static_cast<int>(entry.priority)
                           << ") already exists with different value";
            return UBSE_COM_ERROR_ROUTE_EXISTED;
        }
    }
    UBSE_LOG_INFO << "Add route entry, dstNodeId=" << entry.dstNodeId
                  << ", priority=" << static_cast<int>(entry.priority) << ", nextHopNodeId=" << entry.nextHopNodeId
                  << ", capacity=" << entry.capacity;
    entries_.push_back(entry);
    std::sort(entries_.begin(), entries_.end(),
              [](const RouteEntry &a, const RouteEntry &b) { return a.priority < b.priority; });

    return UBSE_OK;
}

void UbseRouteTable::DelRoute(const std::string &nodeId)
{
    ubse::utils::WriteLocker<ubse::utils::ReadWriteLock> locker(&lock_);
    UBSE_LOG_INFO << "Delete route entry, dstNodeId=" << nodeId;
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [&nodeId](const RouteEntry &entry) {
                                      if (entry.priority == 0) {
                                          return false;
                                      }
                                      return entry.dstNodeId == nodeId;
                                  }),
                   entries_.end());
}

void UbseRouteTable::AddDirectRoute(const std::string &nodeId)
{
    ubse::utils::WriteLocker<ubse::utils::ReadWriteLock> locker(&lock_);
    UBSE_LOG_INFO << "Add direct route entry, dstNodeId=" << nodeId;
    for (const auto &entry : entries_) {
        if (entry.priority == 0 && entry.dstNodeId == nodeId) {
            return;
        }
    }

    RouteEntry entry;
    entry.dstNodeId = nodeId;
    entry.capacity = 0;
    entry.priority = 0;
    entry.nextHopNodeId = nodeId;
    entries_.push_back(entry);
    std::sort(entries_.begin(), entries_.end(),
              [](const RouteEntry &a, const RouteEntry &b) { return a.priority < b.priority; });
}

void UbseRouteTable::DelDirectRoute(const std::string &nodeId)
{
    ubse::utils::WriteLocker<ubse::utils::ReadWriteLock> locker(&lock_);
    UBSE_LOG_INFO << "Delete direct route entry, dstNodeId=" << nodeId;
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
                       [&nodeId](const RouteEntry &entry) { return entry.priority == 0 && entry.dstNodeId == nodeId; }),
        entries_.end());
}

} // namespace ubse::com
