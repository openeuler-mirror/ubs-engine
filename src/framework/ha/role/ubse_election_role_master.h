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
#define MODULE_LOG_NAME "ubse"
class Master : public ElectionRole {
public:
    explicit Master(RoleContext &ctx);

    ~Master()
    {
        stopping_ = true;
        // 等待所有回调结束
        while (activeCount_.load() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        UBSE_LOG_INFO <<"[ELECTION] Master destruction completed";
    }

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
        return turnId_;
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
    UbseResult SendHeartBeat(UBSE_ID_TYPE destID, const ElectionPkt &pkt);

private:
    UBSE_ID_TYPE masterId_; // Master的masterId也是selfID
    UBSE_ID_TYPE standbyId_ = INVALID_NODE_ID;
    uint64_t lastTimeMs_ = 0;
    uint64_t turnId_;
    uint64_t sequenceId_;
    uint8_t workStatus_;
    uint8_t standbyStatus_ = 0;
    HeartBeatStatus heartBeatStatus_ = HeartBeatStatus::ENABLED;
    std::map<UBSE_ID_TYPE, BroadcastStatus> broadcast_;
    std::vector<UBSE_ID_TYPE> preNodes_{};
    std::mutex mtx_;  // 互斥锁
    std::atomic<bool> stopping_;     // Master 是否正在销毁
    std::atomic<int> activeCount_;   // 当前活跃回调数量
};
#undef MODULE_LOG_NAME
}
#endif // UBSE_ELECTION_ROLE_MASTER_H
