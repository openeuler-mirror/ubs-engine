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

#include "mock_lcne_port.h"

#include <mutex>
#include <set>
#include <tuple>
#include <vector>

namespace ubse::it::infra::lcne_port {

namespace {

// 端口三元组 (slotId, ubpuId, portId): 端口相关类型与拓扑连接表的唯一归属, 仅供本模块内部使用.
using LcnePortKey = std::tuple<uint32_t, int, int>;
// 被置为 down 的端口集合: 可变状态由本模块统一持有/管理.
using DownPorts = std::set<LcnePortKey>;

// 端口可变状态: 由本模块统一持有, 通过 Meyer's Singleton 管理, 避免静态初始化顺序问题.
struct PortState {
    DownPorts downPorts;
    mutable std::mutex mutex;
};

PortState& GetPortState()
{
    static PortState state;
    return state;
}

// 不同集群规模下的连接映射表: slotId -> (portId -> remoteSlotId)
// 每节点2个ubpu, 各9个physical-port; 表内的端口为 up, 其余端口为 down
// 单节点场景 & 两节点场景: 端口状态: port 0/2/3/4/5/6/7/8=down, port 1=up
//   slot1: port1->2
//   slot2: port1->1
const std::map<uint32_t, std::map<int, uint32_t>> UP_PORT_MAP_2_NODES = {{1, {{1, 2}}}, {2, {{1, 1}}}};
// 四节点场景: 端口状态: port 0/2/3/6/7/8=down, port 1/4/5=up
//   slot1: port1->2, port4->3, port5->4
//   slot2: port1->1, port4->4, port5->3
//   slot3: port1->4, port4->1, port5->2
//   slot4: port1->3, port4->2, port5->1
const std::map<uint32_t, std::map<int, uint32_t>> UP_PORT_MAP_4_NODES = {{1, {{1, 2}, {4, 3}, {5, 4}}},
                                                                         {2, {{1, 1}, {4, 4}, {5, 3}}},
                                                                         {3, {{1, 4}, {4, 1}, {5, 2}}},
                                                                         {4, {{1, 3}, {4, 2}, {5, 1}}}};
// 八节点场景: 端口状态: port 0/8=down, port 1/2/3/4/5/6/7=up
//   slot1: port1->2, port2->8, port3->7, port4->3, port5->4, port6->6, port7->5
//   slot2: port1->1, port2->5, port3->8, port4->4, port5->3, port6->7, port7->6
//   slot3: port1->4, port2->6, port3->5, port4->1, port5->2, port6->8, port7->7
//   slot4: port1->3, port2->7, port3->6, port4->2, port5->1, port6->5, port7->4
//   slot5: port1->6, port2->2, port3->3, port4->7, port5->8, port6->4, port7->1
//   slot6: port1->5, port2->3, port3->4, port4->8, port5->7, port6->1, port7->2
//   slot7: port1->8, port2->4, port3->1, port4->5, port5->6, port6->2, port7->3
//   slot8: port1->7, port2->1, port3->2, port4->6, port5->5, port6->3, port7->4
const std::map<uint32_t, std::map<int, uint32_t>> UP_PORT_MAP_8_NODES = {
    {1, {{1, 2}, {2, 8}, {3, 7}, {4, 3}, {5, 4}, {6, 6}, {7, 5}}},
    {2, {{1, 1}, {2, 5}, {3, 8}, {4, 4}, {5, 3}, {6, 7}, {7, 6}}},
    {3, {{1, 4}, {2, 6}, {3, 5}, {4, 1}, {5, 2}, {6, 8}, {7, 7}}},
    {4, {{1, 3}, {2, 7}, {3, 6}, {4, 2}, {5, 1}, {6, 5}, {7, 4}}},
    {5, {{1, 6}, {2, 2}, {3, 3}, {4, 7}, {5, 8}, {6, 4}, {7, 1}}},
    {6, {{1, 5}, {2, 3}, {3, 4}, {4, 8}, {5, 7}, {6, 1}, {7, 2}}},
    {7, {{1, 8}, {2, 4}, {3, 1}, {4, 5}, {5, 6}, {6, 2}, {7, 3}}},
    {8, {{1, 7}, {2, 1}, {3, 2}, {4, 6}, {5, 5}, {6, 3}, {7, 4}}}};

} // namespace

// 场景连接表(静态、不随注入变化): 选择指定集群规模下的原始连接表.
static const std::map<uint32_t, std::map<int, uint32_t>>& SelectBaseUpPortMap(std::size_t clusterSize)
{
    switch (clusterSize) {
        case 1:
        case 2:
            return UP_PORT_MAP_2_NODES;
        case 8:
            return UP_PORT_MAP_8_NODES;
        case 4:
        default:
            return UP_PORT_MAP_4_NODES;
    }
}

std::map<int, std::map<int, uint32_t>> SelectUpPortMap(uint32_t slotId, std::size_t clusterSize)
{
    const auto& base = SelectBaseUpPortMap(clusterSize);
    std::map<int, std::map<int, uint32_t>> result;
    auto slotIt = base.find(slotId);
    if (slotIt == base.end()) {
        return result; // 场景连接表中无本节点, 返回空表
    }
    auto& state = GetPortState();
    std::lock_guard<std::mutex> lock(state.mutex);
    for (int ubpu : {1, 2}) {
        auto& ubpuPorts = result[ubpu];
        for (const auto& [portId, remoteSlot] : slotIt->second) {
            if (!state.downPorts.count(LcnePortKey(slotId, ubpu, portId))) {
                ubpuPorts[portId] = remoteSlot;
            }
        }
    }
    return result;
}

static void MarkPortDown(uint32_t srcSlot, uint32_t dstSlot, const std::set<int>& ubpuIds, const std::set<int>& portIds,
                         std::size_t clusterSize)
{
    const auto& upPortMap = SelectBaseUpPortMap(clusterSize);
    auto slotIt = upPortMap.find(srcSlot);
    if (slotIt == upPortMap.end()) {
        return; // 场景连接表中无 srcSlot, no-op
    }

    // 查找 srcSlot 上需要断开的端口: 对端不指定 (UINT32_MAX) 时断开指定端口, 否则只断开指向 dstSlot 的端口
    std::vector<int> linkPorts;
    if (dstSlot == UINT32_MAX) {
        for (const auto& portEntry : slotIt->second) {
            if (portIds.find(portEntry.first) != portIds.end()) {
                linkPorts.push_back(portEntry.first);
            }
        }
    } else {
        for (const auto& [portId, remoteSlot] : slotIt->second) {
            if (remoteSlot == dstSlot) {
                linkPorts.push_back(portId);
            }
        }
    }
    if (linkPorts.empty()) {
        return; // 无端口可断, no-op
    }

    // 对指定 ubpu, 将本侧实际相连端口的 down 三元组写入本模块持有的端口状态
    auto& state = GetPortState();
    std::lock_guard<std::mutex> lock(state.mutex);
    for (int ubpu : ubpuIds) {
        for (int portId : linkPorts) {
            state.downPorts.emplace(srcSlot, ubpu, portId);
        }
    }
}

void MarkLinkDown(uint32_t slotA, uint32_t slotB, const std::set<int>& ubpuIds, const std::set<int>& portIds,
                  std::size_t clusterSize)
{
    MarkPortDown(slotA, slotB, ubpuIds, portIds, clusterSize);
    if (slotB != UINT32_MAX) {
        MarkPortDown(slotB, slotA, ubpuIds, portIds, clusterSize);
    }
}

void ClearLinkDowns(uint32_t slotId)
{
    auto& state = GetPortState();
    std::lock_guard<std::mutex> lock(state.mutex);
    for (auto it = state.downPorts.begin(); it != state.downPorts.end();) {
        if (std::get<0>(*it) == slotId) {
            it = state.downPorts.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace ubse::it::infra::lcne_port
