/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_node.h"
namespace ubse::nodeController {

uint32_t UbseMemGetTopologyInfo(std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology)
{
    std::vector<MemNodeData> memNodeDatas;
    TelemetrySocketData telemetrySocketData;
    telemetrySocketData.nodeId = "node1";
    telemetrySocketData.hostname = "0.0.0.0";
    SocketData socketData;
    socketData.socketId = "0";
    NumaData numaData;
    numaData.numaId = "0";
    CpuData cpuData;
    cpuData.CpuId = "0";
    socketData.numas.push_back(numaData);
    socketData.cpus.push_back(cpuData);
    telemetrySocketData.socket = socketData;
    return 0;
}

uint32_t UbseGetNodeTopology(std::vector<UbseNodeTopology>& topologies)
{
    return 0;
}
} // namespace ubse::nodeController