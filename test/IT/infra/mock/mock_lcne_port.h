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

#ifndef MOCK_LCNE_PORT_H
#define MOCK_LCNE_PORT_H

#include <cstdint>
#include <map>
#include <set>

namespace ubse::it::infra::lcne_port {

/**
 * @brief 返回本节点 (slotId) 在指定集群规模下的「有效」连接映射表:
 *        ubpuId -> (portId -> remoteSlotId).
 *
 * 以场景连接表为基准, 已扣除本模块注入的 down 端口 (注入层优先, 被标记的端口强制为 down),
 * 表内端口为 up, 其余为 down; 两个 ubpu 均展开返回完整数据.
 * lcne_xml 直接据此生成 XML, 无需感知 downPorts.
 */
std::map<int, std::map<int, uint32_t>> SelectUpPortMap(uint32_t slotId, std::size_t clusterSize);

/**
 * @brief 按链路注入断链: 若 slotA 与 slotB 在场景连接表中存在链路, 则将两端实际相连端口
 *        (对 ubpuIds 指定的 ubpu) 置为 down; 若两者本身无连接, 则为 no-op.
 * 特殊值: slotB == UINT32_MAX 表示对端不指定, 此时断掉 slotA 上 (ubpuIds 指定 ubpu) 的portIds端口.
 * 端口判定(链路是否存在、对端端口号)全部在本模块内部完成, 调用方无需关心具体端口号.
 */
void MarkLinkDown(uint32_t slotA, uint32_t slotB, const std::set<int>& ubpuIds, const std::set<int>& portIds,
                  std::size_t clusterSize);

/** @brief 清空指定 slot 注入的 down 端口, 恢复默认(不注入). */
void ClearLinkDowns(uint32_t slotId);

} // namespace ubse::it::infra::lcne_port

#endif // MOCK_LCNE_PORT_H
