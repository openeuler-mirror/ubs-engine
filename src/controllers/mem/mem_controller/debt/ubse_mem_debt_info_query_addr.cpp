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

#include "ubs_engine_topo.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_agent_task_manager.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_mem_debt_info.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_node_controller_query_api.h"

namespace ubse::mem::controller::debt {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::election;
using namespace ubse::log;
uint32_t UbseMemAddrGet(const UbseMemDebtQueryRequest &request, UbseMemAddrDesc &desc)
{
    // 获取当前节点
    UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
        return ret;
    }
    if (currentRoleInfo.nodeRole != ELECTION_ROLE_MASTER) {
        UBSE_LOG_ERROR << "current role is not master. nodeId: " << currentRoleInfo.nodeId;
        return UBSE_ERR_INTERNAL;
    }
    const std::string name = request.name;
    const std::string importNodeId = request.importNodeId;
    NodeMemDebtInfo debtInfo = GetNodeMemDebtInfoById(importNodeId);

    auto iterator = debtInfo.addrImportObjMap.find(name);
    if (iterator == debtInfo.addrImportObjMap.end()) {
        UBSE_LOG_ERROR << "Failed to find no delete debt, name: " << name << "related node id: " << importNodeId;
        return UBSE_ERR_NOT_EXIST;
    }

    // 找到目标对象，提取信息
    auto importObj = iterator->second;

    desc.name = name;
    if (importObj.status.importResults.empty()) {
        return UBSE_ERROR;
    }
    desc.numaId = importObj.status.importResults[0].numaId;
    // 填充导入信息
    ubse::nodeController::def::UbseNode importNode;
    ubse::nodeController::UbseNodeGetByNodeId(importNodeId, importNode);
    desc.importNode.slotId = importNode.slotId;
    desc.importNode.hostName = importNode.hostName;
    desc.importNode.socketIdList.clear();
    for (int i = 0; i < UBS_TOPO_SOCKET_NUM; i++) {
        desc.importNode.socketIdList.push_back(static_cast<int16_t>(importNode.socketId[i]));
    }
    // 填充导出信息
    desc.lender.pid = importObj.req.exportPid;
    desc.lender.slotId = static_cast<uint32_t>(std::stoul(importObj.req.exportNodeId));
    desc.lender.socketId = importObj.req.dstSocket;
    for (auto val : importObj.req.exportAddrList) {
        desc.lender.vaLists.push_back({val.addr, val.size});
    }
    for (auto val : importObj.algoResult.exportNumaInfos) {
        desc.size += val.size;
    }
    return UBSE_OK; // 根据查找结果返回
}

UbseMemResult GetAddrStageByObj(const std::string &name, const std::string &importNodeId,
                                const NodeMemDebtInfoMap &debtInfoMap)
{
    UbseMemAddrBorrowImportObj addrImportObj{};
    UbseMemAddrBorrowExportObj addrExportObj{};
    bool importObjExist = false;
    bool exportObjExist = false;
    UbseMemResult result{name};
    for (auto [nodeId, nodeInfo] : debtInfoMap) {
        if (nodeInfo.addrImportObjMap.find(name) == nodeInfo.addrImportObjMap.end()) {
            continue;
        }
        // 节点级唯一、多个node可能有同一个name
        if (nodeId != importNodeId) {
            continue;
        }
        addrImportObj = nodeInfo.addrImportObjMap[name];
        importObjExist = true;
        break;
    }
    auto exportKey = GenerateExportObjKey(name, importNodeId);
    for (auto [nodeId, nodeInfo] : debtInfoMap) {
        if (nodeInfo.addrExportObjMap.find(exportKey) == nodeInfo.addrExportObjMap.end()) {
            continue;
        }
        addrExportObj = nodeInfo.addrExportObjMap[exportKey];
        exportObjExist = true;
        break;
    }
    result.importNodeId = importNodeId;
    if (!importObjExist && !exportObjExist) {
        result.name = name;
        result.stage = UbseMemStage::UBSE_NOT_EXIST;
        return result;
    }

    for (auto addrInfo : addrExportObj.req.exportAddrList) {
        result.realSize += addrInfo.size;
    }
    if (!importObjExist && exportObjExist) {
        result.stage = UbseMemStage::UBSE_ERR_WAIT_UNEXPORT;
        return result;
    }

    result.stage = GetOptStageByObjState(addrImportObj, exportObjExist);
    return result;
}

UbseMemAddrBorrowExportObj UbseAddrExportObjGet(const std::string &nodeId, const std::string &name,
                                                const std::string &importNodeId, const bool isFromTaskManager)
{
    UbseMemAddrBorrowExportObj obj{};
    if (isFromTaskManager) {
        const auto ret = UbseMemAgentTaskManager::GetTaskObj(name, importNodeId, obj);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "name=" << name << ", importNodeId=" << importNodeId << " is not in task manager.";
        } else {
            return obj;
        }
    }

    const auto map = GetNodeMemDebtInfoMap();
    const auto nodeIter = map.find(nodeId);
    if (nodeIter == map.end()) {
        return {};
    }
    const auto objIter = nodeIter->second.addrExportObjMap.find(GenerateExportObjKey(name, importNodeId));
    if (objIter == nodeIter->second.addrExportObjMap.end()) {
        UBSE_LOG_WARN << "name=" << name << ", importNodeId=" << importNodeId << " is not in debt.";
        return {};
    }
    return objIter->second;
}

UbseMemAddrBorrowImportObj UbseAddrImportObjGet(const std::string &nodeId, const std::string &name,
                                                const bool isFromTaskManager)
{
    UbseMemAddrBorrowImportObj obj{};
    if (isFromTaskManager) {
        const auto ret = UbseMemAgentTaskManager::GetTaskObj(name, "", obj);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "name=" << name << " is not in task manager.";
        } else {
            return obj;
        }
    }

    const auto map = GetNodeMemDebtInfoMap();
    const auto nodeIter = map.find(nodeId);
    if (nodeIter == map.end()) {
        return {};
    }
    const auto objIter = nodeIter->second.addrImportObjMap.find(name);
    if (objIter == nodeIter->second.addrImportObjMap.end()) {
        UBSE_LOG_WARN << "name=" << name << " is not in debt.";
        return {};
    }
    return objIter->second;
}
} // namespace ubse::mem::controller::debt