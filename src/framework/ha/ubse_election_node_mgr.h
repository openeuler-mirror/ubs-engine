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

#ifndef UBSE_ELECTION_NODE_MGR_H
#define UBSE_ELECTION_NODE_MGR_H

#include <string>
#include <vector>

#include "ubse_com_module.h"
#include "ubse_election_def.h"
#include "ubse_node_controller.h"
#include "ubse_str_util.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"

namespace ubse::election {
class UbseElectionNodeMgr {
public:
    /* *
     * @brief 获取 Node 单例实例
     */
    static UbseElectionNodeMgr &GetInstance();

    // 禁用拷贝和赋值
    UbseElectionNodeMgr(const UbseElectionNodeMgr &) = delete;
    UbseElectionNodeMgr &operator=(const UbseElectionNodeMgr &) = delete;

    /* *
     * @brief 构造函数
     * @param configFile 配置文件路径
     */
    UbseElectionNodeMgr();

    /* *
     * @brief 从配置文件中加载配置
     * @return true 加载成功，false 加载失败
     */
    UbseResult LoadConfig();

    /* *
     * @brief 从 LCNE 获取本节点的信息
     * @param Node 用于存储当前节点的信息
     * @return 成功返回0，不成功返回非 0
     */
    UbseResult GetMyselfNode(Node &myself);

    static ubse::nodeController::UbseNodeLocalState GetLocalNodeState();
    /* *
     * @brief 从 节点发现 获取节点的状态信息
     * 单层选主：当前所有节点
     * 两级选主：当前组所有节点
     * @param allNodes 用于存储节点的信息
     * @return 成功返回0，不成功返回非0
     */
    UbseResult GetAllNode(std::vector<Node> &allNodes);

    /* *
     * 获取所有邻居节点
     * 单层选主：当前所有邻居节点
     * 两级选主：当前组所有邻居节点
     * @param neighbourNodes 存储所有邻居节点的信息
     * @return 成功返回0
     */
    UbseResult GetAllNeighbourNode(std::vector<Node> &neighbourNodes);

    std::unordered_set<UBSE_ID_TYPE> GetTopoLinkedNodes() const;

    void ParseAllNodesVector();

    UbseResult GetNodeInfoByID(const UBSE_ID_TYPE &id, std::string &ip, uint16_t &port);

    UbseResult UpdateNodeIdWithConnect(const std::string &ip, const std::string &id);

    UbseResult GetNodeIdByIp(const std::string &ip, std::string &id);

    UbseResult GetNodeIpById(const std::string &id, std::string &ip);

    UbseResult GetNodeIpMap(std::unordered_map<std::string, UBSE_ID_TYPE> &nodeIpMap);

    /* *
     * 获取心跳时间
     * @return uint32_t 间隔时间
     */
    uint32_t GetHeartBeatTime() const;

    /* *
     * 获取备节点心跳丢失次数阈值
     * @return uint32_t 丢失次数
     */
    uint32_t GetHeartBeatLost() const;

    // 获取组内节点
    UbseResult GetGroupNodes(std::vector<Node> &groupNodes);
    // 获取建链节点的groupId
    UbseResult GetGroupIdByNodeId(const std::string &nodeId, std::string &groupId);
    // 获取当前节点的groupId
    UbseResult GetGroupId(std::string &groupId);
    UbseResult GetUBEnable(bool &ubEnable);
    bool IsHierarchicalElection() const;

private:
    // 本地节点信息
    Node currentNode_;

    UBSE_ID_TYPE master_;
    UBSE_ID_TYPE standby_;
    bool isHierarchicalElection_ = false; // 是否分层选举
    bool rootEnable_ = true;
    bool ubEnable_ = true;
    // 心跳时间
    uint32_t heartBeatTime_;
    // 备节点丢失心跳阈值
    uint32_t heartBeatLost_;

    // 当前所有节点信息和上一次所有节点信息
    std::vector<Node> currentAllNodes_; // 单层：当前所有节点；双层：当前组所有节点
    std::vector<Node> lastAllNodes_;    // 上一次所有节点的
    std::unordered_map<std::string, UBSE_ID_TYPE> nodeIpMap_;
    mutable std::shared_mutex mtx_{}; // 读写锁
};
} // namespace ubse::election
#endif // UBSE_ELECTION_NODE_MGR_H