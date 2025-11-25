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

#ifndef UBSE_ELECTION_ROLE_MASTER_H
#define UBSE_ELECTION_ROLE_MASTER_H

#include "ubse_election_role.h"

namespace ubse::election {
class Master : public ElectionRole {
public:
    explicit Master(RoleContext &ctx);

    void ProcTimer() override;

    uint32_t RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply) override;
    uint32_t RecvPktHeart(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply);
    uint32_t RecvPktElection(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply);

    UBSE_ID_TYPE GetMasterNode() override;

    UBSE_ID_TYPE GetStandbyNode() override;

    std::vector<UBSE_ID_TYPE> GetAgentNodes() override;

    RoleType GetRoleType() override
    {
        return RoleType::MASTER;
    }

    uint64_t GetTurnId() override
    {
        return turnId;
    }

    uint8_t GetMasterStatus() override;

    uint8_t GetStandbyStatus() override;

    void SetNodeDownStatus(UBSE_ID_TYPE nodeId) override;
    void DealBroadcast(ElectionReplyPkt &reply, const UBSE_ID_TYPE &id);
    void DealHbCnt(const UBSE_ID_TYPE &id);

    /* *
     * 处理节点上下线的通知
     * @param allNodes [in] 现在连接的节点信息
     */
    void DealNodeUpdate();
private:
    void PrepareElectionPkt(ElectionPkt &pkt);
    void UpdateStandbyNode(ElectionPkt &pkt, ElectionReplyPkt &reply);
    void ReplaceStandbyNode(ElectionPkt &pkt);
    void HandleSplitBrainMerge(const ElectionPkt rcvPkt, ElectionReplyPkt &reply);
    void GetAllNeighbourNode(std::vector<UBSE_ID_TYPE> &allNodes);
    std::vector<UBSE_ID_TYPE> GetAllAgentIDs();
    std::vector<UBSE_ID_TYPE> GetActiveNodes();
    void InitNodesStatus(const std::vector<UBSE_ID_TYPE> &allNodes);
private:
    UBSE_ID_TYPE masterId; // Master的masterId也是selfID
    UBSE_ID_TYPE standbyId = INVALID_NODE_ID;
    uint64_t lastTimeMs = 0;
    uint64_t turnId;
    uint64_t sequenceId;
    uint8_t workStatus;
    uint8_t standbyStatus = 0;
    HeartBeatStatus heartBeatStatus = HeartBeatStatus::ENABLED;
    std::map<UBSE_ID_TYPE, BroadcastStatus> broadcast_;
    std::vector<UBSE_ID_TYPE> preNodes_{};
    std::mutex mtx;  // 互斥锁
};
}
#endif // UBSE_ELECTION_ROLE_MASTER_H
