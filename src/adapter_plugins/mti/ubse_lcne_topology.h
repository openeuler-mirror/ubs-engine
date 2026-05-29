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

#ifndef UBSE_LCNE_TOPOLOGY_H
#define UBSE_LCNE_TOPOLOGY_H

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_http_common.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"

#include <atomic>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include "adapter_plugins/mti/ubse_mti_def.h"

namespace ubse::mti {
using ubse::common::def::UbseResult;
using ubse::http::UbseHttpRequest;
using ubse::http::UbseHttpResponse;
DevType StringToDevType(const std::string& str);
adapter_plugins::mti::UbseDevType StringToUbseDevType(const std::string& str);
DevType StringToDevTypeVBus(const std::string& str);

struct LcnePortInfo {
    std::string portId;   // 端口ID
    std::string ifName;   // 端口名（带宽）
    std::string portRole; // 表示框内/框外
    std::string portStatus;
    std::string remoteSlotId; // 对端槽位号
    std::string remoteChipId; // 对端模组号
    std::string remoteCardId; // 对端卡号
    std::string remoteIfName; // 对端端口名（带宽）
    std::string remotePortId; // 对端端口号
};

struct LcneNodeInfo {
    std::string slotId; // 槽位号
    std::string chipId; // 模组号
    std::string cardId; // 卡号
    std::string type;   // 设备类型
    std::vector<LcnePortInfo> ports;
};

struct LcnePortCnaInfo {
    std::string portId;     // port_id / 4 = portGroupId
    std::string portCna;    // <bus-port-cna>
    uint32_t portCnaUint32; // 转为标准的cna值
};

struct LcneNodeCnaInfo {
    std::string slotId;        // 槽位号
    std::string chipId;        // 模组号
    std::string cardId;        // 卡号
    std::string busNodeCna;    // <bus-node-cna>
    uint32_t busNodeCnaUint32; // 转为标准的cna值
    std::vector<LcnePortCnaInfo> ports;
};

class UbseLcneTopology {
public:
    UbseResult Start();
    // 增加节点和边
    void UbseAddNodeAndEdge(const adapter_plugins::mti::UbseDeviceInfo& nodeInfo,
                            const adapter_plugins::mti::UbseMtiCpuTopoPortInfo& portInfo);
    // 移除边（如果节点无边则移除该节点）
    void UbseEraseEdge(const adapter_plugins::mti::UbseDevName& dev, const adapter_plugins::mti::UbseDevPortName& port);
    // 向拓扑图增加信息
    // 增加cna信息
    void UbseAddNodeAndEdgeCna(const std::vector<LcneNodeCnaInfo>& lcneCnaInfos);
    // 创建设备拓扑
    UbseResult CreateDevTopology();
    // 获取拓扑图
    UbseResult UbseGetDevTopology(adapter_plugins::mti::UbseDevTopology& devTopology);
    // Tcp回调注册
    UbseResult RegHttpHandler();
    std::string GetUrlLinkUpAndDown();
    static std::string urlLinkUpAndDown;

private:
    uint32_t PortUpDownFunc(const UbseHttpRequest& req, UbseHttpResponse& resp);

    void ClearUbseTopologyInfo();

    UbseResult UbseDevGetCna();
    UbseResult UbseDevGetTopology();

    void UbseNodeAddTopology(std::vector<LcneNodeInfo>& lcneNodes);
    void IdentifyTopoChange(
        std::unordered_map<adapter_plugins::mti::UbseDevName,
                           std::unordered_set<adapter_plugins::mti::UbseDevName, adapter_plugins::mti::UbseDevNameHash>,
                           adapter_plugins::mti::UbseDevNameHash>
            peerDevMapOld,
        std::string& eventMessage);
    UbseResult PubPortUpDownEvent(const std::string& linkUpDown, const std::string& interfaceName);
    void AddPortCnaInfo(
        const LcneNodeCnaInfo& lcneCnaInfo, const adapter_plugins::mti::UbseDevName& localDevName,
        std::unordered_map<adapter_plugins::mti::UbseDevPortName, adapter_plugins::mti::UbseMtiCpuTopoPortInfo,
                           adapter_plugins::mti::UbseDevPortNameHash>& portInfo);

private:
    // K:设备名称
    // V:[本节点信息，链接信息]
    // CPU以外的设备均没有链接
    adapter_plugins::mti::UbseDevTopology ubseTopologyInfo;
    std::shared_mutex mtx;

    // socketId -> nodeId
    std::unordered_map<std::string, std::string> devNameToNodeIdMap;
    // nodeId -> socketId
    std::unordered_map<std::string, std::unordered_set<std::string>> nodeIdToDevNameMap;
    // 本节点每个设备的直连设备
    std::unordered_map<adapter_plugins::mti::UbseDevName,
                       std::unordered_set<adapter_plugins::mti::UbseDevName, adapter_plugins::mti::UbseDevNameHash>,
                       adapter_plugins::mti::UbseDevNameHash>
        peerDevMap;
    // 所链接的设备和它的端口的映射
    std::unordered_map<std::string, std::unordered_set<std::string>> peerDevToPortMap;
};
} // namespace ubse::mti

#endif // UBSE_LCNE_TOPOLOGY_H
