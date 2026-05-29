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

#ifndef UBSE_ELECTION_ROLE_MGR_H
#define UBSE_ELECTION_ROLE_MGR_H

#include <iostream>
#include <memory>
#include <mutex>
#include <utility>
#include "ubse_com_module.h"
#include "ubse_election_module.h"
#include "ubse_election_role.h"
#include "ubse_election_role_agent.h"
#include "ubse_election_role_initializer.h"
#include "ubse_election_role_master.h"
#include "ubse_election_role_standby.h"
#include "../ubse_election_comm_mgr.h"
#include "../ubse_election_def.h"
#include "../ubse_election_node_mgr.h"

namespace ubse::election {
#define MODULE_LOG_NAME "ubse"
class RoleMgr {
public:
    RoleMgr()
    {
        UbseElectionNodeMgr& nodeMgr = UbseElectionNodeMgr::GetInstance();
        Node myself;
        nodeMgr.GetMyselfNode(myself);
        currentRole_ = SafeMakeShared<Initializer>();
        if (!currentRole_) {
            UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared Initializer currentRole failed.";
        }
        commMgr_ = SafeMakeShared<UbseElectionCommMgr>(myself.id, "UbseMasterRpcServer");
        if (!commMgr_) {
            UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared Initializer commMgr failed.";
        }
    };

    static RoleMgr& GetInstance()
    {
        static RoleMgr roleMgr;
        return roleMgr;
    }

    std::shared_ptr<ElectionRole> GetRole();

    void SwitchRole(RoleType roleType, RoleContext& ctx);

    std::shared_ptr<UbseElectionCommMgr> GetCommMgr()
    {
        return commMgr_;
    };

    uint32_t RoleChangeAttach(UbseElectionEventType type, UbseElectionHandler handler);

    uint32_t RoleChangeDeAttach(UbseElectionEventType type, UbseElectionHandler handler);

    uint32_t RoleChangeNotify(UbseElectionEventType type, UBSE_ID_TYPE newId);

    void RoleChangeNotifyAsync(UbseElectionEventType type, UBSE_ID_TYPE newId);

    uint32_t RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt& rcvPkt, ElectionReplyPkt& reply);
    void ProcTimer();

private:
    static std::shared_ptr<RoleMgr> instance_;
    std::shared_ptr<ElectionRole> currentRole_;
    std::shared_ptr<UbseElectionCommMgr> commMgr_;
    // 使用 shared_ptr 存储 Handler
    using HandlerPtr = std::shared_ptr<UbseElectionHandler>;
    using HandlerWeakPtr = std::weak_ptr<UbseElectionHandler>;

    // 安全句柄结构
    struct SafeHandler {
        HandlerWeakPtr weak_handler;
        std::string name;
        UbseElectionHandlerPriority priority;
        int sequenceId;
        std::mutex mutex;

        SafeHandler(HandlerWeakPtr weak_handler, std::string name, UbseElectionHandlerPriority priority, int sequenceId)
            : weak_handler(std::move(weak_handler)),
              name(std::move(name)),
              priority(priority),
              sequenceId(sequenceId)
        {
        }

        bool operator<(const SafeHandler& other) const
        {
            if (priority != other.priority) {
                return priority < other.priority;
            }
            return sequenceId < other.sequenceId;
        }
    };
    std::vector<HandlerPtr> active_handlers_;
    std::unordered_map<UbseElectionEventType, std::vector<std::shared_ptr<SafeHandler>>> handlers_;
    std::recursive_mutex mProcessorLock_{};
    std::mutex mutex_;
};
#undef MODULE_LOG_NAME
} // namespace ubse::election
#endif // UBSE_ELECTION_ROLE_MGR_H
