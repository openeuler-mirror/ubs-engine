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

#include "ubse_node_com_urma_collector.h"
#include <vector>
#include "securec.h"
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_lcne_module.h"
#include "ubse_logger_module.h"
#include "ubse_node_controller.h"
#include "ubse_smbios.h"
#include "ubse_smbios_impl.h"
#include "ubse_str_util.h"
#include "ubse_mti_eid_interface.h"
#include "ubse_urma_uvs_module.h"

#include "adapter_plugins/mti/ubse_mti_interface.h"

namespace ubse::nodeController {
using namespace ubse::utils;
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::adapter_plugins::mti;
using namespace ubse::adapter_plugins::smbios;
using namespace ubse::urma;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseNodeComUrmaCollector::FillComUrmaInfo()
{
    comUrmaInfos.clear();
    std::vector<UbseMtiNodeInfo> ubseNodeInfos;
    auto ret = UbseMtiInterface::GetInstance().GetClusterNodeInfoList(ubseNodeInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get all cluster node info failed, " << FormatRetCode(ret);
        return ret;
    }
    std::map<UbseMtiIouInfo, UbseMtiEidGroup> comUrmaInfoMap{};
    ret = UbseMtiInterface::GetInstance().GetMtiComEid(comUrmaInfoMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get all socket eid failed, " << FormatRetCode(ret);
        return ret;
    }
    for (const auto& ubseNodeInfo : ubseNodeInfos) {
        comUrmaInfos[ubseNodeInfo.nodeId].urmaDevEid = ubseNodeInfo.eid;
    }

    for (const auto &socketComEid : comUrmaInfoMap) {
        FillComUrmaFeInfo(socketComEid.first.slotId, socketComEid);
    }
    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::FillComUrmaInfoClos()
{
    comUrmaInfos.clear();
    UbseMtiNodeInfo curNodeInfo{};
    auto ret = UbseMtiInterface::GetInstance().GetLocalNodeInfo(curNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get current node info failed, " << FormatRetCode(ret);
        return ret;
    }
    comUrmaInfos[curNodeInfo.nodeId].urmaDevEid = curNodeInfo.eid;
    std::map<UbseMtiIouInfo, UbseMtiEidGroup> comUrmaInfoMap{};
    ret = UbseMtiInterface::GetInstance().GetMtiComEid(comUrmaInfoMap);
    if (ret != UBSE_OK || comUrmaInfoMap.size() != NO_2) {
        UBSE_LOG_ERROR << "Get all socket eid failed, " << FormatRetCode(ret);
        return ret;
    }
    for (const auto &socketComEid : comUrmaInfoMap) {
        FillComUrmaFeInfo(curNodeInfo.nodeId, socketComEid);
    }

    for (uint32_t serverIdx = 0; serverIdx < UBSE_CLOS_MAX_NODE_NUM; serverIdx++) {
        ret = ProcessClusterNode(curNodeInfo.nodeId, serverIdx);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Process ClusterNode failed, serverIdx=" << serverIdx << ", ret=" << ret;
            return ret;
        }
    }
    return UBSE_OK;
}

void UbseNodeComUrmaCollector::FillComUrmaFeInfo(const std::string& nodeId,
                                                 const std::pair<UbseMtiIouInfo, UbseMtiEidGroup>& socketComEid)
{
    UbseUrmaUvsFe fe{};
    fe.ubpuId = socketComEid.first.ubpuId;
    fe.primaryEid = socketComEid.second.primaryEid;
    fe.entityId = socketComEid.second.entityId;
    for (auto &port : socketComEid.second.portEids) {
        fe.portEid[port.first] = port.second;
    }
    comUrmaInfos[nodeId].feList.push_back(fe);
}

UbseResult UbseNodeComUrmaCollector::ProcessClusterNode(const std::string& curNodeId, uint32_t serverIdx)
{
    uint32_t nodeId = serverIdx + 1;
    std::string targetNodeId = std::to_string(nodeId);
    if (targetNodeId == curNodeId) {
        return UBSE_OK; // 当前节点跳过处理
    }

    UbseUrmaUvsAggrDev aggr_dev{};
    aggr_dev.urmaDevEid = utils::GenerateUrmaDevEid(0, nodeId, 0, 0);
    if (aggr_dev.urmaDevEid.empty()) {
        UBSE_LOG_ERROR << "Generate bondingEid failed, nodeId=" << nodeId;
        return UBSE_ERROR;
    }

    // 处理当前节点的FE列表，生成聚合设备的FE信息
    for (auto& fe : comUrmaInfos[curNodeId].feList) {
        UbseUrmaUvsFe fe_aggr_dev{};
        auto ret = ProcessFeDevice(serverIdx, fe, fe_aggr_dev);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "ProcessFeDevice failed, nodeId=" << nodeId << ", ret=" << ret;
            return ret;
        }
        aggr_dev.feList.push_back(fe_aggr_dev);
    }

    comUrmaInfos[targetNodeId] = aggr_dev;
    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::ProcessFeDevice(uint32_t serverIdx,
                                                     const UbseUrmaUvsFe& srcFe, UbseUrmaUvsFe& destFe)
{
    destFe.ubpuId = srcFe.ubpuId;
    destFe.entityId = srcFe.entityId;

    // 处理primaryEID
    auto ret = OverwriteEid(serverIdx, srcFe.primaryEid, destFe.primaryEid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Overwrite primaryEid failed, serverIdx=" << serverIdx << ", ret=" << ret;
        return ret;
    }

    // 处理portEID列表
    for (const auto& port : srcFe.portEid) {
        ret = OverwriteEid(serverIdx, port.second, destFe.portEid[port.first]);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Overwrite portEid failed, serverIdx=" << serverIdx
                           << ", port=" << port.first << ", ret=" << ret;
            return ret;
        }
    }

    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::SetComUrma(std::vector<PhysicalLink> &allLinkInfo, bool isBeforeElection)
{
    UbseNodeInfo ubseNodeInfo = UbseNodeController::GetInstance().GetCurNode();
    if (ubseNodeInfo.nodeId.empty()) {
        UBSE_LOG_ERROR << "Current node id is empty.";
        return UBSE_ERROR;
    }

    std::vector<UbseUrmaUvsNodeInfo> hostUrmaInfos;
    auto ret = GetAllComUrma(hostUrmaInfos);
    if (ret != UBSE_OK || hostUrmaInfos.empty()) {
        UBSE_LOG_ERROR << "Get all com urma info failed.";
        return UBSE_ERROR;
    }
    ret = UbsePushTopoAndBondingToUvs(ubseNodeInfo.nodeId, allLinkInfo, hostUrmaInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Set urma_uvs failed.";
        return ret;
    }

    if (isBeforeElection) {
        const std::string aggrDevName = "bonding_dev_0";
        ret = UbseActiveBonding(comUrmaInfos[ubseNodeInfo.nodeId].urmaDevEid, aggrDevName);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Activate urmaDevEid=" << comUrmaInfos[ubseNodeInfo.nodeId].urmaDevEid << " failed.";
        }
    }
    return ret;
}

UbseResult UbseNodeComUrmaCollector::GetAllComUrma(std::vector<UbseUrmaUvsNodeInfo>& hostUrmaInfos)
{
    hostUrmaInfos.clear();
    hostUrmaInfos.reserve(comUrmaInfos.size());
    for (const auto& kv : comUrmaInfos) {
        std::vector<UbseUrmaUvsAggrDev> aggrs;
        aggrs.push_back(kv.second);
        UbseUrmaUvsNodeInfo info{kv.first, std::move(aggrs)};
        hostUrmaInfos.push_back(std::move(info));
    }
    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::GetComUrmaByNodeId(const std::string& nodeId,
                                                        std::vector<UbseUrmaUvsNodeInfo>& hostUrmaInfos)
{
    hostUrmaInfos.clear();
    auto it = comUrmaInfos.find(nodeId);
    if (it == comUrmaInfos.end()) {
        UBSE_LOG_ERROR << "Node " << nodeId << " not found in comUrmaInfos";
        return UBSE_ERROR;
    }
    std::vector<UbseUrmaUvsAggrDev> aggrs;
    aggrs.push_back(it->second);
    UbseUrmaUvsNodeInfo info{it->first, std::move(aggrs)};
    hostUrmaInfos.push_back(std::move(info));
    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::GetCurNodeTopo(std::vector<PhysicalLink>& allLinkInfo)
{
    std::vector<PhysicalLink> allLinks;
    auto ret = GetCurNodePorts(allLinks);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get cur node ports failed, " << FormatRetCode(ret);
        return ret;
    }
    for (auto &link : allLinks) {
        if (link.linkStatus == LinkStatus::unavailable) {
            continue;
        } else {
            allLinkInfo.push_back(link);
        }
    }
    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::GetCurNodePorts(std::vector<PhysicalLink> &allLinkInfo)
{
    UbseDevTopology devTopology{};
    auto ret = UbseMtiInterface::GetInstance().GetCurNodeTopo(devTopology);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[MTI] get devTopology not successful, " << FormatRetCode(ret);
        return ret;
    }
    for (const auto &kv : devTopology) {
        std::string nodeId;
        std::string ubpuId;
        kv.first.GetNodeIdAndChipId(nodeId, ubpuId);
        for (const auto &portKv : kv.second.second) {
            PhysicalLink link{};
            if (ConvertStrToUint32(nodeId, link.slotId) != UBSE_OK || ConvertStrToUint32(ubpuId, link.chipId) != UBSE_OK
                || ConvertStrToUint32(portKv.second.portId, link.portId) != UBSE_OK) {
                UBSE_LOG_WARN << "Failed to convert nodeId=" << nodeId << ", ubpuId=" << ubpuId
                              << ", portId=" << portKv.second.portId << ", skip this link";
                continue;
            }
            link.interfaceName = portKv.second.ifName;

            if (!UbseSmbios::GetInstance().IsClosType()) {
                if (ConvertStrToUint32(portKv.second.remoteSlotId, link.peerSlotId) != UBSE_OK ||
                    ConvertStrToUint32(portKv.second.remoteChipId, link.peerChipId) != UBSE_OK ||
                    ConvertStrToUint32(portKv.second.remotePortId, link.peerPortId) != UBSE_OK) {
                    UBSE_LOG_WARN << "Failed to convert slotId=" << portKv.second.remoteSlotId << ", ubpuId="
                        << portKv.second.remoteChipId << ", portId=" << portKv.second.remotePortId
                        << ", skip this link";
                    continue;
                }
            }
            if (portKv.second.portStatus == UbseMtiCpuTopoPortStatus::DOWN) {
                link.linkStatus = LinkStatus::unavailable;
            } else {
                link.linkStatus = LinkStatus::init;
            }

            allLinkInfo.push_back(link);
        }
    }
    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::GetCurNodeIouList(std::vector<UbseMtiIouInfo>& iouList)
{
    UbseDevTopology devTopology{};
    auto ret = UbseMtiInterface::GetInstance().GetCurNodeTopo(devTopology);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "get topology info failed, " << FormatRetCode(ret);
        return ret;
    }
    iouList.clear();
    iouList.reserve(devTopology.size());

    for (const auto &[devName, deviceInfo] : devTopology) {
        iouList.emplace_back(deviceInfo.first.slotId, deviceInfo.first.chipId, deviceInfo.first.cardId);
    }
    return UBSE_OK;
}
} // namespace ubse::nodeController