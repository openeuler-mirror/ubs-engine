/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBS_ENGINE_UBSE_ELECTION_ROLE_GLOBAL_MASTER_H
#define UBS_ENGINE_UBSE_ELECTION_ROLE_GLOBAL_MASTER_H

#include "ubse_election_role.h"
#include "ubse_timer.h"

namespace ubse::election {
using namespace ubse::timer;
#define MODULE_LOG_NAME "ubse"
class GlobalMaster : public ElectionRole {
public:
    explicit GlobalMaster(RoleContext &ctx);

    ~GlobalMaster();

    void ProcTimer() override;

    uint32_t RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply) override;

    void RecvInterGroupInfo(const InterGroupInfo &rcvInfo, InterGroupInfo &replyInfo) override;

    UBSE_ID_TYPE GetGlobalMasterNode() override;

    UBSE_ID_TYPE GetGlobalStandbyNode() override;

    std::vector<UBSE_ID_TYPE> GetAgentNodes() override;

    RoleType GetRoleType() override;

    GlobalRoleType GetGlobalRoleType() override;

    uint64_t GetTurnId() override;

    uint8_t GetMasterStatus() override;

    uint8_t GetStandbyStatus() override;

    void SetNodeDownStatus(UBSE_ID_TYPE nodeId) override;

    InterGroupInfo GetCascadeGroupReport() override;

    std::vector<GroupTopology> GetManagingGroupNodeIds() override;

    std::vector<GroupTopology> GetCascadeGroupNodeIds() override;


private:
    uint32_t RecvPktHeart(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply);
    uint32_t RecvPktElection(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply);
    void DealHbCnt(const UBSE_ID_TYPE &id);
    void DealNodeUpdate();
    void PrepareHeartBeatPkt(ElectionPkt &pkt);
    void ReplaceStandbyNode(ElectionPkt &pkt);
    void HandleSplitBrainMerge(const ElectionPkt rcvPkt, ElectionReplyPkt &reply);
    std::vector<UBSE_ID_TYPE> GetAllGlobalAgentIds() const;
    std::vector<UBSE_ID_TYPE> GetActiveNodes();
    void InitNodesStatus();
    UbseResult SendGlobalHeartBeat(UBSE_ID_TYPE destID, const ElectionPkt &pkt);
    void DetectCascadeGroupTimeout();

private:
    UBSE_ID_TYPE nodeId_{}; // 当前节点id = globalMasterId
    std::string groupId_{};
    UBSE_ID_TYPE globalStandbyId_{};
    std::vector<UBSE_ID_TYPE> globalStandbyAgentNodes_{};
    uint64_t lastTimeMs_ = 0;
    uint64_t lastCascadeReportTime_ = 0;
    uint64_t globalTurnId_{};
    uint8_t globalStandbyStatus_ = 0;
    std::map<UBSE_ID_TYPE, BroadcastStatus> globalStandbyAgentBroadcast_{};
    std::mutex mtx_{};  // 互斥锁
    std::atomic<bool> stopping_{};     // Master 是否正在销毁
    std::atomic<int> activeCount_{};   // 当前活跃回调数量
    InterGroupInfo cascadeGroupReport_{};
    std::map<UBSE_ID_TYPE, GroupTopology> globalStandbyAgentGroupTopologies_{};
    std::map<UBSE_ID_TYPE, GroupTopology> globalCascadeGroupTopologies_{};
    void InitManagingToCascadeNodeIds();
    void AddDownstreamGroupRoute(const UBSE_ID_TYPE &groupId, const UBSE_ID_TYPE &dstNodeId,
                                  const UBSE_ID_TYPE &nextHopNodeId);
    void DeleteDownstreamGroupRoute(const UBSE_ID_TYPE &groupId);
    void DeleteAllDownstreamGroupRoutes();
    void CleanupRoutes() override;
    std::map<UBSE_ID_TYPE, RouteEntry> downstreamRouteEntries_;
    std::map<UBSE_ID_TYPE, UBSE_ID_TYPE> managingToCascadeNodeId_;
};
#undef MODULE_LOG_NAME
}
#endif // UBS_ENGINE_UBSE_ELECTION_ROLE_GLOBAL_MASTER_H