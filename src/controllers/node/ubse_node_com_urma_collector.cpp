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
#include "ubse_context.h"
#include "ubse_lcne_module.h"
#include "ubse_logger_inner.h"
#include "ubse_node_controller.h"
#include "ubse_str_util.h"
#include "ubse_urma_uvs_module.h"

namespace ubse::nodeController {
using namespace ubse::utils;
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::mti;
using namespace ubse::urma;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_NODE_CONTROLLER_MID)

UbseResult UbseNodeComUrmaCollector::FillComUrmaInfo()
{
    auto lcneModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::mti::UbseLcneModule>();
    if (lcneModule == nullptr) {
        UBSE_LOG_ERROR << "Get lcne module failed. ";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    std::vector<MtiNodeInfo> ubseNodeInfos{};
    auto ret = lcneModule->UbseGetAllNodeInfos(ubseNodeInfos);
    for (const auto &ubseNodeInfo : ubseNodeInfos) {
        comUrmaInfos[ubseNodeInfo.nodeId].urmaDevEid = ubseNodeInfo.eid;
    }

    auto allSocketComEid = lcneModule->GetAllSocketComEid();
    for (const auto &socketComEid : allSocketComEid) {
        std::string nodeId;
        std::string ubpuId;
        socketComEid.first.SplitDevName(nodeId, ubpuId);

        UbseUrmaUvsFe fe{};
        fe.ubpuId = ubpuId;
        fe.primaryEid = socketComEid.second.primaryEid;
        for (auto &port : socketComEid.second.portEidList) {
            fe.portEid[port.first] = port.second.urmaEid;
        }
        comUrmaInfos[nodeId].feList.push_back(fe);
    }
    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::SetComUrma(std::vector<PhysicalLink> &allLinkInfo, bool isBeforeElection)
{
    auto urmaUvsModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::urma::UbseUrmaUvsModule>();
    if (urmaUvsModule == nullptr) {
        UBSE_LOG_ERROR << "Get urma_uvs module failed. ";
        return UBSE_ERROR;
    }
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

    ret = urmaUvsModule->SetUvsInfo(ubseNodeInfo.nodeId, allLinkInfo, hostUrmaInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Set urma_uvs failed.";
        return ret;
    }

    if (isBeforeElection) {
        ret = urmaUvsModule->ActivateBondingDevice(comUrmaInfos[ubseNodeInfo.nodeId].urmaDevEid);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Activate urmaDevEid=" << comUrmaInfos[ubseNodeInfo.nodeId].urmaDevEid << " failed.";
        }
    }
    return ret;
}

UbseResult UbseNodeComUrmaCollector::GetAllComUrma(std::vector<UbseUrmaUvsNodeInfo> &hostUrmaInfos)
{
    hostUrmaInfos.clear();
    hostUrmaInfos.reserve(comUrmaInfos.size());
    for (const auto &kv : comUrmaInfos) {
        std::vector<UbseUrmaUvsAggrDev> aggrs;
        aggrs.push_back(kv.second);
        UbseUrmaUvsNodeInfo info{kv.first, aggrs};
        hostUrmaInfos.push_back(info);
    }
    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::GetCurNodeTopo(std::vector<PhysicalLink> &allLinkInfo)
{
    auto lcneModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::mti::UbseLcneModule>();
    if (lcneModule == nullptr) {
        UBSE_LOG_ERROR << "Get lcne module failed. ";
        return UBSE_ERROR;
    }

    DevTopology devTopology{};
    auto ret = lcneModule->UbseGetDevTopology(devTopology);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "get topology info not successful, ret: " << FormatRetCode(ret);
        return ret;
    }

    for (const auto &kv : devTopology) {
        std::string nodeId;
        std::string ubpuId;
        kv.first.SplitDevName(nodeId, ubpuId);

        for (const auto &portKv : kv.second.second) {
            if (portKv.second.portStatus == ubse::mti::PortStatus::DOWN) {
                continue;
            }
            PhysicalLink link{};
            if (ConvertStrToUint32(nodeId, link.slotId) != UBSE_OK ||
                ConvertStrToUint32(ubpuId, link.chipId) != UBSE_OK ||
                ConvertStrToUint32(portKv.second.portId, link.portId) != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to convert nodeId=" << nodeId << ", ubpuId=" << ubpuId
                               << ", portId=" << portKv.second.portId;
                continue;
            }
            if (ConvertStrToUint32(portKv.second.remoteSlotId, link.peerSlotId) != UBSE_OK ||
                ConvertStrToUint32(portKv.second.remoteChipId, link.peerChipId) != UBSE_OK ||
                ConvertStrToUint32(portKv.second.remotePortId, link.peerPortId) != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to convert nodeId=" << portKv.second.remoteSlotId
                               << ", ubpuId=" << portKv.second.remoteChipId
                               << ", portId=" << portKv.second.remotePortId;
                continue;
            }
            link.linkStatus = LinkStatus::init;
            allLinkInfo.push_back(link);
        }
    }
    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::GetCurNodeIouList(std::vector<UbseLcneIouInfo> &iouList)
{
    auto lcneModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::mti::UbseLcneModule>();
    if (lcneModule == nullptr) {
        UBSE_LOG_ERROR << "Get lcne module failed. ";
        return UBSE_ERROR;
    }

    DevTopology devTopology{};
    auto ret = lcneModule->UbseGetDevTopology(devTopology);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "get topology info not successful, ret: " << FormatRetCode(ret);
        return ret;
    }
    iouList.clear();
    iouList.reserve(devTopology.size());

    for (const auto &kv : devTopology) {
        const auto& deviceInfo = kv.second.first;
        iouList.push_back(UbseLcneIouInfo{
            .slotId = deviceInfo.slotId,
            .ubpuId = deviceInfo.chipId,
            .iouId   = deviceInfo.cardId
        });
    }
    return UBSE_OK;
}
} // namespace ubse::nodeController