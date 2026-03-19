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

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>

namespace ubse::election {
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
    INITIALIZER
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
constexpr uint8_t NEED_SWITCH_OVER = 1;
constexpr uint8_t NO_SWITCH_OVER = 0;

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
    // 0：选主报文，1：心跳报文
    uint8_t type;
    // 选主报文中：表示自己的 id，心跳报文中：表示主节点的 id
    UBSE_ID_TYPE masterId;
    UBSE_ID_TYPE standbyId;
    uint64_t turnId = 0;
    uint64_t sequenceId = 0;
    uint64_t agentCount = 0;
    std::vector<UBSE_ID_TYPE> agentIds;
    uint8_t masterStatus = NOT_READY;
    uint8_t standbyStatus = NOT_READY;
    uint8_t broadcast = static_cast<uint8_t>(NotifyStatus::NOT_BROADCAST);
    // 保留字段
    uint8_t rev1;
    uint16_t rsv2;
};

constexpr int ELECTION_PKT_RESULT_ACCEPT = 0;
constexpr int ELECTION_PKT_TYPE_REJECT = 1;
constexpr int ELECTION_PKT_TYPE_REJECT_HAS_MASTER = 2;
constexpr int ELECTION_PKT_REPLY_GLOBAL_STOP = 3;

struct ElectionReplyPkt {
    // 0：选主报文，1：心跳报文
    uint8_t type;
    UBSE_ID_TYPE replyId;
    // 接受或者拒绝
    uint32_t replyResult = ELECTION_PKT_RESULT_ACCEPT; // 0 接收 1 拒绝 2 拒绝且有主
    // 拒绝的话，relay认为的主ID
    UBSE_ID_TYPE masterId;
    uint64_t turnId = 0;
    // 备节点状态
    uint8_t standbyStatus = NOT_READY;
    // 主节点上线通知状态
    uint8_t broadcast = static_cast<uint8_t>(NotifyStatus::NOT_BROADCAST);
    // 保留字段
    uint16_t rsv;
    uint8_t length;
};

struct CallbackCtx {
    std::map<UBSE_ID_TYPE, BroadcastStatus> *broadcast;
    std::string destId{};
    uint8_t *standbyStatus;
    std::mutex *mtx = nullptr;
    std::atomic<bool> *stopping;
    std::atomic<int> *activeCount;
};
} // namespace ubse::election
#endif // UBSE_MANAGER_UBSE_ELECTION_DEF_H
