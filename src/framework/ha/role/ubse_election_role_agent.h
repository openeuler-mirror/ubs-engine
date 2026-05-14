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

#ifndef UBSE_ELECTION_ROLE_AGENT_H
#define UBSE_ELECTION_ROLE_AGENT_H

#include "ubse_election_role.h"

namespace ubse::election {
class Agent : public ElectionRole {
public:
    explicit Agent(RoleContext& ctx);
    void ProcTimer() override;

    uint32_t RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt& reply) override;

    UBSE_ID_TYPE GetMasterNode() override;

    UBSE_ID_TYPE GetStandbyNode() override;

    std::vector<UBSE_ID_TYPE> GetAgentNodes() override;

    uint8_t GetMasterStatus() override;

    uint8_t GetStandbyStatus() override;

    void SetNodeDownStatus(UBSE_ID_TYPE nodeId) override{};

    RoleType GetRoleType() override
    {
        return RoleType::AGENT;
    }

    uint64_t GetTurnId() override
    {
        return turnId_;
    }

private:
    void HandleMasterChange(const ElectionPkt& rcvPkt, ElectionReplyPkt& reply);
    void RecvPktForSelect(ElectionReplyPkt& reply) const;
    void RecvPktForHeart(const ElectionPkt& rcvPkt, ElectionReplyPkt& reply);
    bool IsAgentHeartBeatTimeout(uint32_t heartbeatMultiplier) const;
    void DisconnectAgents(const ElectionPkt& rcvPkt);

private:
    uint64_t lastHeartTime_;
    UBSE_ID_TYPE masterId_;
    UBSE_ID_TYPE standbyId_;
    std::vector<UBSE_ID_TYPE> agentIds_{};
    UBSE_ID_TYPE myselfID_;
    uint64_t turnId_;
    uint64_t sequenceId_ = 0;
    uint8_t masterStatus_ = 0;
    uint8_t standbyStatus_ = 0;
    uint8_t flag_ = 0;
};
} // namespace ubse::election
#endif // UBSE_ELECTION_ROLE_AGENT_H
