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

    virtual uint32_t RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply) = 0;

    virtual void RecvInterGroupInfo(const InterGroupInfo &rcvInfo, InterGroupInfo &replyInfo);

    virtual UBSE_ID_TYPE GetMasterNode();

    virtual UBSE_ID_TYPE GetStandbyNode();

    virtual std::vector<UBSE_ID_TYPE> GetAgentNodes() = 0;

    virtual UBSE_ID_TYPE GetGlobalMasterNode();

    virtual UBSE_ID_TYPE GetGlobalStandbyNode();

    virtual RoleType GetRoleType() = 0;

    virtual GlobalRoleType GetGlobalRoleType()
    {
        return GlobalRoleType::GLOBAL_NONE;
    }

    virtual uint8_t GetMasterStatus() = 0;

    virtual uint8_t GetStandbyStatus() = 0;

    virtual void SetNodeDownStatus(UBSE_ID_TYPE nodeId) = 0;

    virtual void CleanupRoutes() {}

    virtual uint64_t GetTurnId() = 0;

    virtual InterGroupInfo GetCascadeGroupReport();

    virtual std::vector<GroupTopology> GetManagingGroupNodeIds();

    virtual std::vector<GroupTopology> GetCascadeGroupNodeIds();

    static uint32_t GetHeartTimeInterval()
    {
        return UbseElectionNodeMgr::GetInstance().GetHeartBeatTime();
    }

    static uint32_t GetHbLostTimes()
    {
        return UbseElectionNodeMgr::GetInstance().GetHeartBeatLost();
    }
};
UbseResult GetBootTime(uint64_t &bootTime);
UbseResult ConnectAllNodes();
UbseResult ConnectManagingMasters();
UBSE_ID_TYPE FindSmallestIdExcludingMaster(const UBSE_ID_TYPE &masterId, const std::vector<UBSE_ID_TYPE> &allNodes);
UBSE_ID_TYPE FindSmallestIdExcludingMasterAndAgent(const std::vector<UBSE_ID_TYPE> &allNodes,
    const UBSE_ID_TYPE &masterId, const UBSE_ID_TYPE &agentId);
bool IsSmallestNode(const Node &myself, const std::vector<Node> &allNodes);
bool IsSecondSmallestNode(const Node &myself, const std::vector<Node> &allNodes);
uint32_t SendElectionPkt(UBSE_ID_TYPE myselfID, std::string myselfIp);
uint32_t ForceElection(UBSE_ID_TYPE myselfID, std::string myselfIp);
bool GetElectionCandidate();
bool GetElectionWait();
uint32_t ElectWhenLowest(UBSE_ID_TYPE myselfID, std::vector<UBSE_ID_TYPE> allNodes);
uint32_t SendGlobalElectionPkt(UBSE_ID_TYPE myselfID);
void HandleGlobalMasterOnlineNotification(const ElectionPkt &rcvPkt, ElectionReplyPkt &reply);
void AcceptNewMaster(const ElectionPkt rcvPkt, ElectionReplyPkt &reply, const UBSE_ID_TYPE masterId);
} // namespace ubse::election

#endif // UBSE_ELECTION_ROLE_H