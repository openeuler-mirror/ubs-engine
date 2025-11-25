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

#ifndef UBSE_NODE_CONTROLLER_MODULE_H
#define UBSE_NODE_CONTROLLER_MODULE_H

#include "src/res_plugins/mti/ubse_lcne_module.h"
#include "ubse_context.h" // for context
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_module.h"
#include "ubse_node_controller.h"
#include "ubse_timer.h"
#include "ubse_topology_interface.h"

namespace ubse::nodeController {
using namespace ubse::mti;
using namespace ubse::context;
using namespace ubse::log;
using namespace ubse::module;
using namespace ubse::timer;
using namespace ubse::election;

class UbseNodeControllerModule : public UbseModule {
public:
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    static UbseResult CollectWhenLcneChange(std::string &eventId, std::string &eventMessage);

private:
    /* *
     * 监听主节点上线处理函数，启动周期对账+拓扑采集
     * @param nodeId [in] 主节点id
     * @return
     */
    UbseResult MasterOnlineHandler(const std::string &nodeId);

    /* *
     * 节点上线处理函数，对单个节点立刻触发一次对账+拓扑采集
     * @param nodeId [in] 上线节点id
     */
    void NodeUpHandler(const std::string &nodeId);

    /* *
     * 监听节点下线处理函数，将节点状态置为unknown
     * @param nodeId [in] 下线节点id
     */
    void NodeDownHandler(const std::string &nodeId);

    /* *
     * 订阅故障事件，将节点状态置为fault;
     * @param nodeId [in] 故障节点id
     */
    void NodeFaultHandler(const std::string &nodeId);

    /* *
     * 主上线对账
     * @param currentNode [in] 当前节点id
     */
    void MasterOnLineClusterCollector();

    void CycleCollectLedger();

    void CollectLedger(const std::string &nodeId);

    void ClusterCollectNodeTopology();

    uint32_t UbseNodeGetLinkUpNodes(std::vector<UbseRoleInfo> &roleInfos);

    void CollectNode(const std::string &nodeId);

    void CycleCollectNode(const std::string &nodeId);

    void StartExec();

private:
    UbseTimer localCollectTimer{};        // 节点侧 numa 采集定时器
    UbseTimer clusterCollectTimer{};      // 中心侧 对账 定时器
    UbseTimer clusterNodeTopologyTimer{}; // 中心侧 节点拓扑采集定时器
    std::shared_mutex rwMutex;
};
} // namespace ubse::nodeController
#endif // UBSE_NODE_CONTROLLER_MODULE_H
