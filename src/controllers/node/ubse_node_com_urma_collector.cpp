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
        return UBSE_ERROR;
    }

    std::vector<MtiNodeInfo> ubseNodeInfos{};
    auto ret = lcneModule->UbseGetAllNodeInfos(ubseNodeInfos);
    for (const auto &ubseNodeInfo : ubseNodeInfos) {
        comUrmaInfos[ubseNodeInfo.nodeId].urmaDevEid = ubseNodeInfo.eid;
        comUrmaInfos[ubseNodeInfo.nodeId].urmaDevType = UrmaDevType::SELF_USED;
    }

    auto allSocketComEid = lcneModule->GetAllSocketComEid();
    for (const auto &socketComEid : allSocketComEid) {
        std::string nodeId;
        std::string socketId;
        socketComEid.first.SplitDevName(nodeId, socketId);

        UbseFeInfo fe{};
        if (ConvertStrToUint32(nodeId, fe.slotId) != UBSE_OK && ConvertStrToUint32(socketId, fe.ubpuId) != UBSE_OK &&
            ConvertStrToUint32(socketComEid.second.entityId, fe.entityId) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to convert nodeId=" << nodeId << ", socketId=" << socketId
                           << ", entityId=" << socketComEid.second.entityId;
            return UBSE_ERROR;
        }
        fe.iouId = 1;
        fe.fetype = FeType::PHYSICAL_TYPE;
        fe.primaryEid[0] = socketComEid.second.primaryEid;
        for (auto &port : socketComEid.second.portEidList) {
            uint32_t portId;
            if (ConvertStrToUint32(port.first, portId) != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to convert portId=" << port.first;
                return UBSE_ERROR;
            }
            fe.portEidInfos[portId] = port.second.urmaEid;
        }
        comUrmaInfos[nodeId].feInfoLists.push_back(fe);
    }
    return UBSE_OK;
}

UbseResult UbseNodeComUrmaCollector::SetComUrma(std::vector<PhysicalLink> &allLinkInfo)
{
    auto urmaUvsModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::urma::UbseUrmaUvsModule>();
    if (urmaUvsModule == nullptr) {
        UBSE_LOG_ERROR << "Get urma_uvs module failed. ";
        return UBSE_ERROR;
    }
    uint32_t curNodeId;
    UbseNodeInfo ubseNodeInfo = UbseNodeController::GetInstance().GetCurNode();
    if (ubseNodeInfo.nodeId.empty() || ConvertStrToUint32(ubseNodeInfo.nodeId, curNodeId) != UBSE_OK) {
        UBSE_LOG_ERROR << "Current node id is empty.";
        return UBSE_ERROR;
    }

    std::vector<UbseUrmaInfo> hostUrmaInfos;
    auto ret = GetAllComUrma(hostUrmaInfos);
    if (ret != UBSE_OK || hostUrmaInfos.empty()) {
        UBSE_LOG_ERROR << "Get all com urma info failed.";
        return ret;
    }

    ret = urmaUvsModule->SetUvsInfo(curNodeId, allLinkInfo, hostUrmaInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Set urma_uvs failed.";
        return ret;
    }

    if (comUrmaInfos[ubseNodeInfo.nodeId].name.empty()) {
        ret = urmaUvsModule->ActivateBondingDevice(comUrmaInfos[ubseNodeInfo.nodeId].urmaDevEid);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Activate urmaDevEid=" << comUrmaInfos[ubseNodeInfo.nodeId].urmaDevEid << " failed.";
        }
        comUrmaInfos[ubseNodeInfo.nodeId].state = (ret == UBSE_OK) ? UrmaDevState::ACTIVED : UrmaDevState::INACTIVED;
        ret = urmaUvsModule->GetNameByUrmaEid(comUrmaInfos[ubseNodeInfo.nodeId].urmaDevEid,
                                              comUrmaInfos[ubseNodeInfo.nodeId].name);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Get name by urmaDevEid=" << comUrmaInfos[ubseNodeInfo.nodeId].urmaDevEid << " failed.";
        }
    }
    return ret;
}

UbseResult UbseNodeComUrmaCollector::GetAllComUrma(std::vector<UbseUrmaInfo> &hostUrmaInfos)
{
    hostUrmaInfos.clear();
    hostUrmaInfos.reserve(comUrmaInfos.size());
    for (const auto &kv : comUrmaInfos) {
        hostUrmaInfos.push_back(kv.second);
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
        UBSE_LOG_WARN << "[MTI] get topology info not successful, ret: " << FormatRetCode(ret);
        return ret;
    }

    for (const auto &kv : devTopology) {
        std::string nodeId;
        std::string socketId;
        kv.first.SplitDevName(nodeId, socketId);

        for (const auto &portKv : kv.second.second) {
            if (portKv.second.portStatus == ubse::mti::PortStatus::DOWN) {
                continue;
            }
            PhysicalLink link{};
            if (ConvertStrToUint32(nodeId, link.slotId) != UBSE_OK &&
                ConvertStrToUint32(socketId, link.chipId) != UBSE_OK &&
                ConvertStrToUint32(portKv.second.portId, link.portId) != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to convert nodeId=" << nodeId << ", socketId=" << socketId
                               << ", portId=" << portKv.second.portId;
                return UBSE_ERROR;
            }
            if (ConvertStrToUint32(portKv.second.remoteSlotId, link.peerSlotId) != UBSE_OK &&
                ConvertStrToUint32(portKv.second.remoteChipId, link.peerChipId) != UBSE_OK &&
                ConvertStrToUint32(portKv.second.remotePortId, link.peerPortId) != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to convert nodeId=" << portKv.second.remoteSlotId
                               << ", socketId=" << portKv.second.remoteChipId
                               << ", portId=" << portKv.second.remotePortId;
                return UBSE_ERROR;
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
        UBSE_LOG_WARN << "[MTI] get topology info not successful, ret: " << FormatRetCode(ret);
        return ret;
    }
    for (const auto &kv : devTopology) {
        UbseDeviceInfo deviceInfo = kv.second.first;
        UbseLcneIouInfo iouInfo{};
        iouInfo.slotId = deviceInfo.slotId;
        iouInfo.ubpuId = deviceInfo.chipId;
        iouInfo.iouId =  deviceInfo.cardId;
        iouList.push_back(iouInfo);
    }
    return UBSE_OK;
}
} // namespace ubse::nodeController