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

#ifndef UBS_ENGINE_UBSE_ELECTION_ROLE_GLOBAL_AGENT_H
#define UBS_ENGINE_UBSE_ELECTION_ROLE_GLOBAL_AGENT_H

#include "ubse_election_role.h"
namespace ubse::election {
class GlobalAgent : public ElectionRole {
public:
    explicit GlobalAgent(RoleContext &ctx);
    ~GlobalAgent();
    void ProcTimer() override;

    uint32_t RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply) override;

    void RecvInterGroupInfo(const InterGroupInfo &rcvInfo, InterGroupInfo &replyInfo) override;

    UBSE_ID_TYPE GetGlobalMasterNode() override;

    UBSE_ID_TYPE GetGlobalStandbyNode() override;

    std::vector<UBSE_ID_TYPE> GetAgentNodes() override;

    uint8_t GetMasterStatus() override;

    uint8_t GetStandbyStatus() override;

    InterGroupInfo GetCascadeGroupReport() override;

    void SetNodeDownStatus(UBSE_ID_TYPE nodeId) override
    {
        nodeId = INVALID_NODE_ID;
    };

    RoleType GetRoleType() override
    {
        return RoleType::MASTER;
    }

    GlobalRoleType GetGlobalRoleType() override
    {
        return GlobalRoleType::GLOBAL_AGENT;
    }

    uint64_t GetTurnId() override
    {
        return globalTurnId_;
    }

private:
    void HandleMasterChange(const ElectionPkt &rcvPkt, ElectionReplyPkt &reply);
    void RecvPktForSelect(ElectionReplyPkt &reply) const;
    void RecvPktForHeart(const ElectionPkt &rcvPkt, ElectionReplyPkt &reply);
    bool IsAgentHeartBeatTimeout(uint32_t heartbeatMultiplier) const;
    void DisconnectAgents(const ElectionPkt &rcvPkt);
private:
    uint64_t lastHeartTime_;
    UBSE_ID_TYPE globalMasterId_;
    UBSE_ID_TYPE globalStandbyId_;
    std::vector<UBSE_ID_TYPE> globalAgentIds_{};
    std::string groupId_{};
    UBSE_ID_TYPE myselfID_;
    uint64_t globalTurnId_;
    uint8_t masterStatus_ = 0;
    uint8_t standbyStatus_ = 0;
    InterGroupInfo cascadeGroupReport_;
};
}
#endif // UBS_ENGINE_UBSE_ELECTION_ROLE_GLOBAL_AGENT_H
