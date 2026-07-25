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
#include "ubse_mem_debt_info.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_str_util.h"
#include "ubs_engine_topo.h"
#include "ubse_smbios.h"
#include "ubse_mem_controller_msg.h"
#include "ubse_mem_controller_helper.h"
#include "ubse_mem_share_store.h"

namespace ubse::mem::controller::debt {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::election;
using namespace ubse::log;
using namespace ubse::mem::strategy;
using namespace ubse::utils;
using namespace ubse::adapter_plugins::mmi;

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

template <typename T>
UbseMemResult GetShmStageByObj(const T &obj)
{
    UbseMemResult result{};
    result.name = obj.req.name;
    switch (obj.status.state) {
        case UBSE_MEM_EXPORT_RUNNING:
        case UBSE_MEM_IMPORT_RUNNING:
            result.stage = UbseMemStage::UBSE_CREATING;
            break;
        case UBSE_MEM_EXPORT_DESTROYING:
        case UBSE_MEM_IMPORT_DESTROYING:
            result.stage = UbseMemStage::UBSE_DELETING;
            break;
        case UBSE_MEM_EXPORT_DESTROYED:
        case UBSE_MEM_IMPORT_DESTROYED:
            result.stage = UbseMemStage::UBSE_NOT_EXIST;
            break;
        case UBSE_MEM_EXPORT_SUCCESS:
        case UBSE_MEM_IMPORT_SUCCESS:
            result.stage = UbseMemStage::UBSE_EXIST;
            break;
        default:
            break;
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
    error_t cpyRet =
        memcpy_s(shmDesc.userInfo, UBSE_MAX_USR_INFO_LEN, exportObjPtr->req.usrInfo, UBSE_MAX_USR_INFO_LEN);
    if (cpyRet != UBSE_OK) {
        UBSE_LOG_WARN << "userInfo create failed" << name;
    }
    if (exportObjPtr->algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_WARN << "The exportObj with empty export numa infos will be ignored, name=" << name;
        return;
    }
    const std::string exportNodeId = exportObjPtr->algoResult.exportNumaInfos[0].nodeId;
    ubse::nodeController::UbseNodeGetByNodeIdInMaster(exportNodeId, shmDesc.exportNode);
    UbseMemResult result = GetShmStageByObj(*exportObjPtr);
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
namespace {
bool IsUsrInfoEmpty(const uint8_t (&usrInfo)[UBSE_MAX_USR_INFO_LEN])
{
    for (const auto v : usrInfo) {
        if (v != 0) {
            return false;
        }
    }
    return true;
}
} // namespace

uint32_t AssignImportInfo(const UbseMemDebtQueryRequest &request,
                          std::vector<std::shared_ptr<const UbseMemShareBorrowImportObj>> &importObjPtrs,
                          UbseMemShmDesc &shmDesc)
{
    const std::string name = request.name;
    // 填充导入相关数据
    auto importObj = importObjPtrs[0];
    error_t cpyRet = memcpy_s(shmDesc.userInfo, UBSE_MAX_USR_INFO_LEN, importObj->req.usrInfo, UBSE_MAX_USR_INFO_LEN);
    if (cpyRet != UBSE_OK) {
        UBSE_LOG_WARN << "userInfo create from importObj failed, name=" << name;
    }
    shmDesc.importDesc.clear();
    for (const auto &importObjPtr : importObjPtrs) {
        if (!request.importNodeId.empty() && importObjPtr->importNodeId != request.importNodeId) {
            continue;
        }
        if (shmDesc.name.empty()) {
            shmDesc.name = name;
        }
        def::UbseMemShmImportDesc importDesc;
        for (const auto& obmmInfo : importObjPtr->status.importResults) {
            importDesc.memIds.push_back(obmmInfo.memId);
        }
        std::string importNodeId = importObjPtr->importNodeId;
        nodeController::UbseNodeGetByNodeIdInMaster(importNodeId, importDesc.importNode);
        UbseMemResult memResult = GetShmStageByObj(*importObjPtr);
        importDesc.state = memResult.stage;
        shmDesc.importDesc.push_back(importDesc);
        // export缺失或未填充usr_info时，从importObj兜底返回调用方私有数据（importObj.req继承自exportObj.req）
        if (IsUsrInfoEmpty(shmDesc.userInfo)) {
            error_t cpyRet =
                memcpy_s(shmDesc.userInfo, UBSE_MAX_USR_INFO_LEN, importObjPtr->req.usrInfo, UBSE_MAX_USR_INFO_LEN);
            if (cpyRet != UBSE_OK) {
                UBSE_LOG_WARN << "userInfo create from importObj failed, name=" << name;
            }
        }
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

    auto doQuery = [&](IShareStore &store) -> uint32_t {
        bool found = false;
        UbseMemShareBorrowExportObj exportObj;
        if (store.LoadExport(request.name, exportObj) == UBSE_OK) {
            if (!exportObj.req.udsInfo.CheckPermission(request.udsInfo)) {
                UBSE_LOG_ERROR << "Permission denied. related name: " << request.name;
                return UBSE_ERR_AUTH_FAILED;
            }
            auto exportObjPtr = std::make_shared<const UbseMemShareBorrowExportObj>(std::move(exportObj));
            ShmDecExportAssignment(request.name, shmDesc, exportObjPtr);
            found = true;
        }

        std::vector<UbseMemShareBorrowImportObj> importObjs;
        store.LoadAllImports(request.name, importObjs);
        std::vector<std::shared_ptr<const UbseMemShareBorrowImportObj>> allImportObjs;
        for (auto &obj : importObjs) {
            if (!request.importNodeId.empty() && obj.importNodeId != request.importNodeId) {
                continue;
            }
            if (obj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
                continue;
            }
            allImportObjs.push_back(std::make_shared<const UbseMemShareBorrowImportObj>(std::move(obj)));
        }

        UBSE_LOG_INFO << "Total import objects found for name=" << request.name << ": " << allImportObjs.size();
        if (!allImportObjs.empty()) {
            found = true;
            if (auto ret = AssignImportInfo(request, allImportObjs, shmDesc); ret != UBSE_OK) {
                UBSE_LOG_ERROR << "AssignImportInfo failed, ret: " << FormatRetCode(ret);
                return ret;
            }
        }

        return found ? UBSE_OK : UBSE_ERR_NOT_EXIST;
    };
    if (UbseCheckWithoutGlobalMasterNodeId()) {
        CascadeMasterStore store;
        return doQuery(store);
    } else {
        GlobalMasterStore store;
        return doQuery(store);
    }
}

static void ProcessExportObjects(const def::UbseMemDebtQueryRequest &request,
                                 std::unordered_map<std::string, def::UbseMemShmDesc> &descMap,
                                 IShareStore &store)
{
    store.ForEachExport([&](const std::string &nodeId, const std::string &name,
                             const UbseMemShareBorrowExportObj &exportObj) {
        if (!request.name.empty() && name.rfind(request.name, 0) != 0) {
            return;
        }
        if (descMap.find(name) != descMap.end()) {
            return;
        }
        if (exportObj.algoResult.exportNumaInfos.empty()) {
            UBSE_LOG_WARN << "ExportObj with empty export numa infos, name=" << name;
            return;
        }
        if (!exportObj.req.udsInfo.CheckPermission(request.udsInfo)) {
            return;
        }
        auto exportObjPtr = std::make_shared<const UbseMemShareBorrowExportObj>(exportObj);
        def::UbseMemShmDesc shmDesc{};
        ShmDecExportAssignment(name, shmDesc, exportObjPtr);
        descMap[name] = std::move(shmDesc);
    });
}

static void ProcessImportObjects(const def::UbseMemDebtQueryRequest &request,
                                  std::unordered_map<std::string, def::UbseMemShmDesc> &descMap,
                                  IShareStore &store)
{
    store.ForEachImport([&](const std::string &nodeId, const std::string &name,
                             const UbseMemShareBorrowImportObj &importObj) {
        if (!request.name.empty() && name.rfind(request.name, 0) != 0) {
            return;
        }
        if (!request.importNodeId.empty() && nodeId != request.importNodeId) {
            return;
        }
        if (!importObj.req.udsInfo.CheckPermission(request.udsInfo)) {
            return;
        }
        auto [it, inserted] = descMap.try_emplace(name);
        auto &shmDesc = it->second;
        if (inserted) {
            shmDesc.name = name;
            if (memcpy_s(shmDesc.userInfo, UBSE_MAX_USR_INFO_LEN, importObj.req.usrInfo, UBSE_MAX_USR_INFO_LEN) !=
                UBSE_OK) {
                UBSE_LOG_WARN << "userInfo create from importObj failed, name=" << name;
            }
        }
        def::UbseMemShmImportDesc importDesc;
        for (const auto &result : importObj.status.importResults) {
            importDesc.memIds.push_back(result.memId);
        }
        nodeController::UbseNodeGetByNodeIdInMaster(nodeId, importDesc.importNode);
        UbseMemResult memResult = GetShmStageByObj(importObj);
        importDesc.state = memResult.stage;
        shmDesc.importDesc.push_back(std::move(importDesc));
    });
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

    auto doList = [&](IShareStore &store) {
        ProcessExportObjects(request, descMap, store);
        ProcessImportObjects(request, descMap, store);
        FillResultWithLimit(descMap, shmDescs);
    };

    if (UbseCheckWithoutGlobalMasterNodeId()) {
        CascadeMasterStore store;
        doList(store);
    } else {
        GlobalMasterStore store;
        doList(store);
    }

    return UBSE_OK;
}

static void FillShmFaultInfo(const UbseMemShareBorrowExportObj &exportObj,
                              UbseMemShmMemStatusDesc &shmStatusDesc)
{
    for (const auto &obmmInfo : exportObj.status.exportObmmInfo) {
        if (obmmInfo.memIdStatus == UB_MEM_HEALTHY) {
            continue;
        }
        shmStatusDesc.memIds.push_back(obmmInfo.memId);
        shmStatusDesc.faultTypes.push_back(obmmInfo.memIdStatus);
    }
}

uint32_t UbseMemShmStatusGet(const UbseMemDebtQueryRequest &request, def::UbseMemShmMemStatusDesc &shmStatusDesc)
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

    UbseMemShareBorrowExportObj exportObj;
    if (UbseCheckWithoutGlobalMasterNodeId()) {
        CascadeMasterStore store;
        if (auto ret = store.LoadExport(request.name, exportObj); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "GetExportItem from summary failed, name=" << request.name
                           << ", ret=" << FormatRetCode(ret);
            return UBSE_ERR_NOT_EXIST;
        }
    } else {
        GlobalMasterStore store;
        if (auto ret = store.LoadExport(request.name, exportObj); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "No export information found. related name: " << request.name;
            return UBSE_ERR_NOT_EXIST;
        }
    }

    FillShmFaultInfo(exportObj, shmStatusDesc);
    return UBSE_OK;
}

UbseMemResult GetShmExportStageByObj(const std::string& name)
{
    auto doQuery = [&](IShareStore &store) -> UbseMemResult {
        UbseMemShareBorrowExportObj exportObj;
        if (store.LoadExport(name, exportObj) != UBSE_OK) {
            UbseMemResult result{};
            result.name = name;
            result.stage = UbseMemStage::UBSE_NOT_EXIST;
            return result;
        }
        return GetShmStageByObj(exportObj);
    };

    if (UbseCheckWithoutGlobalMasterNodeId()) {
        CascadeMasterStore store;
        return doQuery(store);
    }
    GlobalMasterStore store;
    return doQuery(store);
}

UbseMemResult GetShmImportStageByObj(const std::string& name, const std::string& importNodeId)
{
    auto doQuery = [&](IShareStore &store) -> UbseMemResult {
        UbseMemShareBorrowImportObj importObj;
        if (store.LoadImport(importNodeId, name, importObj) != UBSE_OK) {
            UbseMemResult result{};
            result.name = name;
            result.stage = UbseMemStage::UBSE_NOT_EXIST;
            return result;
        }
        return GetShmStageByObj(importObj);
    };

    if (UbseCheckWithoutGlobalMasterNodeId()) {
        CascadeMasterStore store;
        return doQuery(store);
    }
    GlobalMasterStore store;
    return doQuery(store);
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
        UBSE_LOG_WARN << "nodeId=" << nodeId << ", name=" << name << " is not in debt.";
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
        UBSE_LOG_WARN << "name=" << name << ", nodeId=" << nodeId << " is not in debt.";
        return {};
    }
    return *importObjPtr;
}
} // namespace ubse::mem::controller::debt
