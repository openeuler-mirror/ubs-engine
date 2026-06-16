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

#include "ubse_error.h"
#include "ubse_lcne_topology.h"
#include "ubse_logger.h"
#include "ubse_module.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"
#include "src/framework/context/ubse_context.h"
#include "adapter_plugins/mti/ubse_mti_def.h"

namespace ubse::mti {
using ubse::module::UbseModule;
using namespace ubse::adapter_plugins::mti;

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
    UbseResult UbseGetLocalNodeInfo(UbseMtiNodeInfo &ubseNodeInfo);

    /* *
     * @brief 获取LCNE感知的集群节点信息
     * @param[out] ubseNodeInfos: 整个集群节点信息
     * @return 成功返回0, 失败返回非0
     */
    UbseResult UbseGetAllNodeInfos(std::vector<UbseMtiNodeInfo> &ubseNodeInfos);

    UbseResult UbseGetDevTopology(adapter_plugins::mti::UbseDevTopology& devTopology);

    UbseLcneOSInfo GetUbseLcneOSInfo();

    std::string GetCurSlotId();

    const std::map<adapter_plugins::mti::UbseMtiIouInfo, adapter_plugins::mti::UbseMtiEidGroup> GetMtiComEid();
    const std::map<adapter_plugins::mti::UbseMtiIouInfo, UbseLcneIODieInfo> GetLocalBoardIOInfo();

    std::vector<std::string> GetClusterIpList();

    std::string GetClusterLocalIp();

private:
    // 内部功能函数
    UbseResult GetLcneConf();
    UbseResult ConvertPortConfStrToInt(const std::string& portStr, int& port);

    UbseResult GetLcneData();

    UbseResult GetComUrmaEid();

    UbseResult FillNodeComInfo();

    bool IsPrimaryEidExist(const std::string &slotId);

    void UpdateClusterIpListAndLocalIp();

    // lcne获取的本节点拓扑信息（物理意义）
    UbseLcneTopology ubseLcneTopology;
    // 查询全量规划的urma通信EID（物理意义）
    std::map<adapter_plugins::mti::UbseMtiIouInfo, adapter_plugins::mti::UbseMtiEidGroup> allSocketComEid;
    // 查询节点信息（物理意义）
    std::map<adapter_plugins::mti::UbseMtiIouInfo, UbseLcneIODieInfo> localBoardIOInfo;

    // 查询Host信息（逻辑意义）
    UbseLcneOSInfo localBoardHostInfo;
    // 查询节点物理上bus instance信息（物理主机与逻辑）
    UbseLcneBusInstanceInfo ubseLcneBusInstanceInfo;

    UbseMtiNodeInfo ubseNodeInfo_;               // 从lcne获取的本节点信息
    std::vector<UbseMtiNodeInfo> ubseNodeInfos_; // fullmesh从lcne获取的集群节点信息,clos组网仅本节点信息
    std::string localIp;
    std::vector<std::string> clusterIpList;
    std::shared_mutex rw_mutex;
};
} // namespace ubse::mti

#endif // UBSE_TOPOLOGY_MODULE_H
