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

#ifndef UBSE_ELECTION_ROLE_INITIALIZER_H
#define UBSE_ELECTION_ROLE_INITIALIZER_H

#include <pthread.h>
#include "ubse_election_role.h"
#include "../ubse_election_def.h"

namespace ubse::election {
class Initializer : public ElectionRole {
public:
    Initializer();
    ~Initializer();
    void ProcTimer() override;

    uint32_t RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt& reply) override;

    UBSE_ID_TYPE GetMasterNode() override;

    UBSE_ID_TYPE GetStandbyNode() override;

    std::vector<UBSE_ID_TYPE> GetAgentNodes() override;

    uint8_t GetMasterStatus() override;

    uint8_t GetStandbyStatus() override;

    void SetNodeDownStatus(UBSE_ID_TYPE) override{};

    RoleType GetRoleType() override
    {
        return RoleType::INITIALIZER;
    }

    uint64_t GetTurnId() override
    {
        return turnId_;
    }

private:
    void ProcRoleSwitch();
    void CheckAndSwitchMaster(const Node& myself, const std::vector<Node>& allNodes, RoleContext ctx);

private:
    uint64_t startTimeMs_;
    uint64_t lastTimeMs_;
    UBSE_ID_TYPE masterId_;
    UBSE_ID_TYPE myselfID_;
    uint64_t turnId_ = 0;
    uint8_t masterStatus_ = 0;
    uint8_t standbyStatus_ = 0;
    bool isStartTimeSet_ = false;
};
} // namespace ubse::election
#endif // UBSE_ELECTION_ROLE_INITIALIZER_H
