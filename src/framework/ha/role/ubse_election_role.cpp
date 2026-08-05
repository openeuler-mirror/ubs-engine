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
#include <algorithm>
#include <iostream>
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election_role_mgr.h"
#include "ubse_str_util.h"
#include "../ubse_election_node_mgr.h"

namespace ubse::election {
using namespace ubse::module;
using namespace ::ubse::common::def;
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::config;
using ::ubse::log::FormatRetCode;
constexpr size_t MIN_NODES_FOR_COMPARISON = 2;
constexpr size_t SECOND_SMALLEST_INDEX = 1;

namespace {
struct ElectionConfig {
    std::vector<std::string> candidateNodes;
    bool hasCandidateNodes = false;
    bool electionCandidate = true;
    bool electionWait = true;
};
ElectionConfig g_electionConfig;
} // anonymous namespace

UbseResult GetBootTime(uint64_t& bootTime)
{
    struct timespec ts {
    };
    if (clock_gettime(CLOCK_BOOTTIME, &ts) == 0) {
        uint64_t seconds = ts.tv_sec;
        uint64_t milliseconds = ts.tv_nsec / 1000000; // 纳秒转换为毫秒
        bootTime = (seconds * 1000) + milliseconds;   // 1000,转换单位
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

    for (const auto& it : allNodes) {
        if (it.ip != myselfNode.ip) {
            taskExecutor->Execute([it]() -> void { RoleMgr::GetInstance().GetCommMgr()->Connect(it.ip); });
        }
    }
    taskExecutor->Wait();
    UBSE_LOG_DEBUG << "[ELECTION] Wait over";
    return UBSE_OK;
}

UBSE_ID_TYPE FindSmallestId(const std::vector<UBSE_ID_TYPE>& allNodes)
{
    UBSE_ID_TYPE smallestId = INVALID_NODE_ID;

    for (const auto& nodeId : allNodes) {
        if (smallestId.empty() || nodeId < smallestId) {
            smallestId = nodeId;
        }
    }

    return smallestId;
}

bool IsSmallestNode(const Node& myself, const std::vector<Node>& allNodes)
{
    if (allNodes.size() < 2) { // 2,节点数小于2
        return true;
    }
    return std::all_of(allNodes.begin(), allNodes.end(),
                       [&myself](const Node& node) { return node.id.empty() || myself.id <= node.id; });
}

bool IsSecondSmallestNode(const Node& myself, const std::vector<Node>& allNodes)
{
    if (allNodes.size() < MIN_NODES_FOR_COMPARISON) {
        return true;
    }

    // 过滤掉 id 为空的节点
    std::vector<Node> validNodes;
    for (const auto& node : allNodes) {
        if (!node.id.empty()) {
            validNodes.push_back(node);
        }
    }

    if (validNodes.size() < MIN_NODES_FOR_COMPARISON) {
        return true;
    }

    std::partial_sort(validNodes.begin(), validNodes.begin() + MIN_NODES_FOR_COMPARISON, validNodes.end());

    // 检查第二个最小的节点是否是自己
    return validNodes[SECOND_SMALLEST_INDEX] == myself;
}

UBSE_ID_TYPE FindSmallestIdExcludingMaster(const UBSE_ID_TYPE& masterId, const std::vector<UBSE_ID_TYPE>& allNodes)
{
    UBSE_ID_TYPE smallestId = INVALID_NODE_ID;

    for (const auto& nodeId : allNodes) {
        if (nodeId != masterId) {
            if (smallestId.empty() || nodeId < smallestId) {
                smallestId = nodeId;
            }
        }
    }

    return smallestId;
}
UBSE_ID_TYPE FindSmallestIdExcludingMasterAndAgent(const std::vector<UBSE_ID_TYPE>& allNodes,
                                                   const UBSE_ID_TYPE& masterId, const UBSE_ID_TYPE& agentId)
{
    UBSE_ID_TYPE smallestId = INVALID_NODE_ID;

    for (const auto& nodeId : allNodes) {
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
    std::vector<std::string> allNodes = RoleMgr::GetInstance().GetCommMgr()->GetConnectedNodes();
    for (auto it : allNodes) {
        ElectionPkt pkt;
        ElectionReplyPkt reply;
        pkt.type = ELECTION_PKT_TYPE_SELECT;
        pkt.masterId = myselfID;
        auto ret = RoleMgr::GetInstance().GetCommMgr()->SendElectionPkt(it, pkt, reply);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "[ELECTION] SendElectionPkt to node=" << it << " failed or timeout, " << FormatRetCode(ret)
                          << ", treat as accept by default";
            continue;
        }
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

std::vector<std::string> ParseCandidateNodeList(const std::string& rawValue)
{
    std::vector<std::string> rawNodes;
    ubse::utils::Split(rawValue, ",", rawNodes);
    std::vector<std::string> candidateNodes;
    candidateNodes.reserve(rawNodes.size());
    for (auto& node : rawNodes) {
        auto trimmed = ubse::utils::Trim(node);
        if (!trimmed.empty()) {
            candidateNodes.emplace_back(std::move(trimmed));
        }
    }
    return candidateNodes;
}

bool GetElectionCandidateNodesFromConf(std::vector<std::string>& candidateNodes)
{
    if (g_electionConfig.hasCandidateNodes) {
        candidateNodes = g_electionConfig.candidateNodes;
        return true;
    }
    return false;
}

bool IsMyselfInCandidateNodes(const std::vector<std::string>& candidateNodes)
{
    Node myselfNode{};
    if (UBSE_ERROR == UbseElectionNodeMgr::GetInstance().GetMyselfNode(myselfNode)) {
        UBSE_LOG_ERROR << "[ELECTION] IsMyselfInCandidateNodes: GetMyselfNode failed.";
        return false;
    }
    return std::find(candidateNodes.begin(), candidateNodes.end(), myselfNode.id) != candidateNodes.end();
}

bool IsAllowedMasterNode(const UBSE_ID_TYPE& nodeId)
{
    std::vector<std::string> candidateNodes;
    if (!GetElectionCandidateNodesFromConf(candidateNodes)) {
        // 未配置候选主节点列表（或为空/非法），不限制主节点
        return true;
    }
    return std::find(candidateNodes.begin(), candidateNodes.end(), nodeId) != candidateNodes.end();
}

bool GetElectionCandidateConfig()
{
    return g_electionConfig.electionCandidate;
}

void InitElectionConfig()
{
    auto module = ubse::context::UbseContext::GetInstance().GetModule<ubse::config::UbseConfModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] InitElectionConfig: GetConfModule failed, using defaults.";
        return;
    }

    std::string rawValue;
    UbseResult ret = module->GetConf<std::string>(UBSE_ELECTION_SECTION, UBSE_ELECTION_CANDIDATE_NODES, rawValue);
    if (ret == UBSE_OK) {
        g_electionConfig.candidateNodes = ParseCandidateNodeList(rawValue);
        g_electionConfig.hasCandidateNodes = !g_electionConfig.candidateNodes.empty();
        if (g_electionConfig.candidateNodes.empty()) {
            UBSE_LOG_WARN << "[ELECTION] election.candidateNodes is empty or invalid, treated as not configured.";
        }
    }

    ret = module->GetConf<bool>(UBSE_ELECTION_SECTION, UBSE_ELECTION_CANDIDATE, g_electionConfig.electionCandidate);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] Get election.candidate failed, will set it true.";
        g_electionConfig.electionCandidate = true;
    }

    ret = module->GetConf<bool>(UBSE_ELECTION_SECTION, UBSE_ELECTION_WAIT, g_electionConfig.electionWait);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] Get election.wait failed, will set it true.";
        g_electionConfig.electionWait = true;
    }

    UBSE_LOG_INFO << "[ELECTION] Config loaded: candidateNodes="
                  << (g_electionConfig.hasCandidateNodes ? "configured" : "not configured")
                  << ", electionCandidate=" << g_electionConfig.electionCandidate
                  << ", electionWait=" << g_electionConfig.electionWait << ".";
}

bool GetElectionCandidate()
{
    std::vector<std::string> candidateNodes;
    if (GetElectionCandidateNodesFromConf(candidateNodes)) {
        // 配置了候选主节点列表，以该列表为准，election.candidate 开关不生效
        bool isCandidate = IsMyselfInCandidateNodes(candidateNodes);
        UBSE_LOG_DEBUG << "[ELECTION] election.candidateNodes takes effect, isCandidate=" << isCandidate << ".";
        return isCandidate;
    }
    // 未配置候选主节点列表（或配置为空/非法），回退到原 election.candidate 开关
    return GetElectionCandidateConfig();
}

bool GetElectionWait()
{
    return g_electionConfig.electionWait;
}

bool IsHeartBeatEnabled(HeartBeatStatus status)
{
    return status == HeartBeatStatus::ENABLED;
}

bool ElectionRole::IsCurrentRole() const
{
    return RoleMgr::GetInstance().GetRole().get() == this;
}
} // namespace ubse::election
