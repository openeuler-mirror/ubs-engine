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
#include <ubse_node_controller_query_api.h>

#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_agent_task_manager.h"
#include "ubse_mem_constants.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_str_util.h"
#include "ubs_engine_topo.h"
namespace ubse::mem::controller::debt {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::election;
using namespace ubse::log;
using namespace ubse::mem::strategy;

constexpr size_t MAX_MEM_DESC_COUNT = 2000; // 查询内存信息列表返回的最大数据量

std::vector<uint32_t> ConvertNodelistToRegion(const std::vector<UbseNodeInfo>& nodelist)
{
    std::vector<uint32_t> region;
    region.reserve(nodelist.size());

    for (const auto& node : nodelist) {
        uint32_t nodeId;
        auto ret = ConvertStrToUint32(node.nodeId, nodeId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "invalid nodeId=" << node.nodeId;
            continue;
        }
        region.push_back(nodeId);
    }

    return region;
}

UbseMemResult GetShmExportStageByObj(const std::shared_ptr<const UbseMemShareBorrowExportObj>& exportObjPtr)
{
    UbseMemResult result{};
    result.name = exportObjPtr->req.name;
    if (exportObjPtr->status.state == UBSE_MEM_EXPORT_RUNNING) {
        result.stage = UbseMemStage::UBSE_CREATING;
    }
    if (exportObjPtr->status.state == UBSE_MEM_EXPORT_DESTROYING) {
        result.stage = UbseMemStage::UBSE_DELETING;
    }
    if (exportObjPtr->status.state == UBSE_MEM_EXPORT_DESTROYED) {
        result.stage = UbseMemStage::UBSE_NOT_EXIST;
    }
    if (exportObjPtr->status.state == UBSE_MEM_EXPORT_SUCCESS) {
        result.stage = UbseMemStage::UBSE_EXIST;
    }
    return result;
}

UbseMemResult GetShmImportStageByObj(const std::shared_ptr<const UbseMemShareBorrowImportObj>& importObjPtr)
{
    UbseMemResult result{};
    result.name = importObjPtr->req.name;
    if (importObjPtr->status.state == UBSE_MEM_IMPORT_RUNNING) {
        result.stage = UbseMemStage::UBSE_CREATING;
    }
    if (importObjPtr->status.state == UBSE_MEM_IMPORT_DESTROYING) {
        result.stage = UbseMemStage::UBSE_DELETING;
    }
    if (importObjPtr->status.state == UBSE_MEM_IMPORT_DESTROYED) {
        result.stage = UbseMemStage::UBSE_NOT_EXIST;
    }
    if (importObjPtr->status.state == UBSE_MEM_IMPORT_SUCCESS) {
        result.stage = UbseMemStage::UBSE_EXIST;
    }
    return result;
}

void ShmDecExportAssignment(const std::string& name, def::UbseMemShmDesc& shmDesc,
                            const std::shared_ptr<const UbseMemShareBorrowExportObj>& exportObjPtr)
{
    shmDesc.name = name;
    shmDesc.totalMemSize = exportObjPtr->req.size;
    auto& nodeController = nodeController::UbseNodeController::GetInstance();
    shmDesc.unitSize = static_cast<uint64_t>(exportObjPtr->algoResult.blockSize) * MB_TO_BYTE;
    shmDesc.region = ConvertNodelistToRegion(exportObjPtr->req.shmRegion.nodelist);
    if (exportObjPtr->algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_WARN << "The exportObj with empty export numa infos will be ignored, name=" << name;
        return;
    }
    const std::string exportNodeId = exportObjPtr->algoResult.exportNumaInfos[0].nodeId;
    ubse::nodeController::UbseNodeGetByNodeIdInMaster(exportNodeId, shmDesc.exportNode);
    error_t cpyRet =
        memcpy_s(shmDesc.userInfo, UBSE_MAX_USR_INFO_LEN, exportObjPtr->req.usrInfo, UBSE_MAX_USR_INFO_LEN);
    if (cpyRet != UBSE_OK) {
        UBSE_LOG_WARN << "userInfo create failed" << name;
    }
    UbseMemResult result = GetShmExportStageByObj(exportObjPtr);
    shmDesc.state = result.stage;
}

uint32_t AssignExportInfo(const UbseMemDebtQueryRequest& request,
                          const std::shared_ptr<const UbseMemShareBorrowExportObj>& exportObjPtr,
                          UbseMemShmDesc& shmDesc)
{
    const std::string name = request.name;
    const UbseUdsInfo udsInfo = request.udsInfo;
    // 校验权限
    if (!exportObjPtr->req.udsInfo.CheckPermission(udsInfo)) {
        UBSE_LOG_ERROR << "src udsInfo: username: " << udsInfo.username << ", uid: " << udsInfo.uid
                       << "dst udsInfo: username: " << exportObjPtr->req.udsInfo.username
                       << ", uid: " << exportObjPtr->req.udsInfo.uid;
        UBSE_LOG_ERROR << "Permission denied. related name: " << name;
        return UBSE_ERR_AUTH_FAILED;
    }

    ShmDecExportAssignment(name, shmDesc, exportObjPtr);
    return UBSE_OK;
}
uint32_t AssignImportInfo(const UbseMemDebtQueryRequest& request,
                          std::vector<std::shared_ptr<const UbseMemShareBorrowImportObj>>& importObjPtrs,
                          UbseMemShmDesc& shmDesc)
{
    const std::string name = request.name;
    // 填充导入相关数据
    auto importObj = importObjPtrs[0];
    error_t cpyRet = memcpy_s(shmDesc.userInfo, UBSE_MAX_USR_INFO_LEN, importObj->req.usrInfo, UBSE_MAX_USR_INFO_LEN);
    if (cpyRet != UBSE_OK) {
        UBSE_LOG_WARN << "userInfo create from importObj failed, name=" << name;
    }
    shmDesc.importDesc.clear();
    for (const auto& importObjPtr : importObjPtrs) {
        if (shmDesc.name.empty()) {
            shmDesc.name = name;
        }
        def::UbseMemShmImportDesc importDesc;
        for (const auto& obmmInfo : importObjPtr->status.importResults) {
            importDesc.memIds.push_back(obmmInfo.memId);
        }
        std::string importNodeId = importObjPtr->importNodeId;
        nodeController::UbseNodeGetByNodeIdInMaster(importNodeId, importDesc.importNode);
        UbseMemResult memResult = GetShmImportStageByObj(importObjPtr);
        importDesc.state = memResult.stage;
        shmDesc.importDesc.push_back(importDesc);
    }
    return UBSE_OK;
}
uint32_t UbseMemShmGet(const UbseMemDebtQueryRequest& request, UbseMemShmDesc& shmDesc)
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

    bool found = false;

    auto& ledger = UbseMemDebtLedger::GetInstance();
    auto exportObjPtr = ledger.GetDebtMap<UbseMemShareBorrowExportObj>().GetExportResourceByResId(request.name);
    if (exportObjPtr) {
        found = true;
        // 填充导出数据
        auto ret = AssignExportInfo(request, exportObjPtr, shmDesc);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "AssignExportInfo failed, ret: " << FormatRetCode(ret);
            return ret;
        }
    }

    std::vector<std::shared_ptr<const UbseMemShareBorrowImportObj>> allImportObjs;
    auto allNodeImportMaps = ledger.GetDebtMap<UbseMemShareBorrowImportObj>().GetAllNodeMaps();
    for (const auto& [nodeId, nodeMap] : allNodeImportMaps) {
        if (!request.importNodeId.empty() && nodeId != request.importNodeId) {
            continue;
        }
        auto importObjPtr = nodeMap->Get(request.name);
        if (importObjPtr && importObjPtr->status.state != UBSE_MEM_IMPORT_DESTROYED) {
            allImportObjs.emplace_back(importObjPtr);
        }
    }
    UBSE_LOG_INFO << "Total import objects found for name=" << request.name << ": " << allImportObjs.size();
    if (!allImportObjs.empty()) {
        found = true;
        auto ret = AssignImportInfo(request, allImportObjs, shmDesc);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "AssignImportInfo failed, ret: " << FormatRetCode(ret);
            return ret;
        }
    }
    return found ? UBSE_OK : UBSE_ERR_NOT_EXIST;
}

void ProcessShareExportObjects(const def::UbseMemDebtQueryRequest& request,
                               std::unordered_map<std::string, def::UbseMemShmDesc>& descMap)
{
    auto& ledger = UbseMemDebtLedger::GetInstance();
    auto allNodeMaps = ledger.GetDebtMap<UbseMemShareBorrowExportObj>().GetAllNodeMaps();

    for (const auto& [nodeId, nodeMap] : allNodeMaps) {
        auto allResources = nodeMap->GetAll();
        for (const auto& [name, exportObjPtr] : allResources) {
            if (!request.name.empty() && name.rfind(request.name, 0) != 0) {
                continue;
            }
            // 检查权限
            if (!exportObjPtr->req.udsInfo.CheckPermission(request.udsInfo)) {
                continue;
            }
            def::UbseMemShmDesc shmDesc{};
            ShmDecExportAssignment(name, shmDesc, exportObjPtr);
            descMap[name] = std::move(shmDesc);
        }
    }
}

void ProcessShareImportObjects(const def::UbseMemDebtQueryRequest& request,
                               std::unordered_map<std::string, def::UbseMemShmDesc>& descMap)
{
    auto& ledger = UbseMemDebtLedger::GetInstance();
    auto allNodeMaps = ledger.GetDebtMap<UbseMemShareBorrowImportObj>().GetAllNodeMaps();

    for (const auto& [nodeId, nodeMap] : allNodeMaps) {
        auto allResources = nodeMap->GetAll();
        for (const auto& [name, importObjPtr] : allResources) {
            if (!request.name.empty() && name.rfind(request.name, 0) != 0) {
                continue;
            }
            // 检查权限
            if (!importObjPtr->req.udsInfo.CheckPermission(request.udsInfo)) {
                continue;
            }

            auto [it, inserted] = descMap.try_emplace(name);
            auto& shmDesc = it->second;
            // name不存在
            if (inserted) {
                shmDesc.name = name;
            }
            def::UbseMemShmImportDesc importDesc;
            for (const auto& obmmInfo : importObjPtr->status.importResults) {
                importDesc.memIds.push_back(obmmInfo.memId);
            }

            std::string importNodeId = importObjPtr->importNodeId;
            nodeController::UbseNodeGetByNodeIdInMaster(importNodeId, importDesc.importNode);
            UbseMemResult memResult = GetShmImportStageByObj(importObjPtr);
            importDesc.state = memResult.stage;
            shmDesc.importDesc.push_back(std::move(importDesc));
            error_t cpyRet =
                memcpy_s(shmDesc.userInfo, UBSE_MAX_USR_INFO_LEN, importObjPtr->req.usrInfo, UBSE_MAX_USR_INFO_LEN);
            if (cpyRet != UBSE_OK) {
                UBSE_LOG_WARN << "userInfo create from importObj failed, name=" << name;
            }
        }
    }
}

void FillResultWithLimit(std::unordered_map<std::string, def::UbseMemShmDesc>& descMap,
                         std::vector<def::UbseMemShmDesc>& out)
{
    out.clear();
    out.reserve(std::min(descMap.size(), MAX_MEM_DESC_COUNT));

    size_t count = 0;
    for (auto& kv : descMap) {
        if (count >= MAX_MEM_DESC_COUNT)
            break;
        out.push_back(std::move(kv.second));
        ++count;
    }
}

uint32_t UbseMemShmList(const UbseMemDebtQueryRequest& request, std::vector<UbseMemShmDesc>& shmDescs)
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
    std::unordered_map<std::string, def::UbseMemShmDesc> descMap{};
    shmDescs.clear();
    // 遍历 export
    ProcessShareExportObjects(request, descMap);
    // 遍历 import
    ProcessShareImportObjects(request, descMap);
    // 回填结果
    FillResultWithLimit(descMap, shmDescs);
    return UBSE_OK;
}

uint32_t UbseMemShmStatusGet(const UbseMemDebtQueryRequest& request, def::UbseMemShmMemStatusDesc& shmStatusDesc)
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

    auto& ledger = UbseMemDebtLedger::GetInstance();
    auto exportObjPtr = ledger.GetDebtMap<UbseMemShareBorrowExportObj>().GetExportResourceByResId(request.name);
    if (!exportObjPtr) {
        UBSE_LOG_WARN << "No export information found. related name: " << name;
    }

    for (const auto& obmmInfo : exportObjPtr->status.exportObmmInfo) {
        if (obmmInfo.memIdStatus == UB_MEM_HEALTHY) {
            continue;
        }
        shmStatusDesc.memIds.push_back(obmmInfo.memId);
        shmStatusDesc.faultTypes.push_back(obmmInfo.memIdStatus);
    }
    return UBSE_OK;
}

UbseMemResult GetShmExportStageByObj(const std::string& name)
{
    auto& ledger = UbseMemDebtLedger::GetInstance();
    auto exportObjPtr = ledger.GetDebtMap<UbseMemShareBorrowExportObj>().GetExportResourceByResId(name);
    if (!exportObjPtr) {
        UbseMemResult result{};
        result.name = name;
        result.stage = UbseMemStage::UBSE_NOT_EXIST;
        return result;
    }
    return GetShmExportStageByObj(exportObjPtr);
}

UbseMemResult GetShmImportStageByObj(const std::string& name, const std::string& importNodeId)
{
    auto& ledger = UbseMemDebtLedger::GetInstance();
    auto importObjPtr = ledger.GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(importNodeId, name);
    if (!importObjPtr) {
        UbseMemResult result{};
        result.name = name;
        result.stage = UbseMemStage::UBSE_NOT_EXIST;
        return result;
    }
    return GetShmImportStageByObj(importObjPtr);
}

UbseMemShareBorrowExportObj UbseShareExportObjGet(const std::string& nodeId, const std::string& name,
                                                  const bool isFromTaskManager)
{
    UbseMemShareBorrowExportObj obj{};
    if (isFromTaskManager) {
        const auto ret = UbseMemAgentTaskManager::GetTaskObj(name, "", obj);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "name=" << name << " is not in task manager.";
        } else {
            return obj;
        }
    }

    auto& ledger = UbseMemDebtLedger::GetInstance();
    auto exportObjPtr = ledger.GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(nodeId, name);
    if (!exportObjPtr) {
        UBSE_LOG_WARN << "name=" << name << " is not in debt.";
        return {};
    }
    return *exportObjPtr;
}

UbseMemShareBorrowImportObj UbseShareImportObjGet(const std::string& nodeId, const std::string& name,
                                                  const bool isFromTaskManager)
{
    UbseMemShareBorrowImportObj obj{};
    if (isFromTaskManager) {
        const auto ret = UbseMemAgentTaskManager::GetTaskObj(name, "", obj);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "name=" << name << " is not in task manager.";
        } else {
            return obj;
        }
    }

    auto& ledger = UbseMemDebtLedger::GetInstance();
    auto importObjPtr = ledger.GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(nodeId, name);
    if (!importObjPtr) {
        UBSE_LOG_WARN << "name=" << name << " is not in debt.";
        return {};
    }
    return *importObjPtr;
}
} // namespace ubse::mem::controller::debt
