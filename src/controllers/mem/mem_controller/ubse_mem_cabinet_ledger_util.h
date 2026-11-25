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

#ifndef UBS_ENGINE_UBSE_MEM_CABINET_LEDGER_UTIL_H
#define UBS_ENGINE_UBSE_MEM_CABINET_LEDGER_UTIL_H

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "ubse_mmi_interface.h"
#include "ubse_node_controller.h"

namespace ubse::mem::controller {

using ubse::adapter_plugins::mmi::NodeMemDebtInfo;

 /**
 * @brief 查询节点所属逻辑机柜号（A 侧外柜归并、B 侧按柜过滤共用，设计文档 §4.1.1/§5.1）
 * @param nodeId [in] 节点Id
 * @param groupId [out] 逻辑机柜号
 * @return true 节点在拓扑内；false 节点不在任何机柜（调用方按 SKIPPED 处理，不得当空账本）
 */
bool GetCabinetGroupIdByNodeId(const std::string& nodeId, uint32_t& groupId);

 /**
 * @brief 枚举逻辑机柜的全部成员节点（B 侧 0x001A 应答必须逐节点产出条目，设计文档 §5.2）
 * @param groupId [in] 逻辑机柜号
 * @return 成员 nodeId 列表，按 nodeId 升序（保证回包顺序稳定）；柜号非法或拓扑缺失时为空
 */
std::vector<std::string> GetCabinetMemberNodeIds(uint32_t groupId);

 /**
 * @brief 提取一份节点账本中出现的全部对端 nodeId（口径唯一源，与既有 CollectRelatedNodeIds 同源）
 *        A 侧按柜归并、B 侧按柜过滤、A 侧 SKIPPED 遮蔽都必须走本函数，禁止各自解析 algoResult
 * @param info [in] 单个节点的账本
 * @return 对端 nodeId 集合（去重；不含空串）
 */
std::set<std::string> GetDebtPeerNodeIds(const NodeMemDebtInfo& info);

 /**
 * @brief 账本投影模式
 *        KEEP_IN_SET=只保留命中条目（B 侧按柜过滤）；REMOVE_IN_SET=剔除命中条目（A 侧 SKIPPED 遮蔽）
 */
enum class PeerFilterMode
{
    KEEP_IN_SET = 0,
    REMOVE_IN_SET = 1
};

 /**
 * @brief 按对端 nodeId 集合投影一份节点账本（A 侧遮蔽与 B 侧按柜过滤共用同一个实现）
 *        语义为"条目级"：一条记录只要其任一相关对端命中集合即视为命中；落选条目被丢弃，不改原账本
 * @param src [in] 源账本（只读，不会被修改）
 * @param peerNodeIds [in] 对端 nodeId 集合
 * @param mode [in] 投影模式，见 PeerFilterMode
 * @return 投影后的账本；KEEP_IN_SET 下无命中时返回空账本（不等于"采集失败"）
 */
NodeMemDebtInfo FilterDebtInfoByPeerNodeIds(const NodeMemDebtInfo& src, const std::set<std::string>& peerNodeIds,
                                            PeerFilterMode mode);

 /**
 * @brief 节点状态是否为"确认故障"（设计文档 §5.2 步骤2）
 *        FAULT/PRE_BMC/UNKNOWN -> true（不采集，应答 CONFIRMED_FAULT）
 *        INIT/SMOOTHING/WORKING -> false（必须尝试采集，能应答即账本完整）
 * @param state [in] 节点集群状态
 * @return true 确认故障
 */
bool IsConfirmedFaultClusterState(nodeController::UbseNodeClusterState state);
} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_MEM_CABINET_LEDGER_UTIL_H
