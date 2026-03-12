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
#include <securec.h>

#include "ubs_engine_topo.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_agent_task_manager.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_debt_info.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_query_api.h"

namespace ubse::mem::controller::debt {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::election;
using namespace ubse::log;

uint32_t FillNumaDesc(const std::string &name, const std::string &importNodeId,
                      const UbseMemNumaBorrowImportObj &importObj, def::UbseMemNumaDesc &numaDesc)
{
    numaDesc.name = name;
    UbseMemResult memResult = GetNumaStageByObj(name, importNodeId, GetNodeMemDebtInfoMap());
    numaDesc.state = memResult.stage;
    if (numaDesc.state != UbseMemStage::UBSE_EXIST && numaDesc.state != UbseMemStage::UBSE_ERR_ONLY_IMPORT) {
        // 内存的中间状态、其余信息并不完整、无需继续获取
        return UBSE_OK;
    }
    if (importObj.status.importResults.empty()) {
        UBSE_LOG_WARN << "importResult is empty, name: " << name << ", importNodeId: " << importNodeId;
        return UBSE_ERROR;
    }
    numaDesc.numaId = importObj.status.importResults[0].numaId;
    // 填充导入信息
    nodeController::UbseNodeGetByNodeIdInMaster(importNodeId, numaDesc.importNode);
    // 填充导出信息
    if (!importObj.algoResult.exportNumaInfos.empty()) {
        nodeController::UbseNodeGetByNodeIdInMaster(importObj.algoResult.exportNumaInfos[0].nodeId,
                                                    numaDesc.exportNode);
    }
    numaDesc.size = importObj.req.size;
    return UBSE_OK;
}
uint32_t UbseMemNumaGet(const UbseMemDebtQueryRequest &request, def::UbseMemNumaDesc &numaDesc)
{
    UbseRoleInfo currentRoleInfo{};
    if (const auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
    }
    if (currentRoleInfo.nodeRole != ELECTION_ROLE_MASTER) {
        UBSE_LOG_ERROR << "current role is not master. nodeId: " << currentRoleInfo.nodeId;
        return UBSE_ERR_INTERNAL;
    }
    const std::string name = request.name;
    const UbseUdsInfo udsInfo = request.udsInfo;
    const std::string importNodeId = request.importNodeId;
    NodeMemDebtInfo debtInfo = GetNodeMemDebtInfoById(importNodeId);
    const auto iterator = debtInfo.numaImportObjMap.find(name);
    if (iterator == debtInfo.numaImportObjMap.end()) {
        UBSE_LOG_ERROR << "Failed to find no delete debt, name: " << name << "related node id: " << importNodeId;
        return UBSE_ERR_NOT_EXIST;
    }
    // 找到目标对象，提取信息
    const auto importObj = iterator->second;
    // 校验权限
    if (!importObj.req.udsInfo.CheckPermission(udsInfo)) {
        UBSE_LOG_ERROR << "src udsInfo: username: " << udsInfo.username << ", uid: " << udsInfo.uid
                       << "dst udsInfo: username: " << importObj.req.udsInfo.username
                       << ", uid: " << importObj.req.udsInfo.uid << "Permission denied. related name: " << name;
        return UBSE_ERR_AUTH_FAILED;
    }
    return FillNumaDesc(name, importNodeId, importObj, numaDesc);
}
void FillExportNuma(def::UbseMemNumaDesc &numaDesc, std::vector<UbseMemDebtNumaInfo> exportNumaInfos)
{
    if (exportNumaInfos.empty()) {
        return;
    }
    numaDesc.exportNode.socketId[0] = exportNumaInfos[0].socketId;
    for (size_t i = 0; i < UBS_TOPO_SOCKET_NUM; i++) {
        for (size_t j = 0; j < UBS_TOPO_NUMA_NUM; j++) {
            numaDesc.exportNode.numaIds[i][j] = UINT32_MAX;
        }
    }
    std::set<uint32_t> numaIds{};
    for (auto &numaInfo : exportNumaInfos) {
        numaIds.insert(numaInfo.numaId);
    }
    uint32_t index = 0;
    for (auto &item : numaIds) {
        if (index >= UBS_TOPO_NUMA_NUM) {
            break;
        }
        numaDesc.exportNode.numaIds[0][index] = item;
        index++;
    }
}
uint32_t UbseMemNumaList(const UbseMemDebtQueryRequest &request, std::vector<def::UbseMemNumaDesc> &numaDescs)
{
    const UbseUdsInfo udsInfo = request.udsInfo;
    numaDescs.clear();
    UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
        return ret;
    }
    if (currentRoleInfo.nodeRole != ELECTION_ROLE_MASTER) {
        UBSE_LOG_ERROR << "current role is not master. nodeId: " << currentRoleInfo.nodeId;
        return UBSE_ERR_INTERNAL;
    }
    NodeMemDebtInfoMap debtInfoMap = GetNodeMemDebtInfoMap();
    for (const auto &[nodeId, debtInfos] : debtInfoMap) {
        for (const auto &[name, importObj] : debtInfos.numaImportObjMap) {
            if (!importObj.req.udsInfo.CheckPermission(udsInfo)) {
                continue;
            }
            if (!request.importNodeId.empty() && request.importNodeId != nodeId) {
                continue;
            }
            def::UbseMemNumaDesc numaDesc{};
            auto ret = FillNumaDesc(name, nodeId, importObj, numaDesc);
            if (ret != UBSE_OK) {
                UBSE_LOG_WARN << "fill numa desc failed, ret: " << ret << ", name: " << name
                              << ", importNodeId: " << nodeId;
                continue;
            }
            FillExportNuma(numaDesc, importObj.algoResult.exportNumaInfos);
            if (memcpy_s(numaDesc.usrInfo, UBSE_MAX_USR_INFO_LEN, importObj.req.usrInfo, UBSE_MAX_USR_INFO_LEN) !=
                EOK) {
                UBSE_LOG_ERROR << "MemCopy fail when copy usrInfo, numaImportObj name is " << importObj.req.name;
                return UBSE_ERR_OUT_OF_MEMORY;
            }
            numaDescs.push_back(numaDesc);
        }
    }
    return UBSE_OK;
}
uint32_t UbseMemNumaGetWithImportNode(const UbseMemDebtQueryRequest &request, UbseMemNumaDesc &numaDesc)
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
    const std::string name = request.name;
    const std::string importNodeId = request.importNodeId;
    UBSE_LOG_INFO << "UbseMemNumaGetWithImportNode enter, name: " << name << ", importNodeId: " << importNodeId;
    NodeMemDebtInfo debtInfo = GetNodeMemDebtInfoById(importNodeId);
    const auto iterator = debtInfo.numaImportObjMap.find(name);
    if (iterator == debtInfo.numaImportObjMap.end()) {
        UBSE_LOG_ERROR << "Failed to find no delete debt, name: " << name << "related node id: " << importNodeId;
        return UBSE_ERR_NOT_EXIST;
    }
    // 找到目标对象，提取信息
    auto importObj = iterator->second;
    numaDesc.name = name;
    if (importObj.status.importResults.empty()) {
        return UBSE_ERROR;
    }
    numaDesc.numaId = importObj.status.importResults[0].numaId;
    // 填充导入信息
    ubse::nodeController::def::UbseNode importNode;
    ubse::nodeController::UbseNodeGetByNodeIdInMaster(importNodeId, importNode);
    numaDesc.importNode.slotId = importNode.slotId;
    numaDesc.importNode.hostName = importNode.hostName;
    numaDesc.importNode.socketIdList.clear();
    for (int i = 0; i < UBS_TOPO_SOCKET_NUM; i++) {
        numaDesc.importNode.socketIdList.push_back(static_cast<int16_t>(importNode.socketId[i]));
    }

    // 填充导出信息
    if (!importObj.algoResult.exportNumaInfos.empty()) {
        ubse::nodeController::def::UbseNode exportNode;
        ubse::nodeController::UbseNodeGetByNodeIdInMaster(importObj.algoResult.exportNumaInfos[0].nodeId, exportNode);
        numaDesc.exportNode.slotId = exportNode.slotId;
        numaDesc.exportNode.hostName = exportNode.hostName;
        numaDesc.exportNode.socketIdList.clear();
        for (int i = 0; i < UBS_TOPO_SOCKET_NUM; i++) {
            numaDesc.exportNode.socketIdList.push_back(static_cast<int16_t>(exportNode.socketId[i]));
        }
    }
    numaDesc.size = importObj.req.size;
    return UBSE_OK; // 根据查找结果返回
}

UbseMemResult GetNumaStageByObj(const std::string &name, const std::string &importNodeId,
                                const NodeMemDebtInfoMap &debtInfoMap)
{
    UbseMemNumaBorrowImportObj numaImportObj{};
    UbseMemNumaBorrowExportObj numaExportObj{};
    bool importObjExist = false;
    bool exportObjExist = false;
    UbseMemResult result{};
    result.name = name;
    for (auto [nodeId, nodeInfo] : debtInfoMap) {
        if (nodeInfo.numaImportObjMap.find(name) == nodeInfo.numaImportObjMap.end()) {
            continue;
        }
        // 节点级唯一、多个node可能有同一个name
        if (nodeId != importNodeId) {
            continue;
        }
        numaImportObj = nodeInfo.numaImportObjMap[name];
        importObjExist = true;
        break;
    }
    auto exportKey = GenerateExportObjKey(name, importNodeId);
    for (auto [nodeId, nodeInfo] : debtInfoMap) {
        if (nodeInfo.numaExportObjMap.find(exportKey) == nodeInfo.numaExportObjMap.end()) {
            continue;
        }
        numaExportObj = nodeInfo.numaExportObjMap[exportKey];
        exportObjExist = true;
    }
    result.importNodeId = importNodeId;
    if (!importObjExist && !exportObjExist) {
        result.stage = UbseMemStage::UBSE_NOT_EXIST;
        return result;
    }

    result.realSize = numaExportObj.req.size;
    if (!importObjExist && exportObjExist) {
        result.stage = UbseMemStage::UBSE_ERR_WAIT_UNEXPORT;
        return result;
    }

    result.stage = GetOptStageByObjState(numaImportObj, exportObjExist);
    return result;
}

UbseMemNumaBorrowExportObj UbseNumaExportObjGet(const std::string &nodeId, const std::string &name,
                                                const std::string &importNodeId, const bool isFromTaskManager)
{
    UbseMemNumaBorrowExportObj obj{};
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
    const auto objIter = nodeIter->second.numaExportObjMap.find(GenerateExportObjKey(name, importNodeId));
    if (objIter == nodeIter->second.numaExportObjMap.end()) {
        UBSE_LOG_WARN << "name=" << name << ", importNodeId=" << importNodeId << " is not in debt.";
        return {};
    }
    return objIter->second;
}

UbseMemNumaBorrowImportObj UbseNumaImportObjGet(const std::string &nodeId, const std::string &name,
                                                const bool isFromTaskManager)
{
    UbseMemNumaBorrowImportObj obj{};
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
    const auto objIter = nodeIter->second.numaImportObjMap.find(name);
    if (objIter == nodeIter->second.numaImportObjMap.end()) {
        UBSE_LOG_WARN << "name=" << name << " is not in debt.";
        return {};
    }
    return objIter->second;
}
} // namespace ubse::mem::controller::debt