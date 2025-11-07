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

#include "ubse_lcne_topology.h"

#include <unistd.h>
#include <shared_mutex>
#include <thread>

#include "ubse_event_module.h"
#include "ubse_http_module.h"
#include "lcne/ubse_lcne_sub_topo_change_info.h"
#include "lcne/ubse_lcne_topology_client.h"
#include "lcne/ubse_topo_cna.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_logger_inner.h"
#include "ubse_topology_interface.h"

#include "ubse_thread_pool_module.h"

namespace ubse::mti {
using namespace ubse::log;
using namespace ubse::context;
using namespace ubse::http;
using namespace ubse::config;
using namespace ubse::task_executor;
using namespace ubse::lcne;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_DEV_MID)

std::string UbseLcneTopology::urlLinkUpAndDown{"/topolink/change/"};

UbseTaskExecutorPtr GetExecutor(const std::string &name)
{
    auto taskExecutor = ubse::context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        return nullptr;
    }
    return taskExecutor->Get(name);
}

UbseResult UbseLcneTopology::Start()
{
    // 订阅变化，必须订阅成功，LCNE尚未好，先不订阅成功
    if (UbseLcneLinkInfo::GetInstance().SubLcneLinkInfo() != UBSE_OK) {
        UBSE_LOG_WARN << "[MTI] SubLcneLinkInfo is failed.";
    }
    // 启动获取,必须获取成功
    if (CreateDevTopology() != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Obtain topology or cna information provided by LCNE is failed.";
        return UBSE_ERROR;
    };
    return UBSE_OK;
}

UbseResult UbseLcneTopology::PubUbseTopoChangeEvent(std::string &eventMessage) const
{
    auto &ctxRef = UbseContext::GetInstance();
    auto eventPtr = ctxRef.GetModule<ubse::event::UbseEventModule>();
    if (eventPtr == nullptr) {
        UBSE_LOG_WARN << "Can not get event module";
        return UBSE_ERROR;
    }
    auto ret = eventPtr->UbsePubEvent(ubseTopologyChangeId, eventMessage);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "pub event UbseTopologyChangeEvent is failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseLcneTopology::UbseGetDevTopology(DevTopology &devTopology)
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    devTopology = this->ubseTopologyInfo;
    return UBSE_OK;
}

UbseResult UbseLcneTopology::RegHttpHandler()
{
    // 端口link up/link down消息
    UbseHttpHandlerFunc portUpDownFunc =
        std::bind(&UbseLcneTopology::PortUpDownFunc, this, std::placeholders::_1, std::placeholders::_2);

    UbseHttpModule::RegHttpTcpService(UbseHttpMethod::UBSE_HTTP_METHOD_POST, urlLinkUpAndDown, portUpDownFunc);
    return UBSE_OK;
}

// 一次发送可能有多次改变，需要考虑同步性
void UbseLcneTopology::IdentifyTopoChange(
    std::unordered_map<DevName, std::unordered_set<DevName, DevNameHash>, DevNameHash> peerDevMapOld,
    std::string &eventMessage)
{
    // 端口Down
    for (auto &peerInfo : peerDevMapOld) {
        auto devNameOld = peerInfo.first;
        auto itNow = peerDevMap.find(devNameOld);
        // 老设备全部不连 || 老对端在新的不存在
        for (auto &peerDevOld : peerInfo.second) {
            if (itNow == peerDevMap.end() || itNow->second.find(peerDevOld) == itNow->second.end()) {
                std::string message = "DOWN;" + devNameOld.devName + ":" + peerDevOld.devName + "|";
                eventMessage += message;
            }
        }
    }
    // 端口UP
    for (auto &peerInfo : peerDevMap) {
        auto devNameNew = peerInfo.first;
        auto itOld = peerDevMapOld.find(devNameNew);
        // 新设备连接上线 || 新对端在老的不存在
        for (auto &peerDevNew : peerInfo.second) {
            if (itOld == peerDevMapOld.end() || itOld->second.find(peerDevNew) == itOld->second.end()) {
                std::string message = "UP;" + devNameNew.devName + ":" + peerDevNew.devName + "|";
                eventMessage += message;
            }
        }
    }
    return;
}

uint32_t UbseLcneTopology::PortUpDownFunc(const UbseHttpRequest &req, UbseHttpResponse &resp)
{
    UBSE_LOG_INFO << "[MTI] Received topology change message reported by lcne";
    std::string eventMessage{};
    // 必须重新获取拓扑，避免连线对端变化的情况，然后识别存在的变化并发事件
    std::unordered_map<DevName, std::unordered_set<DevName, DevNameHash>, DevNameHash> peerDevMapOld;
    {
        std::shared_lock<std::shared_mutex> lock(mtx); // mti模块获取信息
        peerDevMapOld = peerDevMap;
    }
    CreateDevTopology();
    IdentifyTopoChange(peerDevMapOld, eventMessage);
    UBSE_LOG_INFO << "[MTI] Topo Change eventMessage is " << eventMessage << " .";

    if (PubUbseTopoChangeEvent(eventMessage) != UBSE_OK) {
        UBSE_LOG_WARN << "[MTI] pub event UbseTopologyChangeEvent is failed! eventMessage is " << eventMessage;
    }

    resp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    return UBSE_OK;
}

void UbseLcneTopology::ClearUbseTopologyInfo()
{
    peerDevMap.clear();
    ubseTopologyInfo.clear();
}

void UbseLcneTopology::UbseAddNodeAndEdge(const UbseDeviceInfo &nodeInfo, const UbsePortInfo &portInfo)
{
    DevName devName(nodeInfo.slotId, nodeInfo.chipId);
    UbseDevPortName portName;
    portName.devPortName = portInfo.portId; // 端口名称均直接使用端口id即可
    auto it = ubseTopologyInfo.find(devName);
    if (it != ubseTopologyInfo.end()) {
        if (nodeInfo.type != DevType::CPU) {
            UBSE_LOG_DEBUG << "Adding non-CPU devices repeatedly";
        } else {
            it->second.second[portName] = portInfo;
        }
    } else {
        if (nodeInfo.type != DevType::CPU || portInfo.portId == "") { // 不存在边的情况
            ubseTopologyInfo[devName] = {{nodeInfo}, {}};
        } else {
            ubseTopologyInfo[devName] = {{nodeInfo}, {{portName, portInfo}}};
        }
    }
}

void UbseLcneTopology::UbseAddNodeAndEdgeCna(const std::vector<LcneNodeCnaInfo> &lcneCnaInfos)
{
    for (const auto &lcneCnaInfo : lcneCnaInfos) {
        std::string slotId = lcneCnaInfo.slotId;
        std::string chipId = lcneCnaInfo.chipId;
        DevName localDevName(slotId, chipId);
        auto it = ubseTopologyInfo.find(localDevName);
        if (it != ubseTopologyInfo.end()) {
            UBSE_LOG_DEBUG << "[MTI] Cna devName" << localDevName.devName << "exist in lcne cna.";
            it->second.first.busNodeCna = lcneCnaInfo.busNodeCnaUint32;
            auto &portInfo = it->second.second;
            AddPortCnaInfo(lcneCnaInfo, localDevName, portInfo);
        } else {
            UBSE_LOG_INFO << "[MTI] Cna devName" << localDevName.devName << "does not exist in ubseTopologyInfo.";
        }
    }
}

void UbseLcneTopology::AddPortCnaInfo(const LcneNodeCnaInfo &lcneCnaInfo, const DevName &localDevName,
                                      std::unordered_map<UbseDevPortName, UbsePortInfo, DevPortNameHash> &portInfo)
{
    for (const auto &cnaPortInfo : lcneCnaInfo.ports) {
        UbseDevPortName physicalPort{cnaPortInfo.portId};
        auto itPort = portInfo.find(physicalPort);
        if (itPort != portInfo.end()) {
            itPort->second.portCna = cnaPortInfo.portCnaUint32;
        } else {
            UBSE_LOG_INFO << "[MTI] Cna devName" << localDevName.devName << " and itPort " << physicalPort.devPortName
                        << " does not exist in ubseTopologyInfo.";
        }
    }
}

void UbseLcneTopology::UbseEraseEdge(const DevName &dev, const UbseDevPortName &port)
{
    auto it = ubseTopologyInfo.find(dev);
    if (it != ubseTopologyInfo.end()) {
        if (it->second.first.type != DevType::CPU) {
            UBSE_LOG_DEBUG << "The node is not a CPU in the device topology.";
        } else {
            it->second.second.erase(port);
        }
    } else {
        UBSE_LOG_DEBUG << "The node does not exist in the device topology.";
    }
}

static PortStatus StringToPortStatus(const std::string &str)
{
    if (str == "up") {
        return PortStatus::UP;
    } else {
        return PortStatus::DOWN;
    }
}

UbseResult UbseLcneTopology::UbseDevGetCna()
{
    std::vector<LcneNodeCnaInfo> lcneCnaInfos{};
    const auto ret = UbseTopoCna::GetInstance().QueryTopoCna(lcneCnaInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[MTI] get lcne topology cna info is failed.";
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "[MTI] get lcne topology cna info succeed.";
    UbseAddNodeAndEdgeCna(lcneCnaInfos);
    return UBSE_OK;
}

UbseResult UbseLcneTopology::UbseDevGetTopology()
{
    std::vector<LcneNodeInfo> lcneNodes{};
    const auto ret = UbseLcneTopologyClient::GetInstance().GetTopology(lcneNodes);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] get lcne topology info is failed.";
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "[MTI] get lcne topology info is succeed.";
    UbseNodeAddTopology(lcneNodes);
    return UBSE_OK;
}

void UbseLcneTopology::UbseNodeAddTopology(std::vector<LcneNodeInfo> &lcneNodes)
{
    for (const auto &node : lcneNodes) {
        // 节点信息
        UbseDeviceInfo nodeInfo;
        nodeInfo.slotId = node.slotId;
        nodeInfo.chipId = node.chipId;
        nodeInfo.cardId = node.cardId;
        nodeInfo.type = StringToDevType(node.type);
        std::string localDevNameStr = node.slotId + "-" + node.chipId;
        DevName devName(localDevNameStr);
        nodeInfo.devName = devName;

        // 边信息
        for (const auto &port : node.ports) {
            UbsePortInfo portInfo;
            portInfo.portId = port.portId;
            portInfo.ifName = port.ifName;
            portInfo.portRole = port.portRole;
            portInfo.remoteSlotId = port.remoteSlotId;
            portInfo.remoteChipId = port.remoteChipId;
            portInfo.remoteCardId = port.remoteCardId;
            portInfo.remotePortId = port.remotePortId;
            portInfo.portStatus = StringToPortStatus(port.portStatus);
            UbseDevPortName devPortName(nodeInfo.slotId, nodeInfo.chipId, nodeInfo.cardId, portInfo.portId);
            portInfo.devPortName = devPortName;
            std::string remoteDevNameStr = port.remoteSlotId + "-" + port.remoteChipId;
            DevName remoteDevName(remoteDevNameStr);
            portInfo.remoteDevName = remoteDevName;
            if (portInfo.portStatus == PortStatus::UP) {
                peerDevMap[devName].insert(remoteDevName); // 增加对端设备
            }
            UbseAddNodeAndEdge(nodeInfo, portInfo);
        }
        // 无连接场景
        if (node.ports.empty()) {
            UbsePortInfo portInfoEmpty;
            UbseAddNodeAndEdge(nodeInfo, portInfoEmpty);
        }
        devNameToNodeIdMap[localDevNameStr] = nodeInfo.slotId;
        nodeIdToDevNameMap[nodeInfo.slotId].insert(localDevNameStr);
    }
}

UbseResult UbseLcneTopology::CreateDevTopology()
{
    std::unique_lock<std::shared_mutex> lock(mtx); // mti模块获取信息
    UBSE_LOG_DEBUG << "[MTI] CreateDevTopology execution.";
    ClearUbseTopologyInfo();
    // 获取拓扑
    if (UbseDevGetTopology() != UBSE_OK) {
        return UBSE_ERROR;
    };
    // 获取CNA
    if (UbseDevGetCna() != UBSE_OK) {
        return UBSE_ERROR;
    };
    return UBSE_OK;
}

DevType StringToDevType(const std::string &str)
{
    if (str == "SSU") {
        return DevType::SSU;
    }
    if (str == "DPU") {
        return DevType::DPU;
    }
    if (str == "CPU-LINK") {
        return DevType::CPU;
    }
    return DevType::CPU;
}

DevType StringToDevTypeVBus(const std::string &str)
{
    if (str == "SSU") {
        return DevType::SSU;
    }
    if (str == "DPU") {
        return DevType::DPU;
    }
    if (str == "CPU") {
        return DevType::CPU;
    }
    return DevType::ALL;
}
} // namespace ubse::mti
