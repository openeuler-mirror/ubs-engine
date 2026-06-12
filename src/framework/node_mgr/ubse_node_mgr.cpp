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

#include "ubse_node_mgr.h"
#include "adapter_plugins/mti/ubse_smbios.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_node_static_info_mgr.h"
#include "ubse_context.h"
#include "ubse_lcne_module.h"
#include "ubse_str_util.h"

UBSE_DEFINE_THIS_MODULE("ubse");
namespace ubse::nodeMgr {
using namespace ubse::adapter_plugins::smbios;
using namespace ubse::log;
using namespace ubse::context;
using namespace ubse::mti;
using namespace ubse::utils;

UbseNodeStaticInfo GetCurrentNode()
{
    return  UbseNodeStaticInfoMgr::GetInstance().GetCurrentNode();
}

std::vector<UbseNodeStaticInfo> GetAllNodes()
{
    std::vector<UbseNodeStaticInfo> nodes{};
    auto nodesMap = UbseNodeStaticInfoMgr::GetInstance().GetAllNodes();
    for (const auto &pod : nodesMap) {
        for (const auto &node : pod.second) {
            nodes.push_back(node.second);
        }
    }
    return nodes;
}

std::unordered_map<uint16_t, std::vector<UbseNodeStaticInfo>> GetAllNodesStoredByGroup()
{
    std::unordered_map<uint16_t, std::vector<UbseNodeStaticInfo>> nodesMap{};
    auto nodes = UbseNodeStaticInfoMgr::GetInstance().GetAllNodes();
    for (const auto &pod : nodes) {
        for (const auto &node : pod.second) {
            nodesMap[pod.first].push_back(node.second);
        }
    }
    return nodesMap;
}

UbseNodeStaticInfo GetUbseNodeById(const std::string &nodeId)
{
    return UbseNodeStaticInfoMgr::GetInstance().GetUbseNodeById(nodeId);
}

bool IsUrma()
{
    return UbseNodeStaticInfoMgr::GetInstance().IsUrma();
}

std::vector<std::string> GetRootIpList()
{
    return UbseNodeStaticInfoMgr::GetInstance().GetRootIpList();
}

std::vector<UbseNodeStaticInfo> GetNodesByGroupId(uint16_t groupId)
{
    auto nodes = UbseNodeStaticInfoMgr::GetInstance().GetNodesByGroupId(groupId);
    std::vector<UbseNodeStaticInfo> nodesVec{};
    nodesVec.reserve(nodes.size());
    for (const auto &node : nodes) {
        nodesVec.push_back(node.second);
    }
    return nodesVec;
}

UbseResult GetCurPhysicalLinkInfo(std::vector<nodeController::PhysicalLink> &allLinkInfo)
{
    if (UbseSmbios::GetInstance().IsClosType()) {
        return UBSE_OK;
    }
    const auto lcneModule = UbseContext::GetInstance().GetModule<mti::UbseLcneModule>();
    if (lcneModule == nullptr) {
        UBSE_LOG_ERROR << "Get lcne module failed. ";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    UbseDevTopology devTopology{};
    const auto ret = lcneModule->UbseGetDevTopology(devTopology);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get topology info failed, " << FormatRetCode(ret);
        return ret;
    }
    for (const auto &kv : devTopology) {
        std::string nodeId;
        std::string ubpuId;
        kv.first.GetNodeIdAndChipId(nodeId, ubpuId);
        for (const auto &portKv : kv.second.second) {
            nodeController::PhysicalLink link{};
            if (ConvertStrToUint32(nodeId, link.slotId) != UBSE_OK ||
                ConvertStrToUint32(ubpuId, link.chipId) != UBSE_OK ||
                ConvertStrToUint32(portKv.second.portId, link.portId) != UBSE_OK) {
                UBSE_LOG_WARN << "Failed to convert nodeId=" << nodeId << ", ubpuId=" << ubpuId
                              << ", portId=" << portKv.second.portId << ", skip this link";
                continue;
            }
            const auto &remote = portKv.second;
            if (ConvertStrToUint32(remote.remoteSlotId, link.peerSlotId) != UBSE_OK ||
                ConvertStrToUint32(remote.remoteChipId, link.peerChipId) != UBSE_OK ||
                ConvertStrToUint32(remote.remotePortId, link.peerPortId) != UBSE_OK) {
                UBSE_LOG_WARN << "Failed to convert remoteSlotId=" << remote.remoteSlotId
                              << ", remoteChipId=" << remote.remoteChipId << ", remotePortId=" << remote.remotePortId
                              << ", skip this link";
                continue;
            }

            link.linkStatus = nodeController::LinkStatus::init;
            allLinkInfo.push_back(link);
        }
    }
    return UBSE_OK;
}

void ApplyUrmaDev()
{
    UbseNodeStaticInfoMgr::GetInstance().ApplyUrmaDev();
}
}
