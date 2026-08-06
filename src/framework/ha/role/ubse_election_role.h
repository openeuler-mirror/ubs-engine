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

#ifndef UBSE_ELECTION_ROLE_H
#define UBSE_ELECTION_ROLE_H
#include <mutex>
#include <vector>
#include "../ubse_election_comm_mgr.h"
#include "../ubse_election_def.h"
#include "../ubse_election_node_mgr.h"

namespace ubse::election {

struct RoleContext {
    uint64_t turnId;
    UBSE_ID_TYPE masterId;
    UBSE_ID_TYPE standbyId;
};

class ElectionRole {
public:
    virtual void ProcTimer() = 0;

    virtual uint32_t RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt& reply) = 0;

    virtual UBSE_ID_TYPE GetMasterNode() = 0;

    virtual UBSE_ID_TYPE GetStandbyNode() = 0;

    virtual std::vector<UBSE_ID_TYPE> GetAgentNodes() = 0;

    virtual RoleType GetRoleType() = 0;

    virtual uint8_t GetMasterStatus() = 0;

    virtual uint8_t GetStandbyStatus() = 0;

    virtual void SetNodeDownStatus(UBSE_ID_TYPE nodeId) = 0;

    virtual uint64_t GetTurnId() = 0;

    static uint32_t GetHeartTimeInterval()
    {
        return UbseElectionNodeMgr::GetInstance().GetHeartBeatTime();
    }

    static uint32_t GetHbLostTimes()
    {
        return UbseElectionNodeMgr::GetInstance().GetHeartBeatLost();
    }

protected:
    // 当前对象是否仍是 RoleMgr 的当前角色。
    // RoleMgr::ProcTimer/RecvPkt 已改为锁外执行，并发 SwitchRole 后 this 可能指向陈旧角色对象，
    // 任何可能产生切换/发送副作用的方法（ProcTimer、TrySwitchToMaster 等）入口都需先校验。
    bool IsCurrentRole() const;
    // 角色内部互斥锁：串行化同角色上的 ProcTimer / RecvPkt / 状态读写。
    // 注意：持锁期间禁止做阻塞的网络等待，网络发送必须在锁外进行。
    mutable std::mutex roleMutex_;
};
UbseResult GetBootTime(uint64_t& bootTime);
UbseResult ConnectAllNodes();
UBSE_ID_TYPE FindSmallestIdExcludingMaster(const UBSE_ID_TYPE& masterId, const std::vector<UBSE_ID_TYPE>& allNodes);
UBSE_ID_TYPE FindSmallestIdExcludingMasterAndAgent(const std::vector<UBSE_ID_TYPE>& allNodes,
                                                   const UBSE_ID_TYPE& masterId, const UBSE_ID_TYPE& agentId);
bool IsSmallestNode(const Node& myself, const std::vector<Node>& allNodes);
bool IsSecondSmallestNode(const Node& myself, const std::vector<Node>& allNodes);
uint32_t SendElectionPkt(UBSE_ID_TYPE myselfID);
uint32_t ForceElection(UBSE_ID_TYPE myselfID);
bool GetElectionCandidate();
bool GetElectionWait();
bool IsHeartBeatEnabled(HeartBeatStatus status);

// 将候选主节点列表配置字符串（逗号分隔）解析为去空白、非空的节点ID列表
std::vector<std::string> ParseCandidateNodeList(const std::string& rawValue);
// 读取候选主节点列表配置，返回true表示配置存在且解析得到合法非空列表（以该列表为准），
// 返回false表示未配置/解析非法/为空（应回退到 election.candidate 开关）
bool GetElectionCandidateNodesFromConf(std::vector<std::string>& candidateNodes);
// 判断本节点ID是否在候选主节点列表中
bool IsMyselfInCandidateNodes(const std::vector<std::string>& candidateNodes);
// 判断给定节点是否被允许作为主节点（候选主节点列表已配置时受限，未配置则不限制）
bool IsAllowedMasterNode(const UBSE_ID_TYPE& nodeId);
// 读取原 election.candidate 开关配置
bool GetElectionCandidateConfig();
// 在HA模块初始化时加载所有选举配置到内存缓存，后续读取直接从缓存获取
void InitElectionConfig();
} // namespace ubse::election

#endif // UBSE_ELECTION_ROLE_H
