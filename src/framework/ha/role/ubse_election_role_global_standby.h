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

#ifndef UBS_ENGINE_UBSE_ELECTION_ROLE_GLOBAL_STANDBY_H
#define UBS_ENGINE_UBSE_ELECTION_ROLE_GLOBAL_STANDBY_H

#include "ubse_election_role.h"
namespace ubse::election {
class GlobalStandby : public ElectionRole {
public:
    explicit GlobalStandby(RoleContext &ctx);
    ~GlobalStandby();
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
        return RoleType::MASTER;
    }

    GlobalRoleType GetGlobalRoleType() override
    {
        return GlobalRoleType::GLOBAL_STANDBY;
    }

    uint64_t GetTurnId() override
    {
        return globalTurnId_;
    }

private:
    uint64_t lastHeartTime_;
    UBSE_ID_TYPE globalMasterId_{};
    UBSE_ID_TYPE globalStandbyId_{};
    std::vector<UBSE_ID_TYPE> globalAgentIds_{};
    uint64_t globalTurnId_;
    uint8_t globalMasterStatus_ = 0;
    void RecvPktForHeart(const ElectionPkt &rcvPkt, ElectionReplyPkt &reply);
    bool IsStandbyHeartBeatTimeout(uint32_t heartbeatMultiplier) const;
    void RegisterTimers();
    void FillGroupRoleInfo(ElectionReplyPkt &reply);
};
}

#endif // UBS_ENGINE_UBSE_ELECTION_ROLE_GLOBAL_STANDBY_H
