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

#ifndef UBS_ENGINE_UBSE_ELECTION_ROLE_GLOBAL_INITIALIZER_H
#define UBS_ENGINE_UBSE_ELECTION_ROLE_GLOBAL_INITIALIZER_H

#include "ubse_election_role.h"
namespace ubse::election {
class GlobalInitializer : public ElectionRole {
public:
    GlobalInitializer();
    ~GlobalInitializer();
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
        return GlobalRoleType::GLOBAL_INITIALIZER;
    }

    uint64_t GetTurnId() override
    {
        return globalTurnId_;
    }
private:
    void ProcRoleSwitch(const std::vector<Node> &masterIds);
    void CheckAndSwitchMaster(const Node &myself, const std::vector<Node> &allNodes, RoleContext ctx);
    void RegisterTimers();
private:
    uint64_t startTimeMs_;
    uint64_t lastTimeMs_;
    UBSE_ID_TYPE groupId_;
    UBSE_ID_TYPE myselfID_;
    uint64_t globalTurnId_ = 0;
    uint8_t masterStatus_ = 0;
    uint8_t standbyStatus_ = 0;
    bool hasConnMasterNodesOnce_ = false;
    bool isStartTimeSet_ = false;
};
}

#endif // UBS_ENGINE_UBSE_ELECTION_ROLE_GLOBAL_INITIALIZER_H
