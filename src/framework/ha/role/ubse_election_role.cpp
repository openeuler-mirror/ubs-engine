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

#include "ubse_election_role.h"
#include <iostream>
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election_role_mgr.h"

namespace ubse::election {
using namespace ubse::config;
UbseResult GetBootTime(uint64_t &bootTime)
{
    struct timespec ts{};
    if (clock_gettime(CLOCK_BOOTTIME, &ts) == 0) {
        uint64_t seconds = ts.tv_sec;
        uint64_t milliseconds = ts.tv_nsec / 1000000; // 纳秒转换为毫秒
        bootTime = (seconds * 1000) + milliseconds; // 1000,转换单位
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

UbseResult ConnectAllNodes()
{
    Node myselfNode{};
    if (UBSE_ERROR == UbseElectionNodeMgr::GetInstance().GetMyselfNode(myselfNode)) {
        UBSE_LOG_ERROR << "[ELECTION] GetMyselfNode: no node found.";
        return UBSE_ERROR;
    }

    std::vector<Node> allNodes{};
    UbseResult result = UbseElectionNodeMgr::GetInstance().GetAllNode(allNodes);
    if (result == UBSE_ERROR) {
        UBSE_LOG_ERROR << "[ELECTION] GetAllNode: no node found.";
        return UBSE_ERROR;
    }

    auto taskExecutorModule =
        ubse::context::UbseContext::GetInstance().GetModule<ubse::task_executor::UbseTaskExecutorModule>();
    if (taskExecutorModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    auto taskExecutor = taskExecutorModule->Get(ELECTION_TASK_EXECUTOR_NAME);
    if (taskExecutor == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }

    for (const auto &it : allNodes) {
        if (it.id != myselfNode.id) {
            taskExecutor->Execute([it]() -> void { RoleMgr::GetInstance().GetCommMgr()->Connect(it.id); });
        }
    }
    taskExecutor->Wait();
    UBSE_LOG_DEBUG << "[ELECTION] Wait over";
    return UBSE_OK;
}

UBSE_ID_TYPE FindSmallestId(const std::vector<UBSE_ID_TYPE> &allNodes)
{
    UBSE_ID_TYPE smallestId = INVALID_NODE_ID;

    for (const auto &nodeId : allNodes) {
        if (smallestId.empty() || nodeId < smallestId) {
            smallestId = nodeId;
        }
    }

    return smallestId;
}

bool IsSmallestNode(const Node &myself, const std::vector<Node> &allNodes)
{
    if (allNodes.size() < 2) { // 2,节点数小于2
        return true;
    }

    return std::all_of(allNodes.begin(), allNodes.end(), [&myself](const Node &node) { return myself.id <= node.id; });
}

bool IsSecondSmallestNode(const Node &myself, const std::vector<Node> &allNodes)
{
    if (allNodes.size() < 2) { // 节点数小于2
        return true;
    }

    // 找到两个最小的节点
    std::vector<Node> sortedNodes = allNodes;
    std::sort(sortedNodes.begin(), sortedNodes.end());

    return sortedNodes[1] == myself;
}

UBSE_ID_TYPE FindSmallestIdExcludingMaster(const UBSE_ID_TYPE &masterId, const std::vector<UBSE_ID_TYPE> &allNodes)
{
    UBSE_ID_TYPE smallestId = INVALID_NODE_ID;

    for (const auto &nodeId : allNodes) {
        if (nodeId != masterId) {
            if (smallestId.empty() || nodeId < smallestId) {
                smallestId = nodeId;
            }
        }
    }

    return smallestId;
}
UBSE_ID_TYPE FindSmallestIdExcludingMasterAndAgent(const std::vector<UBSE_ID_TYPE> &allNodes,
    const UBSE_ID_TYPE &masterId, const UBSE_ID_TYPE &agentId)
{
    UBSE_ID_TYPE smallestId = INVALID_NODE_ID;

    for (const auto &nodeId : allNodes) {
        if (nodeId != masterId && nodeId != agentId) {
            if (smallestId == INVALID_NODE_ID || nodeId < smallestId) {
                smallestId = nodeId;
            }
        }
    }

    return smallestId;
}

uint32_t SendElectionPkt(UBSE_ID_TYPE myselfID)
{
    std::vector<UBSE_ID_TYPE> allNodes;

    allNodes = RoleMgr::GetInstance().GetCommMgr()->GetConnectedNodes();
    for (auto it : allNodes) {
        ElectionPkt pkt;
        ElectionReplyPkt reply;
        pkt.type = ELECTION_PKT_TYPE_SELECT;
        pkt.masterId = myselfID;
        RoleMgr::GetInstance().GetCommMgr()->SendElectionPkt(it, pkt, reply);
        if (reply.replyResult == ELECTION_PKT_RESULT_ACCEPT) {
            continue;
        } else if (reply.replyResult == ELECTION_PKT_TYPE_REJECT) {
            return ELECTION_PKT_TYPE_REJECT;
        } else if (reply.replyResult == ELECTION_PKT_TYPE_REJECT_HAS_MASTER) {
            return ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
        }
    }
    return ELECTION_PKT_RESULT_ACCEPT;
}

uint32_t ForceElection(UBSE_ID_TYPE myselfID)
{
    std::vector<UBSE_ID_TYPE> allNodes = RoleMgr::GetInstance().GetCommMgr()->GetConnectedNodes();
    UBSE_LOG_INFO << "[ELECTION] Initializer: ForceElection: SIZE " << allNodes.size() << ".";
    if (allNodes.empty()) {
        UBSE_LOG_INFO << "[ELECTION] Initializer: allNodes.empty "
                    << ".";
        return ELECTION_PKT_RESULT_ACCEPT;
    }

    UBSE_ID_TYPE smallestId = FindSmallestId(allNodes);
    UBSE_LOG_INFO << "[ELECTION] Initializer: ForceElection:  " << smallestId << ".";
    if (smallestId >= myselfID) {
        if (SendElectionPkt(myselfID) == ELECTION_PKT_RESULT_ACCEPT) {
            return ELECTION_PKT_RESULT_ACCEPT;
        }
    }
    return ELECTION_PKT_TYPE_REJECT;
}

bool IsHeartBeatEnabled(HeartBeatStatus status)
{
    return status == HeartBeatStatus::ENABLED;
}
}
