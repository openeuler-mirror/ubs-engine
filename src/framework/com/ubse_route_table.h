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

#ifndef UBSE_ROUTE_TABLE_H
#define UBSE_ROUTE_TABLE_H

#include <cstdint>
#include <string>
#include <vector>

#include "lock/ubse_lock.h"
#include "ubse_common_def.h"

namespace ubse::com {
using namespace ubse::common::def;

constexpr uint32_t DEFAULT_ROUTE_MAX_CAPACITY = UINT32_MAX;
struct RouteEntry {
    std::string dstNodeId;
    uint32_t capacity = 0;  // 0为精确匹配，>0为位掩码分组匹配（必须为2的幂）
    uint8_t priority = 127; // 0~127，0最高（为直连链路保留）
    std::string nextHopNodeId;
};

class UbseRouteTable {
public:
    UbseRouteTable() = default;
    ~UbseRouteTable() = default;

    /**
     * @brief 查找目的节点的下一跳
     * @details 按 priority 升序遍历路由表，命中第一条匹配项即返回。
     *         1. 精确路由：capacity=0, dstNodeId=目标节点ID
     *            Lookup 仅命中完全相同的 dstNodeId，适合点对点转发。
     *         2. 分组路由：capacity=2^n（n≥1）, dstNodeId=组内任一节点ID
     *            Lookup 时将 (dstNodeId-1) 的低 N 位置 0 得到分组 ID（N=log₂(capacity)），
     *            分组 ID 相同的节点共享该路由。
     *         3. 默认路由：dstNodeId="0", capacity=UINT32_MAX
     * @param[in] dstNodeId 目标节点 ID
     * @return 下一跳节点 ID，无路由则返回空字符串
     */
    std::string Lookup(const std::string &dstNodeId) const;

    /**
     * @brief 添加路由表项，用于多跳转发路径配置
     * @details 以 (dstNodeId, priority) 为键；键相同值相同时幂等，
     *         键相同值不同或表项无效时拒绝。支持三种路由类型：
     *         1. 精确路由：capacity=0, dstNodeId=目标节点ID
     *            Lookup 仅命中完全相同的 dstNodeId，适合点对点转发。
     *         2. 分组路由：capacity=2^n（n≥1）, dstNodeId=组内任一节点ID
     *            Lookup 时将 (dstNodeId-1) 低 n 位置 0 得到分组 ID，
     *            同一分组内所有节点共享该路由。适合机柜级转发（例如 capacity=8 覆盖节点 1~8）。
     *         3. 默认路由：dstNodeId="0", capacity=UINT32_MAX
     *            Lookup 未命中其他路由时的兜底，匹配任意目的地。
     *         约束：
     *         - priority=0 预留给直连路由，对外 API 只接受 priority≥1。
     *         - capacity 必须为 0 或 2 的幂。
     *
     * @param[in] entry 路由表项
     * @return UBSE_OK                      — 添加成功，或键相同值相同（幂等）
     *         UBSE_COM_ERROR_ROUTE_EXISTED — 键相同但值不同
     *         UBSE_COM_ERROR_ROUTE_INVAL  — 表项无效（dstNodeId为空、priority超范围、capacity非2的幂等）
     */
    UbseResult AddRoute(const RouteEntry &entry);

    /**
     * @brief 删除目的节点的路由表项
     * @details 可删除精确路由、分组路由和默认路由（nodeId="0"）。
     *         不会删除 priority=0 的直连路由（链路断开时由通信框架自动清理）。
     * @param[in] nodeId 目的节点 ID
     */
    void DelRoute(const std::string &nodeId);

    /**
     * @brief 添加直连路由（内部接口，priority=0）
     *        链路建立时自动调用
     * @param nodeId 直连节点ID
     */
    void AddDirectRoute(const std::string &nodeId);

    /**
     * @brief 删除直连路由（内部接口，priority=0）
     *        链路断开时自动调用
     * @param nodeId 直连节点ID
     */
    void DelDirectRoute(const std::string &nodeId);

private:
    std::vector<RouteEntry> entries_;
    mutable ubse::utils::ReadWriteLock lock_;
};

} // namespace ubse::com

#endif // UBSE_ROUTE_TABLE_H
