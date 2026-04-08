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

#include "ubs_engine_topo.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_agent_task_manager.h"
#include "ubse_mem_constants.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_mem_debt_info.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_str_util.h"
namespace ubse::mem::controller::debt {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::election;
using namespace ubse::log;
using namespace ubse::mem::strategy;

constexpr size_t MAX_MEM_DESC_COUNT = 2000; // 查询内存信息列表返回的最大数据量

std::vector<uint32_t> ConvertNodelistToRegion(const std::vector<UbseNodeInfo> &nodelist)
{
    std::vector<uint32_t> region;
    region.reserve(nodelist.size());

    for (const auto &node : nodelist) {
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

void ShmDecExportAssignment(const std::string &name, def::UbseMemShmDesc &shmDesc,
                            const UbseMemShareBorrowExportObj &exportObj)
{
    shmDesc.name = name;
    shmDesc.totalMemSize = exportObj.req.size;
    auto &nodeController = nodeController::UbseNodeController::GetInstance();
    shmDesc.unitSize = static_cast<uint64_t>(exportObj.algoResult.blockSize) * MB_TO_BYTE;
    shmDesc.region = ConvertNodelistToRegion(exportObj.req.shmRegion.nodelist);
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_WARN << "The exportObj with empty export numa infos will be ignored, name=" << name;
        return;
    }
    const std::string exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    ubse::nodeController::UbseNodeGetByNodeIdInMaster(exportNodeId, shmDesc.exportNode);
    error_t cpyRet = memcpy_s(shmDesc.userInfo, UBSE_MAX_USR_INFO_LEN, exportObj.req.usrInfo, UBSE_MAX_USR_INFO_LEN);
    if (cpyRet != UBSE_OK) {
        UBSE_LOG_WARN << "userInfo create failed" << name;
    }
    UbseMemResult result{};
    if (exportObj.status.state == UBSE_MEM_EXPORT_RUNNING) {
        result.stage = UbseMemStage::UBSE_CREATING;
    }
    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING) {
        result.stage = UbseMemStage::UBSE_DELETING;
    }
    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        result.stage = UbseMemStage::UBSE_NOT_EXIST;
    }
    if (exportObj.status.state == UBSE_MEM_EXPORT_SUCCESS) {
        result.stage = UbseMemStage::UBSE_EXIST;
    }
    shmDesc.state = result.stage;
}

uint32_t AssignExportInfo(const UbseMemDebtQueryRequest &request, UbseMemShareBorrowExportObj &exportObj, bool &found,
                          UbseMemShmDesc &shmDesc)
{
    const std::string name = request.name;
    const UbseUdsInfo udsInfo = request.udsInfo;
    if (name != exportObj.req.name) {
        UBSE_LOG_WARN << "No export information found. related name: " << name;
        return UBSE_OK;
    }
    // 校验权限
    if (!exportObj.req.udsInfo.CheckPermission(udsInfo)) {
        UBSE_LOG_ERROR << "src udsInfo: username: " << udsInfo.username << ", uid: " << udsInfo.uid
                       << "dst udsInfo: username: " << exportObj.req.udsInfo.username
                       << ", uid: " << exportObj.req.udsInfo.uid;
        UBSE_LOG_ERROR << "Permission denied. related name: " << name;
        return UBSE_ERR_AUTH_FAILED;
    }

    found = true;
    ShmDecExportAssignment(name, shmDesc, exportObj);
    return UBSE_OK;
}
uint32_t AssignImportInfo(const UbseMemDebtQueryRequest &request, std::vector<UbseMemShareBorrowImportObj> &importObjs,
                          bool &found, UbseMemShmDesc &shmDesc)
{
    const std::string name = request.name;
    // 填充导入相关数据
    shmDesc.importDesc.clear();
    for (const auto &importObj : importObjs) {
        if (shmDesc.name.empty()) {
            shmDesc.name = name;
        }
        def::UbseMemShmImportDesc importDesc;
        for (const auto &obmmInfo : importObj.status.importResults) {
            importDesc.memIds.push_back(obmmInfo.memId);
        }
        std::string importNodeId = importObj.importNodeId;
        nodeController::UbseNodeGetByNodeIdInMaster(importNodeId, importDesc.importNode);
        found = true;
        UbseMemResult memResult;
        if (importObj.status.state == UBSE_MEM_IMPORT_RUNNING) {
            memResult.stage = UbseMemStage::UBSE_CREATING;
        }
        if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYING) {
            memResult.stage = UbseMemStage::UBSE_DELETING;
        }
        if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
            memResult.stage = UbseMemStage::UBSE_NOT_EXIST;
        }
        if (importObj.status.state == UBSE_MEM_IMPORT_SUCCESS) {
            memResult.stage = UbseMemStage::UBSE_EXIST;
        }
        importDesc.state = memResult.stage;
        shmDesc.importDesc.push_back(importDesc);
    }
    return UBSE_OK;
}
uint32_t UbseMemShmGet(const UbseMemDebtQueryRequest &request, UbseMemShmDesc &shmDesc)
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
    UbseMemShareBorrowExportObj obj = GetNodeMemShareExpDebtInfoMap(request.exportNodeId, request.name);
    bool found = false;
    // 填充导出数据
    auto ret = AssignExportInfo(request, obj, found, shmDesc);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AssignExportInfo failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    auto importObjs = GetNodeMemShareImportDebtInfoMap(request.importNodeId, request.name);
    ret = AssignImportInfo(request, importObjs, found, shmDesc);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AssignImportInfo failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    return found ? UBSE_OK : UBSE_ERR_NOT_EXIST; // 根据查找结果返回
}

void ProcessShareExportObjects(const def::UbseMemDebtQueryRequest &request, const NodeMemDebtInfoMap &debtInfoMap,
                               std::unordered_map<std::string, def::UbseMemShmDesc> &descMap)
{
    for (const auto &[nodeKey, debtInfo] : debtInfoMap) {
        // 遍历 export
        for (const auto &[name, exportObj] : debtInfo.shareExportObjMap) {
            // name 前缀过滤
            if (!request.name.empty() && name.rfind(request.name, 0) != 0) {
                continue;
            }
            // 检查权限
            if (!exportObj.req.udsInfo.CheckPermission(request.udsInfo)) {
                continue;
            }
            def::UbseMemShmDesc shmDesc{};
            ShmDecExportAssignment(name, shmDesc, exportObj);
            descMap[name] = std::move(shmDesc);
        }
    }
}

void ProcessShareImportObjects(const def::UbseMemDebtQueryRequest &request, const NodeMemDebtInfoMap &debtInfoMap,
                               std::unordered_map<std::string, def::UbseMemShmDesc> &descMap)
{
    for (const auto &[nodeKey, debtInfo] : debtInfoMap) {
        for (const auto &[name, importObj] : debtInfo.shareImportObjMap) {
            // name 前缀过滤
            if (!request.name.empty() && name.rfind(request.name, 0) != 0) {
                continue;
            }
            // 检查权限
            if (!importObj.req.udsInfo.CheckPermission(request.udsInfo)) {
                continue;
            }

            auto [it, inserted] = descMap.try_emplace(name);
            auto &shmDesc = it->second;
            // name不存在
            if (inserted) {
                shmDesc.name = name;
            }
            def::UbseMemShmImportDesc importDesc;
            for (const auto &obmmInfo : importObj.status.importResults) {
                importDesc.memIds.push_back(obmmInfo.memId);
            }

            std::string importNodeId = importObj.importNodeId;
            nodeController::UbseNodeGetByNodeIdInMaster(importNodeId, importDesc.importNode);
            UbseMemResult memResult = GetShmImportStageByObj(name, importNodeId, debtInfoMap);
            importDesc.state = memResult.stage;
            shmDesc.importDesc.push_back(std::move(importDesc));
        }
    }
}

void FillResultWithLimit(std::unordered_map<std::string, def::UbseMemShmDesc> &descMap,
                         std::vector<def::UbseMemShmDesc> &out)
{
    out.clear();
    out.reserve(std::min(descMap.size(), MAX_MEM_DESC_COUNT));

    size_t count = 0;
    for (auto &kv : descMap) {
        if (count >= MAX_MEM_DESC_COUNT)
            break;
        out.push_back(std::move(kv.second));
        ++count;
    }
}

uint32_t UbseMemShmList(const UbseMemDebtQueryRequest &request, std::vector<UbseMemShmDesc> &shmDescs)
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
    NodeMemDebtInfoMap debtInfoMap = GetNodeMemDebtInfoMap();
    shmDescs.clear();
    // 遍历 export
    ProcessShareExportObjects(request, debtInfoMap, descMap);

    // 遍历 import
    ProcessShareImportObjects(request, debtInfoMap, descMap);
    // 回填结果
    FillResultWithLimit(descMap, shmDescs);
    return UBSE_OK;
}

uint32_t UbseMemShmStatusGet(const UbseMemDebtQueryRequest &request, def::UbseMemShmMemStatusDesc &shmStatusDesc)
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
    NodeMemDebtInfoMap debtInfoMap = GetNodeMemDebtInfoMap();

    for (const auto &[nodeId, debtInfos] : debtInfoMap) {
        auto exportIterator = debtInfos.shareExportObjMap.find(name);
        if (exportIterator == debtInfos.shareExportObjMap.end()) {
            UBSE_LOG_WARN << "No export information found. related name: " << name;
            continue;
        }

        for (const auto &obmmInfo : exportIterator->second.status.exportObmmInfo) {
            if (obmmInfo.memIdStatus == UB_MEM_HEALTHY) {
                continue;
            }
            shmStatusDesc.memIds.push_back(obmmInfo.memId);
            shmStatusDesc.faultTypes.push_back(obmmInfo.memIdStatus);
        }
    }
    return UBSE_OK;
}

UbseMemResult GetShmExportStageByObj(const std::string &name, const NodeMemDebtInfoMap &debtInfoMap)
{
    UbseMemShareBorrowExportObj shmExportObj{};
    bool exportObjExist = false;
    UbseMemResult result{};
    result.name = name;
    // 无法确认导出对象是哪个节点导出,需要遍历账本
    for (auto [nodeId, nodeInfo] : debtInfoMap) {
        if (nodeInfo.shareExportObjMap.find(name) == nodeInfo.shareExportObjMap.end()) {
            continue;
        }
        shmExportObj = nodeInfo.shareExportObjMap[name];
        exportObjExist = true;
        break;
    }
    if (!exportObjExist) {
        result.stage = UbseMemStage::UBSE_NOT_EXIST;
        return result;
    }
    if (shmExportObj.status.state == UBSE_MEM_EXPORT_RUNNING) {
        result.stage = UbseMemStage::UBSE_CREATING;
    }
    if (shmExportObj.status.state == UBSE_MEM_EXPORT_DESTROYING) {
        result.stage = UbseMemStage::UBSE_DELETING;
    }
    if (shmExportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        result.stage = UbseMemStage::UBSE_NOT_EXIST;
    }
    if (shmExportObj.status.state == UBSE_MEM_EXPORT_SUCCESS) {
        result.stage = UbseMemStage::UBSE_EXIST;
    }
    return result;
}

UbseMemResult GetShmImportStageByObj(const std::string &name, const std::string &importNodeId,
                                     const NodeMemDebtInfoMap &debtInfoMap)
{
    UbseMemResult result{};
    result.name = name;
    auto nodeInfo = debtInfoMap.find(importNodeId);
    if (nodeInfo == debtInfoMap.end()) {
        result.stage = UbseMemStage::UBSE_NOT_EXIST;
        return result;
    }
    auto shmImportObjIterator = nodeInfo->second.shareImportObjMap.find(name);
    if (shmImportObjIterator == nodeInfo->second.shareImportObjMap.end()) {
        result.stage = UbseMemStage::UBSE_NOT_EXIST;
        return result;
    }
    auto &shmImportObj = shmImportObjIterator->second;
    if (shmImportObj.status.state == UBSE_MEM_IMPORT_RUNNING) {
        result.stage = UbseMemStage::UBSE_CREATING;
    }
    if (shmImportObj.status.state == UBSE_MEM_IMPORT_DESTROYING) {
        result.stage = UbseMemStage::UBSE_DELETING;
    }
    if (shmImportObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        result.stage = UbseMemStage::UBSE_NOT_EXIST;
    }
    if (shmImportObj.status.state == UBSE_MEM_IMPORT_SUCCESS) {
        result.stage = UbseMemStage::UBSE_EXIST;
    }
    return result;
}

UbseMemShareBorrowExportObj UbseShareExportObjGet(const std::string &nodeId, const std::string &name,
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

    const auto map = GetNodeMemDebtInfoMap();
    const auto nodeIter = map.find(nodeId);
    if (nodeIter == map.end()) {
        return {};
    }
    const auto objIter = nodeIter->second.shareExportObjMap.find(name);
    if (objIter == nodeIter->second.shareExportObjMap.end()) {
        UBSE_LOG_WARN << "name=" << name << " is not in debt.";
        return {};
    }
    return objIter->second;
}

UbseMemShareBorrowImportObj UbseShareImportObjGet(const std::string &nodeId, const std::string &name,
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

    const auto map = GetNodeMemDebtInfoMap();
    const auto nodeIter = map.find(nodeId);
    if (nodeIter == map.end()) {
        return {};
    }
    const auto objIter = nodeIter->second.shareImportObjMap.find(name);
    if (objIter == nodeIter->second.shareImportObjMap.end()) {
        UBSE_LOG_WARN << "name=" << name << " is not in debt.";
        return {};
    }
    return objIter->second;
}
} // namespace ubse::mem::controller::debt