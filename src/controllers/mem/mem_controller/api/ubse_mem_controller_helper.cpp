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

#include "ubse_mem_controller_helper.h"
#include <securec.h>
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_mem_util.h"
#include "ubse_request_id_util.h"
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_election_def.h"
#include "ubse_election_module.h"
#include "ubse_logger.h"
#include "role/ubse_election_role_mgr.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::election;
using namespace ubse::context;
using ubse::common::def::UbseResult;

const uint8_t UBSE_MAX_LENDER_CNT = 2;
const uint32_t UBSE_MIN_MEM_SIZE = 4 * 1024 * 1024; // 128 MB

UbseResult UbseMemCreateWithLenderReqIsValid(const std::string &name, const UbseMemBorrower &borrower,
                                             const std::vector<UbseMemNumaLender> &lenders)
{
    if (!util::CheckName(name)) {
        return UBSE_ERR_INVALID_ARG;
    }
    if (borrower.nodeId.empty()) {
        UBSE_LOG_ERROR << "borrower nodeId empty";
        return UBSE_ERR_INVALID_ARG;
    }
    if (lenders.empty() || lenders.size() > UBSE_MAX_LENDER_CNT) {
        UBSE_LOG_ERROR << "Invalid lenders cnt: " << lenders.size();
        return UBSE_ERR_INVALID_ARG;
    }
    for (auto lender : lenders) {
        if (lender.size < UBSE_MIN_MEM_SIZE) {
            UBSE_LOG_ERROR << "Invalid lender size: " << lender.size;
            return UBSE_ERR_INVALID_ARG;
        }
    }
    return UBSE_OK;
}

static std::string GetCurNodeId()
{
    ubse::election::UbseRoleInfo currentRoleInfo{};
    UbseGetCurrentNodeInfo(currentRoleInfo); // 内部记录是否获取 ID 失败
    return currentRoleInfo.nodeId;
}

static uint64_t GenRequestId()
{
    uint8_t slotId = 0;
    ubse::election::UbseRoleInfo roleInfo{};
    auto ret = ubse::election::UbseGetCurrentNodeInfo(roleInfo);
    if (ret == UBSE_OK) {
        uint64_t nodeId = std::stoul(roleInfo.nodeId);
        slotId = static_cast<uint8_t>(nodeId);
    }
    static auto requestIdUtil = UbseRequestIdUtil(ubse::utils::UbseRequestType::INNER_REQUEST);
    return requestIdUtil.GenerateRequestId(slotId);
}

UbseResult ConvertUbseMemNumaCreateWithLenderReq(const std::string &name, const UbseMemBorrower &borrower,
                                                 const std::vector<UbseMemNumaLender> &lenders,
                                                 uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN],
                                                 UbseMemNumaBorrowReq &numaBorrowReq)
{
    numaBorrowReq.name = name;
    numaBorrowReq.requestNodeId = GetCurNodeId();
    numaBorrowReq.importNodeId = borrower.nodeId;
    for (auto lender : lenders) {
        numaBorrowReq.lenderLocs.push_back({std::to_string(lender.slotId), static_cast<uint32_t>(lender.numaId)});
        numaBorrowReq.lenderSizes.push_back(lender.size);
        numaBorrowReq.size += lender.size;
    }
    numaBorrowReq.srcSocket = borrower.affinitySocketId;
    numaBorrowReq.udsInfo.gid = 0;
    numaBorrowReq.udsInfo.uid = borrower.uid;
    numaBorrowReq.udsInfo.pid = 0;
    numaBorrowReq.udsInfo.username = borrower.username;
    numaBorrowReq.requestId = GenRequestId();
    if (memcpy_s(numaBorrowReq.usrInfo, UBSE_MAX_USR_INFO_LEN, usrInfo, UBSE_MAX_USR_INFO_LEN) != UBSE_OK) {
        UBSE_LOG_ERROR << "MemCopy fail when copy usrInfo, name is " << name;
        return UBSE_ERR_INVALID_ARG;
    }
    return UBSE_OK;
}

UbseResult UbseMemCreateReqIsValid(const std::string &name, const UbseMemBorrower &borrower,
                                   const UbseMemNumaCreateOpt &opt)
{
    if (!util::CheckName(name)) {
        return UBSE_ERR_INVALID_ARG;
    }
    if (borrower.nodeId.empty()) {
        UBSE_LOG_ERROR << "borrower nodeId empty";
        return UBSE_ERR_INVALID_ARG;
    }
    if (opt.size < UBSE_MIN_MEM_SIZE) {
        UBSE_LOG_ERROR << "Invalid size: " << opt.size;
        return UBSE_ERR_INVALID_ARG;
    }
    return UBSE_OK;
}

UbseResult ConvertUbseMemNumaCreateReq(const std::string &name, const UbseMemBorrower &borrower,
                                       const UbseMemNumaCreateOpt &opt, UbseMemNumaBorrowReq &numaBorrowReq)
{
    numaBorrowReq.name = name;
    numaBorrowReq.requestNodeId = GetCurNodeId();
    numaBorrowReq.importNodeId = borrower.nodeId;
    numaBorrowReq.size = opt.size;
    numaBorrowReq.distance = static_cast<ubse::adapter_plugins::mmi::UbseMemDistance>(opt.distance);
    numaBorrowReq.srcSocket = borrower.affinitySocketId;
    numaBorrowReq.highWatermark = opt.highWatermark;
    numaBorrowReq.udsInfo.gid = 0;
    numaBorrowReq.udsInfo.uid = borrower.uid;
    numaBorrowReq.udsInfo.pid = 0;
    numaBorrowReq.udsInfo.username = borrower.username;
    numaBorrowReq.requestId = GenRequestId();
    if (memcpy_s(numaBorrowReq.usrInfo, UBSE_MAX_USR_INFO_LEN, opt.usrInfo, UBSE_MAX_USR_INFO_LEN) != UBSE_OK) {
        UBSE_LOG_ERROR << "MemCopy fail when copy usrInfo, name is " << name;
        return UBSE_ERR_INVALID_ARG;
    }
    return UBSE_OK;
}

UbseResult UbseMemCreateWithCandidateReqIsValid(const std::string &name, const UbseMemBorrower &borrower,
                                                const UbseMemNumaCandidateOpt &opt)
{
    if (!util::CheckName(name)) {
        return UBSE_ERR_INVALID_ARG;
    }
    if (borrower.nodeId.empty()) {
        UBSE_LOG_ERROR << "borrower nodeId empty";
        return UBSE_ERR_INVALID_ARG;
    }
    if (opt.size < UBSE_MIN_MEM_SIZE) {
        UBSE_LOG_ERROR << "Invalid size: " << opt.size;
        return UBSE_ERR_INVALID_ARG;
    }
    if (opt.slotIds.empty()) {
        UBSE_LOG_ERROR << "Invalid slotIds size: " << opt.slotIds.size();
        return UBSE_ERR_INVALID_ARG;
    }
    return UBSE_OK;
}

UbseResult ConvertUbseMemNumaCreateWithCandidateReq(const std::string &name, const UbseMemBorrower &borrower,
                                                    const UbseMemNumaCandidateOpt &opt,
                                                    UbseMemNumaBorrowReq &numaBorrowReq)
{
    numaBorrowReq.name = name;
    numaBorrowReq.requestNodeId = GetCurNodeId();
    numaBorrowReq.importNodeId = borrower.nodeId;
    numaBorrowReq.size = opt.size;
    numaBorrowReq.distance = static_cast<ubse::adapter_plugins::mmi::UbseMemDistance>(opt.distance);
    numaBorrowReq.srcSocket = borrower.affinitySocketId;
    numaBorrowReq.highWatermark = opt.highWatermark;
    numaBorrowReq.udsInfo.gid = 0;
    numaBorrowReq.udsInfo.uid = borrower.uid;
    numaBorrowReq.udsInfo.pid = 0;
    numaBorrowReq.udsInfo.username = borrower.username;
    numaBorrowReq.requestId = GenRequestId();
    if (memcpy_s(numaBorrowReq.usrInfo, UBSE_MAX_USR_INFO_LEN, opt.usrInfo, UBSE_MAX_USR_INFO_LEN) != UBSE_OK) {
        UBSE_LOG_ERROR << "MemCopy fail when copy usrInfo, name is " << name;
        return UBSE_ERR_INVALID_ARG;
    }
    numaBorrowReq.candidateNodeList = opt.slotIds;
    return UBSE_OK;
}

UbseResult UbseMemDeleteReqIsValid(const std::string &name, const UbseMemBorrower &borrower)
{
    if (!util::CheckName(name)) {
        return UBSE_ERR_INVALID_ARG;
    }
    if (borrower.nodeId.empty()) {
        UBSE_LOG_ERROR << "borrower nodeId empty";
        return UBSE_ERR_INVALID_ARG;
    }
    return UBSE_OK;
}

void ConvertUbseMemDeleteReq(const std::string &name, const UbseMemBorrower &borrower, UbseMemReturnReq &returnReq)
{
    returnReq.name = name;
    returnReq.requestNodeId = GetCurNodeId();
    returnReq.requestId = GenRequestId();
    returnReq.importNodeId = borrower.nodeId;
}

UbseResult UbseMemAddrCreateReqIsValid(const std::string& name, const UbseMemBorrower& borrower,
                                       const UbseMemProcessLender& lender)
{
    if (!util::CheckName(name)) {
        return UBSE_ERR_INVALID_ARG;
    }
    if (borrower.nodeId.empty()) {
        UBSE_LOG_ERROR << "borrower nodeId empty";
        return UBSE_ERR_INVALID_ARG;
    }
    if (lender.pid == 0) {
        UBSE_LOG_ERROR << "lender pid equal 0";
        return UBSE_ERR_INVALID_ARG;
    }
    for (auto val : lender.vaLists) {
        if (val.size < UBSE_MIN_MEM_SIZE) {
            UBSE_LOG_ERROR << "Invalid size: " << val.size;
            return UBSE_ERR_INVALID_ARG;
        }
    }
    return UBSE_OK;
}

void ConvertUbseMemAddrCreateReq(const std::string &name, const UbseMemBorrower &borrower,
                                 const UbseMemProcessLender &lender, uint32_t flag, uint8_t exportAccessMode,
                                 UbseMemAddrBorrowReq &addrBorrowReq)
{
    addrBorrowReq.name = name;
    addrBorrowReq.requestNodeId = GetCurNodeId();
    addrBorrowReq.importNodeId = borrower.nodeId;
    addrBorrowReq.srcSocket = borrower.affinitySocketId;
    addrBorrowReq.exportNodeId = std::to_string(lender.slotId);
    addrBorrowReq.dstSocket = lender.socketId;
    addrBorrowReq.exportPid = lender.pid;
    for (auto val : lender.vaLists) {
        addrBorrowReq.exportAddrList.push_back({val.addr, val.size});
    }
    addrBorrowReq.requestId = GenRequestId();
    addrBorrowReq.wrDelayComp = flag;
    if (exportAccessMode != 0 && exportAccessMode != 1) {
        UBSE_LOG_WARN << "Invalid exportAccessMode: " << exportAccessMode << ", set exportAccessMode = 0";
        addrBorrowReq.exportAccessMode = 0;
    } else {
        addrBorrowReq.exportAccessMode = exportAccessMode;
    }
}

UbseResult UbseGetGlobalMasterNodeId(std::string &globalMasterNodeId)
{
    auto &ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "election module is not loaded";
        return UBSE_ERROR;
    }
    Node masterInfo{};
    auto ret = electionModule->UbseGetMasterNode(masterInfo);
    if (ret != UBSE_OK || masterInfo.id.empty()) {
        UBSE_LOG_ERROR << "Failed to get global master node id, ret=" << ret;
        return UBSE_ERROR;
    }
    globalMasterNodeId = masterInfo.id;
    return UBSE_OK;
}

// 在全局主拓扑中查找agent节点的级联主节点ID
UbseResult UbseGetCascadeMasterNodeIdByAgentNodeId(const std::string &agentNodeId, std::string &cascadeMasterNodeId)
{
    if (agentNodeId.empty()) {
        UBSE_LOG_ERROR << "agentNodeId is empty";
        return UBSE_ERROR;
    }
    auto &ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "election module is not loaded";
        return UBSE_ERROR;
    }
    HaTopologyInfo topoInfo{};
    if (electionModule->GetCurNodeGlobalTopoInfo(topoInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "get current node global topo info failed";
        return UBSE_ERROR;
    }
    auto &curGroup = topoInfo.currentGroup;
    if (std::find(curGroup.groupNodes.begin(), curGroup.groupNodes.end(), agentNodeId) != curGroup.groupNodes.end()) {
        cascadeMasterNodeId = curGroup.groupMasterId;
        return UBSE_OK;
    }
    for (const auto &group : topoInfo.groups) {
        if (std::find(group.groupNodes.begin(), group.groupNodes.end(), agentNodeId) != group.groupNodes.end()) {
            cascadeMasterNodeId = group.groupMasterId;
            return UBSE_OK;
        }
    }
    UBSE_LOG_ERROR << "cascade master for agent node not found in topology, agentNodeId=" << agentNodeId;
    return UBSE_ERROR;
}

UbseResult UbseGetCurManagerMasterNodeId(std::string &managerMasterNodeId)
{
    auto &ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "election module is not loaded";
        return UBSE_ERROR;
    }
    HaTopologyInfo topoInfo{};
    if (electionModule->GetCurNodeGlobalTopoInfo(topoInfo) != UBSE_OK) {
        return UBSE_ERROR;
    }
    if (topoInfo.currentGroup.isManagingGroup) {
        managerMasterNodeId = topoInfo.currentGroup.groupMasterId;
        return UBSE_OK;
    }
    for (auto &group : topoInfo.groups) {
        if (group.isManagingGroup) {
            managerMasterNodeId = group.groupMasterId;
            return UBSE_OK;
        }
    }
    UBSE_LOG_ERROR << "manager master node not found in topology";
    return UBSE_ERROR;
}

// 管理组调用，检查 unImportNodeId 是否在 cascadeMasterNodeId 管理的级联域内
bool UbseDetachNodeIdInCascadeDomain(const std::string &unImportNodeId, const std::string &cascadeMasterNodeId)
{
    if (unImportNodeId.empty() || cascadeMasterNodeId.empty()) {
        UBSE_LOG_ERROR << "invalid input, unImportNodeId=" << unImportNodeId
                       << ", cascadeMasterNodeId=" << cascadeMasterNodeId;
        return false;
    }
    auto &ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "election module is not loaded";
        return false;
    }
    HaTopologyInfo topoInfo{};
    if (electionModule->GetCurNodeGlobalTopoInfo(topoInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "get topology failed";
        return false;
    }
    if (topoInfo.currentNode.nodeId == cascadeMasterNodeId) {
        bool found = std::find(topoInfo.currentGroup.groupNodes.begin(),
        topoInfo.currentGroup.groupNodes.end(), unImportNodeId) != topoInfo.currentGroup.groupNodes.end();
        return found;
    }
    for (auto &group : topoInfo.groups) {
        if (group.groupMasterId == cascadeMasterNodeId) {
            bool found = std::find(group.groupNodes.begin(), group.groupNodes.end(), unImportNodeId) != group.groupNodes.end();
            return found;
        }
    }
    std::stringstream ss;
    for (auto &group : topoInfo.groups) {
        ss << "groupMasterId=" << group.groupMasterId << ", nodes=[";
        for (const auto &node : group.groupNodes) {
            ss << node << ", ";
        }
        ss << "]";
    }
    UBSE_LOG_ERROR << "cascadeMaster not found in topology, groups=" << ss.str() << ", unImportNodeId=" << unImportNodeId
                       << ", cascadeMasterNodeId=" << cascadeMasterNodeId;
    return false;
}

// 在全局主上面调用，检查 unImportNodeId 是否在manageMasterNodeId管理的机柜中
bool UbseCheckDetachNodeIdInManageDomain(const std::string &unImportNodeId, const std::string &manageMasterNodeId)
{
    if (unImportNodeId.empty() || manageMasterNodeId.empty()) {
        UBSE_LOG_ERROR << "invalid input, unImportNodeId=" << unImportNodeId
                       << ", manageMasterNodeId=" << manageMasterNodeId;
        return false;
    }
    auto &ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "election module is not loaded";
        return false;
    }
    HaTopologyInfo topoInfo{};
    if (electionModule->GetCurNodeGlobalTopoInfo(topoInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "get current node global topo info failed";
        return false;
    }
    auto cascadeStaticGroupInfo = nodeMgr::GetUbseNodeById(unImportNodeId);
    auto manageStaticGroupInfo = nodeMgr::GetUbseNodeById(manageMasterNodeId);
    if (cascadeStaticGroupInfo.nodeId.empty() || manageStaticGroupInfo.nodeId.empty()) {
        UBSE_LOG_ERROR << "node not found in topology, cascadeStaticGroupInfo.nodeId=" << cascadeStaticGroupInfo.nodeId
                       << ", manageStaticGroupInfo.nodeId=" << manageStaticGroupInfo.nodeId;
        return false;
    }
    if (cascadeStaticGroupInfo.groupId == manageStaticGroupInfo.groupId) {
        return true;
    }
    auto nodesMap = nodeMgr::GetAllNodesStoredByGroup();
    uint16_t totalGroupCount = nodesMap.size();
    uint16_t managingGroupCount = 0;
    if (totalGroupCount % NO_2 != 0) {
        UBSE_LOG_ERROR << "Invalid totalGroupCount=" << totalGroupCount
                       << ", expected even number. Please check cluster configuration.";
        return false;
    }
    managingGroupCount = totalGroupCount / NO_2;
    if (cascadeStaticGroupInfo.groupId > manageStaticGroupInfo.groupId &&
        cascadeStaticGroupInfo.groupId - manageStaticGroupInfo.groupId == managingGroupCount) {
        return true;
    }
    UBSE_LOG_ERROR << "groupIdDiff=" << (cascadeStaticGroupInfo.groupId - manageStaticGroupInfo.groupId)
                   << ", managingGroupCount=" << managingGroupCount
                   << ", no match";
    return false;
}

bool UbseCheckWithoutGlobalMasterNodeId()
{
    auto &ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "election module is not loaded";
        return false;
    }
    HaTopologyInfo topoInfo;
    if (electionModule->GetCurNodeGlobalTopoInfo(topoInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "get global topo info failed";
        return false;
    }
    if (topoInfo.currentNode.groupRole == RoleType::MASTER && topoInfo.currentNode.globalRole == GlobalRoleType::GLOBAL_NONE) {
        // 满足此条件说明当前没有全局主节点, 且当前节点是主节点
        return true;
    }
    UBSE_LOG_ERROR << "current node is not master node, global role=" << static_cast<int>(topoInfo.currentNode.globalRole)
                   << ", group role=" << static_cast<int>(topoInfo.currentNode.groupRole);
    return false;
}
} // namespace ubse::mem::controller
