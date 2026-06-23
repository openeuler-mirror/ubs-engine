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

#include "ubse_node_controller.h"

namespace ubse::nodeController {

std::unordered_map<std::string, UbseNodeInfo> UbseNodeController::GetAllNodes()
{
    std::unordered_map<std::string, UbseNodeInfo> maps{};
    return maps;
}

uint32_t UbseNodeController::GetMemGroupNodeList(UbseMemGroupNodeList& groupList)
{
    return 0;
}

uint32_t UbseNodeController::GetMemProviderNodeList(UbseMemProviderNodeList& providerList)
{
    return 0;
}

UbseNodeInfo UbseNodeController::GetNodeById(const std::string& nodeId)
{
    UbseNodeInfo ubseNodeInfo;
    ubseNodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    return ubseNodeInfo;
}

static std::string g_mockCurrentNodeId = "NODE0";

void MockSetCurrentNodeId(const std::string& nodeId)
{
    g_mockCurrentNodeId = nodeId;
}
void MockResetCurrentNodeId()
{
    g_mockCurrentNodeId = "NODE0";
}

std::string UbseNodeController::GetCurrentNodeId()
{
    return g_mockCurrentNodeId;
}

UbseNodeInfo UbseNodeController::GetCurNode()
{
    UbseNodeInfo info{};
    info.localState = UbseNodeLocalState::UBSE_NODE_READY;
    info.blockSize = 256;
    return info;
}

uint32_t UbseNodeController::RegLocalStateNotifyHandler(const UbseLocalStateNotifyHandler& handler)
{
    return 0;
}

uint32_t UbseNodeController::RegClusterStateNotifyHandler(const UbseClusterStateNotifyHandler& handler)
{
    return 0;
}

void UbseNodeController::SetCurrentNodeId(const std::string& nodeId) {}

void UbseNodeController::CleanAfterMasterSwitchRole() {}

std::vector<UbseNodeInfo> UbseNodeController::GetStaticNodeInfo()
{
    return {};
}

UbseNodeInfo UbseNodeController::GetNodeBySlotId(uint32_t slotId)
{
    UbseNodeInfo info{};
    return info;
}

uint32_t UbseNodeController::GetLocalEidBySocket(const uint32_t& socketId, uint32_t& eid)
{
    return 0;
}

uint32_t UbseNodeController::GetEid(const std::string& nodeId, const uint32_t& socketId, uint32_t& eid)
{
    return 0;
}

uint32_t UbseNodeController::UpdateNodeInfo(const std::string& nodeId, UbseNodeInfo info)
{
    return 0;
}

void UbseNodeController::UbseSocketIdChange(const std::string& nodeId) {}

void UbseNodeController::UpdateNodeInfoLocalState(UbseNodeLocalState state) {}

uint32_t UbseNodeController::UpdateNodeInfoClusterState(const std::string& nodeId, UbseNodeClusterState state)
{
    return 0;
}

std::map<std::string, PhysicalLink> UbseNodeController::UbseGetDirConnectInfo()
{
    return {};
}

std::set<uint32_t> UbseNodeController::UbseGetAllDeployedNode()
{
    return {};
}

void UbseNodeController::UpdateDevDirConnectInfo() {}

void UbseNodeController::UpdateConnect(PhysicalLink& physicalLink, std::string& linkId) {}

void UbseNodeController::PrintDevDirConnectInfo() {}

void UbseNodeController::CreateAndUpdateInfo(std::pair<const UbseCpuLocation, UbseCpuInfo> topoInfo) {}

} // namespace ubse::nodeController
