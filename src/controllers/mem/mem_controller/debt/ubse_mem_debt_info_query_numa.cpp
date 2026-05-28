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

#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_agent_task_manager.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_query_api.h"
#include "ubs_engine_topo.h"

namespace ubse::mem::controller::debt {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::election;
using namespace ubse::log;
using namespace ubse::adapter_plugins::mmi;

uint32_t FillNumaDesc(const std::string& name, const std::string& importNodeId,
                      const UbseMemNumaBorrowImportObj& importObj, def::UbseMemNumaDesc& numaDesc)
{
    numaDesc.name = name;
    UbseMemResult memResult = GetNumaStageByObj(name, importNodeId);
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
uint32_t UbseMemNumaGet(const UbseMemDebtQueryRequest& request, def::UbseMemNumaDesc& numaDesc)
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

    auto importObjPtr =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().GetResource(importNodeId, name);
    if (!importObjPtr) {
        UBSE_LOG_ERROR << "Failed to find no delete debt, name: " << name << "related node id: " << importNodeId;
        return UBSE_ERR_NOT_EXIST;
    }
    // 校验权限
    if (!importObjPtr->req.udsInfo.CheckPermission(udsInfo)) {
        UBSE_LOG_ERROR << "src udsInfo: username: " << udsInfo.username << ", uid: " << udsInfo.uid
                       << "dst udsInfo: username: " << importObjPtr->req.udsInfo.username
                       << ", uid: " << importObjPtr->req.udsInfo.uid;
        UBSE_LOG_ERROR << "Permission denied. related name: " << name;
        return UBSE_ERR_AUTH_FAILED;
    }
    return FillNumaDesc(name, importNodeId, *importObjPtr, numaDesc);
}
void FillExportNuma(def::UbseMemNumaDesc& numaDesc, std::vector<UbseMemDebtNumaInfo> exportNumaInfos)
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
    for (auto& numaInfo : exportNumaInfos) {
        numaIds.insert(numaInfo.numaId);
    }
    uint32_t index = 0;
    for (auto& item : numaIds) {
        if (index >= UBS_TOPO_NUMA_NUM) {
            break;
        }
        numaDesc.exportNode.numaIds[0][index] = item;
        index++;
    }
}
uint32_t UbseMemNumaList(const UbseMemDebtQueryRequest& request, std::vector<def::UbseMemNumaDesc>& numaDescs)
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
    auto nodeImportMap =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().FindNodeMap(request.importNodeId);
    if (!nodeImportMap) {
        UBSE_LOG_INFO << "Failed to find import debt, import node id: " << request.importNodeId;
        return UBSE_OK;
    }

    for (const auto& [name, importObjPtr] : nodeImportMap->GetAll()) {
        if (!importObjPtr->req.udsInfo.CheckPermission(udsInfo)) {
            continue;
        }
        def::UbseMemNumaDesc numaDesc{};
        auto ret = FillNumaDesc(name, request.importNodeId, *importObjPtr, numaDesc);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "fill numa desc failed, ret: " << ret << ", name: " << name
                          << ", importNodeId: " << request.importNodeId;
            continue;
        }
        FillExportNuma(numaDesc, importObjPtr->algoResult.exportNumaInfos);
        if (memcpy_s(numaDesc.usrInfo, UBSE_MAX_USR_INFO_LEN, importObjPtr->req.usrInfo, UBSE_MAX_USR_INFO_LEN) !=
            EOK) {
            UBSE_LOG_ERROR << "MemCopy fail when copy usrInfo, numaImportObj name is " << importObjPtr->req.name;
            return UBSE_ERR_OUT_OF_MEMORY;
        }
        numaDescs.push_back(numaDesc);
    }
    return UBSE_OK;
}
uint32_t UbseMemNumaGetWithImportNode(const UbseMemDebtQueryRequest& request, UbseMemNumaDesc& numaDesc)
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

    auto importObjPtr =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().GetResource(importNodeId, name);
    if (!importObjPtr) {
        UBSE_LOG_ERROR << "Failed to find no delete debt, name: " << name << "related node id: " << importNodeId;
        return UBSE_ERR_NOT_EXIST;
    }
    // 找到目标对象，提取信息
    numaDesc.name = name;
    if (importObjPtr->status.importResults.empty()) {
        return UBSE_ERROR;
    }
    numaDesc.numaId = importObjPtr->status.importResults[0].numaId;
    // 填充导入信息
    ubse::nodeController::def::UbseNode importNode;
    ubse::nodeController::UbseNodeGetByNodeIdInMaster(importNodeId, importNode);
    numaDesc.importNode.slotId = importNode.slotId;
    numaDesc.importNode.hostName = importNode.hostName;
    for (int i = 0; i < UBS_TOPO_SOCKET_NUM; i++) {
        numaDesc.importNode.socketIdList.push_back(static_cast<int16_t>(importNode.socketId[i]));
    }

    // 填充导出信息
    if (!importObjPtr->algoResult.exportNumaInfos.empty()) {
        ubse::nodeController::def::UbseNode exportNode;
        ubse::nodeController::UbseNodeGetByNodeIdInMaster(importObjPtr->algoResult.exportNumaInfos[0].nodeId,
                                                          exportNode);
        numaDesc.exportNode.slotId = exportNode.slotId;
        numaDesc.exportNode.hostName = exportNode.hostName;
        for (int i = 0; i < UBS_TOPO_SOCKET_NUM; i++) {
            numaDesc.exportNode.socketIdList.push_back(static_cast<int16_t>(exportNode.socketId[i]));
        }
    }
    numaDesc.size = importObjPtr->req.size;
    return UBSE_OK; // 根据查找结果返回
}

UbseMemResult GetNumaStageByObj(const std::string& name, const std::string& importNodeId)
{
    return GetStageByObj<UbseMemNumaBorrowImportObj, UbseMemNumaBorrowExportObj>(name, importNodeId);
}

UbseMemNumaBorrowExportObj UbseNumaExportObjGet(const std::string& nodeId, const std::string& name,
                                                const std::string& importNodeId, const bool isFromTaskManager)
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

    const auto exportKey = GenerateExportObjKey(name, importNodeId);
    auto exportObjPtr =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowExportObj>().GetExportResourceByResId(exportKey);
    if (!exportObjPtr) {
        UBSE_LOG_WARN << "name=" << name << ", importNodeId=" << importNodeId << " is not in debt.";
        return {};
    }
    return *exportObjPtr;
}

UbseMemNumaBorrowImportObj UbseNumaImportObjGet(const std::string& nodeId, const std::string& name,
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

    auto importObjPtr =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().GetResource(nodeId, name);
    if (!importObjPtr) {
        UBSE_LOG_WARN << "name=" << name << " is not in debt.";
        return {};
    }
    return *importObjPtr;
}
} // namespace ubse::mem::controller::debt
