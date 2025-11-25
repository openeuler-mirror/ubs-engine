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
    explicit Agent(RoleContext &ctx);
    void ProcTimer() override;

    uint32_t RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply) override;

    UBSE_ID_TYPE GetMasterNode() override;

    UBSE_ID_TYPE GetStandbyNode() override;

    std::vector<UBSE_ID_TYPE> GetAgentNodes() override;

    uint8_t GetMasterStatus() override;

    uint8_t GetStandbyStatus() override;

    void SetNodeDownStatus(UBSE_ID_TYPE nodeId) override
    {
        nodeId = INVALID_NODE_ID;
    };

    RoleType GetRoleType() override
    {
        return RoleType::AGENT;
    }

    uint64_t GetTurnId() override
    {
        return turnId;
    }

private:
    void HandleMasterChange(const ElectionPkt &rcvPkt, ElectionReplyPkt &reply);
    void RecvPktForSelect(ElectionReplyPkt &reply) const;
    void RecvPktForHeart(const ElectionPkt &rcvPkt, ElectionReplyPkt &reply);
    bool IsAgentHeartBeatTimeout(uint32_t heartbeatMultiplier) const;
private:
    uint64_t lastHeartTime;
    UBSE_ID_TYPE masterId;
    UBSE_ID_TYPE standbyId;
    std::vector<UBSE_ID_TYPE> agentIds{};
    UBSE_ID_TYPE myselfID;
    uint64_t turnId;
    uint64_t sequenceId = 0;
    uint8_t masterStatus = 0;
    uint8_t standbyStatus = 0;
    uint8_t flag = 0;
};
}
#endif // UBSE_ELECTION_ROLE_AGENT_H
