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

#ifndef UBSE_TOPOLOGY_MODULE_H
#define UBSE_TOPOLOGY_MODULE_H

#include <sys/epoll.h>
#include <shared_mutex>
#include <vector>

#include "ubse_module.h"
#include "src/framework/context/ubse_context.h"
#include "ubse_error.h"
#include "ubse_lcne_topology.h"
#include "ubse_logger.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"

namespace ubse::mti {
using namespace ubse::module;
using namespace ubse::context;

constexpr uint32_t IPV6_FULL_FORMAT_LENGTH = 39;
constexpr uint32_t IPV6_BYTE_COUNT = 16;
constexpr size_t IPV6_SEGMENT_LENGTH = 4;

class UbseLcneModule : public UbseModule {
public:
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    /* *
     * @brief 获取LCNE提供的本节点信息
     * @param[out] ubseNodeInfo: 当前节点信息
     * @return 成功返回0, 失败返回非0
     */
    UbseResult UbseGetLocalNodeInfo(MtiNodeInfo &ubseNodeInfo);

    /* *
     * @brief 获取LCNE感知的集群节点信息
     * @param[out] ubseNodeInfos: 整个集群节点信息
     * @return 成功返回0, 失败返回非0
     */
    UbseResult UbseGetAllNodeInfos(std::vector<MtiNodeInfo> &ubseNodeInfos);

    UbseResult UbseGetDevTopology(adapter_plugins::mti::UbseDevTopology& devTopology);

    UbseLcneOSInfo GetUbseLcneOSInfo();

    UbseResult GetBondingEidByNodeId(std::string &bondingEid, const std::string &nodeId);

    const std::map<adapter_plugins::mti::UbseDevName, adapter_plugins::mti::UbseMtiEidGroup> GetAllSocketComEid();
    const std::map<adapter_plugins::mti::UbseDevName, UbseLcneIODieInfo> GetLocalBoardIOInfo();

    std::vector<std::string> GetClusterIpList();

    std::string GetClusterLocalIp();

private:
    // 内部功能函数
    UbseResult GetLcneConf();
    UbseResult ConvertPortConfStrToInt(const std::string &portStr, int &port);

    UbseResult GetLcneData();

    UbseResult GetComUrmaEid();

    UbseResult FillNodeComInfo();

    UbseResult GetIoDiePortEid(const adapter_plugins::mti::UbseDevName &devName, IODieInfo &ioDieInfo,
                               const std::map<std::string, std::string> &portEids);

    UbseResult GetTopologyInfo(std::map<std::string, std::vector<IODieInfo>> &allNodeIOdieInfo);

    static UbseResult ParseColonHexString(const std::string &input, char outBytes[IPV6_BYTE_COUNT]);

    static std::string BytesToIPv6String(const unsigned char inBytes[IPV6_BYTE_COUNT]);

    bool IsPrimaryEidExist(const std::string &nodeId);

    void UpdateClusterIpListAndLocalIp();

    // lcne获取的本节点拓扑信息（物理意义）
    UbseLcneTopology ubseLcneTopology;
    // 查询全量规划的urma通信EID（物理意义）
    std::map<adapter_plugins::mti::UbseDevName, adapter_plugins::mti::UbseMtiEidGroup>
        allSocketComEid;  // key为devName: nodeId+socketId 值为当前设备的UbseLcneSocketInfo
    // 查询节点信息（物理意义）
    std::map<adapter_plugins::mti::UbseDevName, UbseLcneIODieInfo>
        localBoardIOInfo;  // key为devName: nodeId+socketId 值为当前设备的UbseLcneIODieInfo

    // 查询Host信息（逻辑意义）
    UbseLcneOSInfo localBoardHostInfo;
    // 查询节点物理上bus instance信息（物理主机与逻辑）
    UbseLcneBusInstanceInfo ubseLcneBusInstanceInfo;

    MtiNodeInfo ubseNodeInfo_;               // 从lcne获取的本节点信息
    std::vector<MtiNodeInfo> ubseNodeInfos_; // 从lcne获取的集群节点信息
    std::string localIp;
    std::vector<std::string> clusterIpList;
    std::shared_mutex rw_mutex;
    UbseResult GenerateBondingEid(const std::string &nodeId, unsigned char *bondingEid);
    UbseResult FillTopoArray(std::map<std::string, std::vector<IODieInfo>> &allNodeIOdieInfo,
                             std::unique_ptr<TopoInfo[]> &topoArray);
};
} // namespace ubse::mti

#endif // UBSE_TOPOLOGY_MODULE_H
