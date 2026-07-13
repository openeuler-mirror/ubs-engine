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

#ifndef UBSE_MANAGER_UBSE_ELECTION_DEF_H
#define UBSE_MANAGER_UBSE_ELECTION_DEF_H

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ubse::election {
constexpr uint16_t UBSE_GLOBAL_PROC_INTERVAL = 1; // 单位秒
constexpr uint16_t UBSE_GLOBAL_COM_INTERVAL = 1; // 单位秒
constexpr uint16_t UBSE_GLOBAL_QUERY_LOCAL_MASTER_INTERVAL = 1; // 单位秒
constexpr uint16_t UBSE_GLOBAL_QUERY_COM_INTERVAL = 1; // 单位秒
constexpr uint16_t UBSE_QUERY_NODES_INTERVAL = 10; // 单位秒
constexpr const char *INVALID_NODE_ID = "";
constexpr const uint32_t DEFAULT_HEART_BEAT_TIME = 2000;
constexpr const uint32_t DEFAULT_HEART_BEAT_LOST = 3;
const uint32_t MAX_HEART_BEAT_LOST = 20;
constexpr const int IS_READY = 1;
constexpr const int NOT_READY = 0;
using UBSE_ID_TYPE = std::string;
constexpr const char* NODE_IP_NULL = "";
constexpr const uint16_t NODE_PORT_NULL = 0;
enum class RoleType {
    MASTER,
    STANDBY,
    AGENT,
    INITIALIZER,
};

enum class GlobalRoleType {
    GLOBAL_NONE,
    GLOBAL_INITIALIZER,
    GLOBAL_AGENT,
    GLOBAL_STANDBY,
    GLOBAL_MASTER
};

struct GroupSummaryInfo {
    UBSE_ID_TYPE groupId;
    UBSE_ID_TYPE groupMasterId;
    UBSE_ID_TYPE groupStandbyId;
};

enum class UbseElectionPktType {
    HEART_BEAT_PKT,
    BE_MASTER
};

enum class UbseNodeChangeState {
    INIT,
    UNCHANGED,
    ADD,
    DELETE,
};

struct NodeRoleInfo {
    UBSE_ID_TYPE nodeId;
    RoleType groupRole;
    GlobalRoleType globalRole;
};

struct GroupTopology {
    UBSE_ID_TYPE groupId;
    bool isManagingGroup; // 是否为管理组
    UBSE_ID_TYPE groupMasterId;
    UBSE_ID_TYPE groupStandbyId;
    std::vector<UBSE_ID_TYPE> groupNodes;
};

struct HaTopologyInfo {
    NodeRoleInfo currentNode;
    GroupTopology currentGroup; // 当前节点所在组信息
    std::vector<GroupTopology> groups; // 所有global组及挂载组节点信息
};

struct Node {
    UBSE_ID_TYPE id; // 节点ID
    std::string ip;
    uint16_t port;
    UbseNodeChangeState state = UbseNodeChangeState::UNCHANGED; // 节点变化Add, Delete, UnChanged

    bool operator < (const Node &other) const
    {
        return id < other.id;
    }

    bool operator > (const Node &other) const
    {
        return id > other.id;
    }

    bool operator == (const Node &other) const
    {
        return id == other.id;
    }
};
constexpr int ELECTION_PKT_TYPE_SELECT = 0;
constexpr int ELECTION_PKT_TYPE_HEART = 1;
constexpr int ELECTION_GROUP_INFO_TYPE_QUERY_LOCAL_MASTER = 2;
constexpr int ELECTION_PKT_TYPE_GLOBAL_SELECT = 3;
constexpr int ELECTION_PKT_TYPE_GLOBAL_HEART = 4;
constexpr int ELECTION_GROUP_INFO_TYPE_GLOBAL_CASCADE_REPORT = 5;

enum class NotifyStatus : uint8_t {
    NOT_BROADCAST = 0,
    BROADCAST = 1
};

enum class HeartBeatState : uint8_t {
    LOST = 0,
    ACTIVE = 1
};

struct BroadcastStatus {
    NotifyStatus masterOnlineBcStatus;
    uint32_t masterOnlineBcTimes;
    HeartBeatState activeStatus;
    uint32_t heartBeatLossCnt;

    static BroadcastStatus Default()
    {
        return {NotifyStatus::NOT_BROADCAST, 0, HeartBeatState::LOST, 0};
    }

    static BroadcastStatus WithActiveStatus()
    {
        return {NotifyStatus::NOT_BROADCAST, 0, HeartBeatState::ACTIVE, 0};
    }
};

enum class HeartBeatStatus : uint8_t {
    DISABLED = 0,
    ENABLED = 1
};

struct ElectionPkt {
    uint8_t type;
    UBSE_ID_TYPE masterId;
    std::string masterIp;
    UBSE_ID_TYPE standbyId;
    uint64_t turnId = 0;
    uint64_t sequenceId = 0;
    uint64_t agentCount = 0;
    std::vector<UBSE_ID_TYPE> agentIds;
    uint8_t masterStatus = NOT_READY;
    uint8_t standbyStatus = NOT_READY;
    uint8_t broadcast = static_cast<uint8_t>(NotifyStatus::NOT_BROADCAST);
    std::vector<UBSE_ID_TYPE> queryGroupNodeIds;
    UBSE_ID_TYPE globalMasterId; // global节点给自己组内的节点发组内心跳通知全局主节点ID
    UBSE_ID_TYPE globalStandbyId;
};

struct InterGroupInfo {
    uint8_t type;
    UBSE_ID_TYPE nodeId;
    UBSE_ID_TYPE groupId;
    UBSE_ID_TYPE groupMasterId;
    UBSE_ID_TYPE groupStandbyId;
    std::vector<UBSE_ID_TYPE> groupNodeIds;
    UBSE_ID_TYPE globalMasterId;
    UBSE_ID_TYPE globalStandbyId;
};

constexpr int ELECTION_PKT_RESULT_ACCEPT = 0;
constexpr int ELECTION_PKT_TYPE_REJECT = 1;
constexpr int ELECTION_PKT_TYPE_REJECT_HAS_MASTER = 2;
constexpr int ELECTION_PKT_REPLY_GLOBAL_STOP = 3;

struct ElectionReplyPkt {
    uint8_t type;
    UBSE_ID_TYPE replyId;
    UBSE_ID_TYPE groupId;
    uint32_t replyResult = ELECTION_PKT_RESULT_ACCEPT;
    UBSE_ID_TYPE masterId;
    UBSE_ID_TYPE standbyId;
    uint64_t turnId = 0;
    uint8_t standbyStatus = NOT_READY;
    uint8_t broadcast = static_cast<uint8_t>(NotifyStatus::NOT_BROADCAST);
    std::vector<UBSE_ID_TYPE> managingGroupNodeIds;
    std::vector<UBSE_ID_TYPE> cascadeGroupNodeIds;
    std::string cascadeGroupId;
    UBSE_ID_TYPE cascadeMasterId;
    UBSE_ID_TYPE cascadeStandbyId;
};

struct CallbackCtx {
    std::map<UBSE_ID_TYPE, BroadcastStatus> *broadcast;
    std::string destId{};
    uint8_t *standbyStatus;
    std::mutex *mtx = nullptr;
    std::atomic<bool> *stopping;
    std::atomic<int> *activeCount;
};

struct GlobalCallbackCtx {
    std::map<UBSE_ID_TYPE, BroadcastStatus> *broadcast;
    std::map<UBSE_ID_TYPE, GroupTopology> *globalStandbyAgentGroupTopologies;
    std::map<UBSE_ID_TYPE, GroupTopology> *globalCascadeGroupTopologies;
    std::string destId{};
    uint8_t *standbyStatus;
    std::mutex *mtx = nullptr;
    std::atomic<bool> *stopping;
    std::atomic<int> *activeCount;
};

struct CallbackQueryCtx {
    std::mutex *queryMtx = nullptr;
    std::unordered_map<UBSE_ID_TYPE, GroupSummaryInfo> *groupStates;
    std::string destId{};
};

struct CallbackCascadeCtx {
    std::string destId{};
    std::mutex *mtx = nullptr;
    std::string *globalMasterId;
    std::string *globalStandbyId;
    std::string *upstreamNextHopId;
    InterGroupInfo *managingGroupInfo;
};
} // namespace ubse::election
#endif // UBSE_MANAGER_UBSE_ELECTION_DEF_H