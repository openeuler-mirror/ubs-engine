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
#include "src/controllers/mem/mem_controller/ubse_mem_controller_query_api.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_agent_task_manager.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_debt_info.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_node_controller_query_api.h"
namespace ubse::mem::controller::debt {
UBSE_DEFINE_THIS_MODULE("ubse");
constexpr uint64_t MB_TO_BYTE = 1024 * 1024;

using namespace ubse::election;
using namespace ubse::log;
// 填充 fdDesc
uint32_t FillFdDesc(UbseMemFdDesc &fdDesc, const UbseMemDebtQueryRequest &request,
                    const UbseMemFdBorrowImportObj &importObj)
{
    fdDesc.name = request.name;
    fdDesc.memIds.clear();
    for (const auto &obmmInfo : importObj.status.importResults) {
        fdDesc.memIds.push_back(obmmInfo.memId);
    }
    fdDesc.totalMemSize = importObj.req.size;
    fdDesc.unitSize = static_cast<uint64_t>(importObj.algoResult.blockSize) * MB_TO_BYTE;
    // 填充导入信息
    ubse::nodeController::UbseNodeGetByNodeIdInMaster(request.importNodeId, fdDesc.importNode);
    // 填充导出信息
    if (!importObj.algoResult.exportNumaInfos.empty()) {
        ubse::nodeController::UbseNodeGetByNodeIdInMaster(importObj.algoResult.exportNumaInfos[0].nodeId,
                                                          fdDesc.exportNode);
    }
    UbseMemResult memResult = GetFdStageByObj(request.name, request.importNodeId, GetNodeMemDebtInfoMap());
    fdDesc.state = memResult.stage;
    return UBSE_OK;
}
uint32_t UbseMemFdGet(const UbseMemDebtQueryRequest &request, UbseMemFdDesc &fdDesc)
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
    const UbseUdsInfo udsInfo = request.udsInfo;
    const std::string importNodeId = request.importNodeId;
    NodeMemDebtInfo debtInfo = GetNodeMemDebtInfoById(importNodeId);
    auto iterator = debtInfo.fdImportObjMap.find(name);
    if (iterator == debtInfo.fdImportObjMap.end()) {
        UBSE_LOG_ERROR << "Failed to find no delete debt, name: " << name << "related node id: " << importNodeId;
        return UBSE_ERR_NOT_EXIST;
    }

    // 找到目标对象，提取信息
    auto importObj = iterator->second;
    // udsIndo不为空, 校验权限
    if (!importObj.req.udsInfo.CheckPermission(udsInfo)) {
        UBSE_LOG_ERROR << "src udsInfo: username: " << udsInfo.username << ", uid: " << udsInfo.uid
                       << "dst udsInfo: username: " << importObj.req.udsInfo.username
                       << ", uid: " << importObj.req.udsInfo.uid;
        UBSE_LOG_ERROR << "Permission denied. related name: " << name;
        return UBSE_ERR_AUTH_FAILED;
    }
    FillFdDesc(fdDesc, request, importObj);
    return UBSE_OK; // 根据查找结果返回
}

uint32_t UbseMemFdList(const UbseMemDebtQueryRequest &request, std::vector<UbseMemFdDesc> &fdDescs)
{
    UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
        return ret;
    }
    if (currentRoleInfo.nodeRole != ELECTION_ROLE_MASTER) {
        UBSE_LOG_ERROR << "current role is not master. nodeId: " << currentRoleInfo.nodeId;
        return UBSE_ERR_INTERNAL;
    }
    const UbseUdsInfo udsInfo = request.udsInfo;
    fdDescs.clear();
    NodeMemDebtInfoMap debtInfoMap = GetNodeMemDebtInfoMap();
    for (const auto &[nodeId, debtInfos] : debtInfoMap) {
        for (const auto &[name, importObj] : debtInfos.fdImportObjMap) {
            if (!importObj.req.udsInfo.CheckPermission(udsInfo)) {
                continue;
            }
            if (!request.importNodeId.empty() && request.importNodeId != nodeId) {
                continue;
            }
            def::UbseMemFdDesc fdDesc{};
            fdDesc.name = name;
            fdDesc.memIds.clear();
            for (const auto &obmmInfo : importObj.status.importResults) {
                fdDesc.memIds.push_back(obmmInfo.memId);
            }
            fdDesc.totalMemSize = importObj.req.size;
            fdDesc.unitSize = static_cast<uint64_t>(importObj.algoResult.blockSize) * MB_TO_BYTE;
            // 填充导入信息
            ubse::nodeController::UbseNodeGetByNodeIdInMaster(nodeId, fdDesc.importNode);
            // 填充导出信息
            if (!importObj.algoResult.exportNumaInfos.empty()) {
                ubse::nodeController::UbseNodeGetByNodeIdInMaster(importObj.algoResult.exportNumaInfos[0].nodeId,
                                                                  fdDesc.exportNode);
            }
            UbseMemResult memResult = GetFdStageByObj(name, nodeId, debtInfoMap);
            fdDesc.state = memResult.stage;
            fdDescs.push_back(fdDesc);
        }
    }
    return UBSE_OK;
}

UbseMemResult GetFdStageByObj(const std::string &name, const std::string &importNodeId,
                              const NodeMemDebtInfoMap &debtInfoMap)
{
    UbseMemFdBorrowImportObj fdImportObj{};
    UbseMemFdBorrowExportObj fdExportObj{};
    bool importObjExist = false;
    bool exportObjExist = false;
    UbseMemResult result{name};
    for (auto [nodeId, nodeInfo] : debtInfoMap) {
        if (nodeInfo.fdImportObjMap.find(name) == nodeInfo.fdImportObjMap.end()) {
            continue;
        }
        // 节点级唯一、多个node可能有同一个name
        if (nodeId != importNodeId) {
            continue;
        }
        fdImportObj = nodeInfo.fdImportObjMap[name];
        importObjExist = true;
        break;
    }
    auto exportKey = GenerateExportObjKey(name, importNodeId);
    for (auto [nodeId, nodeInfo] : debtInfoMap) {
        if (nodeInfo.fdExportObjMap.find(exportKey) == nodeInfo.fdExportObjMap.end()) {
            continue;
        }
        fdExportObj = nodeInfo.fdExportObjMap[exportKey];
        exportObjExist = true;
        break;
    }
    result.importNodeId = importNodeId;
    if (!importObjExist && !exportObjExist) {
        result.name = name;
        result.stage = UbseMemStage::UBSE_NOT_EXIST;
        return result;
    }

    result.realSize = fdImportObj.req.size;
    if (!importObjExist && exportObjExist) {
        result.stage = UbseMemStage::UBSE_ERR_WAIT_UNEXPORT;
        return result;
    }

    result.stage = GetOptStageByObjState(fdImportObj, exportObjExist);
    return result;
}

UbseMemFdBorrowExportObj UbseFdExportObjGet(const std::string &nodeId, const std::string &name,
    const std::string &importNodeId, const bool isFromTaskManager)
{
    // 首先从TaskManger中获取
    UbseMemFdBorrowExportObj obj{};
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
    const auto objIter = nodeIter->second.fdExportObjMap.find(GenerateExportObjKey(name, importNodeId));
    if (objIter == nodeIter->second.fdExportObjMap.end()) {
        UBSE_LOG_WARN << "name=" << name << ", importNodeId=" << importNodeId << " is not in debt.";
        return {};
    }
    return objIter->second;
}

UbseMemFdBorrowImportObj UbseFdImportObjGet(const std::string &nodeId, const std::string &name,
                                            const bool isFromTaskManager)
{
    // 首先从TaskManger中获取
    UbseMemFdBorrowImportObj obj{};
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
    const auto objIter = nodeIter->second.fdImportObjMap.find(name);
    if (objIter == nodeIter->second.fdImportObjMap.end()) {
        UBSE_LOG_WARN << "name=" << name << " is not in debt.";
        return {};
    }
    return objIter->second;
}
} // namespace ubse::mem::controller::debt