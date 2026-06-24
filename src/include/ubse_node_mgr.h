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

#ifndef UBS_ENGINE_UBSE_NODE_DISCOVERY_H
#define UBS_ENGINE_UBSE_NODE_DISCOVERY_H

#include <vector>
#include "ubse_node_mgr_def.h"
#include "ubse_node_controller.h"

namespace ubse::nodeMgr {
/**
 * @brief 获取当前节点的静态信息
 * @return 当前节点的UbseNodeStaticInfo，包含nodeId、groupId、addr等字段
 */
UbseNodeStaticInfo GetCurrentNode();

/**
 * @brief 获取集群中所有节点的静态信息列表
 * @return 集群中所有节点的UbseNodeStaticInfo列表
 */
std::vector<UbseNodeStaticInfo> GetAllNodes();

/**
 * @brief 获取以逻辑机柜ID为键的节点映射
 * @return 以groupId为键、对应机柜下节点列表为值的映射
 */
std::unordered_map<uint16_t, std::vector<UbseNodeStaticInfo>> GetAllNodesStoredByGroup();

/**
 * @brief 根据节点ID获取对应节点的静态信息
 * @param nodeId [in] 节点ID
 * @return 匹配的UbseNodeStaticInfo，未找到时返回空对象
 */
UbseNodeStaticInfo GetUbseNodeById(const std::string &nodeId);

/**
 * @brief 判断当前是否使用URMA通信模式
 * @return true URMA模式
 * @return false TCP模式
 */
bool IsUrma();

/**
 * @brief 获取根节点IP地址列表，仅在TCP_ROOT_MODE下生效
 * @return 根节点IP地址字符串列表
 */
std::vector<std::string> GetRootIpList();

/**
 * @brief 根据逻辑机柜ID获取该机柜下所有节点的静态信息
 * @param groupId [in] 逻辑机柜ID
 * @return 对应机柜下所有节点的UbseNodeStaticInfo列表
 */
std::vector<UbseNodeStaticInfo> GetNodesByGroupId(uint16_t groupId);

/**
 * @brief 获取当前节点物理链路信息
 * @param allLinkInfo [out] 物理链路
 * @return
 */
uint32_t GetCurPhysicalLinkInfo(std::vector<nodeController::PhysicalLink> &allLinkInfo);

/**
 * @brief urma场景下, 向node mgr申请占用bonding0设备
 */
void ApplyUrmaDev();
} // namespace ubse::nodeMgr
#endif // UBS_ENGINE_UBSE_NODE_DISCOVERY_H
