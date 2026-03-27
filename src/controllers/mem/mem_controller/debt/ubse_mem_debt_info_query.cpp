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

#include "ubse_mem_debt_info_query.h"

#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_node_controller.h"

namespace ubse::mem::controller::debt {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::election;

inline uint64_t BytesToMB(uint64_t bytes)
{
    constexpr uint64_t BYTE_TO_MB = 1024ULL * 1024ULL;
    return bytes / (BYTE_TO_MB);
}

template <typename ImportT, typename ExportT>
struct DebtTypeTraits {
    using ImportType = ImportT;
    using ExportType = ExportT;
};

using FdDebtTraits = DebtTypeTraits<UbseMemFdBorrowImportObj, UbseMemFdBorrowExportObj>;
using NumaDebtTraits = DebtTypeTraits<UbseMemNumaBorrowImportObj, UbseMemNumaBorrowExportObj>;
using AddrDebtTraits = DebtTypeTraits<UbseMemAddrBorrowImportObj, UbseMemAddrBorrowExportObj>;

/**
 * 通用模板：按 Export 遍历并匹配 Import，累加 size 到 nodeBorrows
 * @tparam Policy 定义所有类型差异
 * @param debtInfoMap 全量账本
 * @param nodeBorrows 统计借用数据
 */
template <typename Traits>
void CollectGenericImportSize(std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> &nodeBorrows)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    auto &exportMap = ledger.GetDebtMap<typename Traits::ExportType>();
    auto &importMap = ledger.GetDebtMap<typename Traits::ImportType>();
    UBSE_LOG_DEBUG << "CollectGenericImportSize start";

    for (const auto &[exportNodeId, nodeExportMap] : exportMap.GetAllNodeMaps()) {
        if (!nodeExportMap) {
            continue;
        }
        for (const auto &[resId, exportObjPtr] : nodeExportMap->GetAll()) {
            if (!exportObjPtr) {
                continue;
            }
            if (exportObjPtr->status.state != UBSE_MEM_EXPORT_SUCCESS) {
                continue;
            }
            const auto &importNodeId = exportObjPtr->req.importNodeId;
            auto importObjPtr = importMap.GetResource(importNodeId, exportObjPtr->req.name);
            if (!importObjPtr) {
                UBSE_LOG_DEBUG << "Import resource not found, exportNodeId: " << exportNodeId
                               << ", importNodeId: " << importNodeId << ", name: " << exportObjPtr->req.name;
                continue;
            }
            if (importObjPtr->status.state != UBSE_MEM_IMPORT_SUCCESS) {
                continue;
            }
            uint64_t size = 0;
            for (const auto &numaInfo : importObjPtr->algoResult.exportNumaInfos) {
                size += numaInfo.size;
            }
            nodeBorrows[exportNodeId][importNodeId] += BytesToMB(size);
        }
    }
}

uint32_t UbseMemNodeBorrowQuery(std::vector<UbseNodeBorrowInfo> &nodeBorrowInfo)
{
    // 获取当前节点
    UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
        return ret;
    }
    if (currentRoleInfo.nodeRole != ELECTION_ROLE_MASTER) {
        UBSE_LOG_ERROR << "current role is not master. nodeId: " << currentRoleInfo.nodeId;
        return UBSE_ERROR;
    }
    // 聚集fd numa addr类型借用size
    std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> nodeBorrowSizeMap;
    CollectGenericImportSize<FdDebtTraits>(nodeBorrowSizeMap);
    CollectGenericImportSize<NumaDebtTraits>(nodeBorrowSizeMap);
    CollectGenericImportSize<AddrDebtTraits>(nodeBorrowSizeMap);
    // build resp
    auto nodeInfos = nodeController::UbseNodeController::GetInstance().GetAllNodes();
    for (const auto &[exportNodeId, importNodeInfo] : nodeBorrowSizeMap) {
        for (const auto &[importNodeId, size] : importNodeInfo) {
            if (nodeInfos.find(importNodeId) == nodeInfos.end() || nodeInfos.find(exportNodeId) == nodeInfos.end()) {
                UBSE_LOG_WARN << "The importNode or exportNode does not exist, importNodeId=" << importNodeId
                              << " , exportNodeId=" << exportNodeId;
                continue;
            }
            nodeBorrowInfo.push_back({nodeInfos[importNodeId].slotId, nodeInfos[importNodeId].hostName,
                                      nodeInfos[exportNodeId].slotId, nodeInfos[exportNodeId].hostName, size});
        }
    }
    return UBSE_OK;
}

template <typename ImportObjMap>
void CollectImportHandleDebtInfo(const ImportObjMap &importObjMap, const std::string &exportNodeId,
                                 ShareHandleInfoVec &importHandleInfo)
{
    for (const auto &[name, importObj] : importObjMap) {
        if (importObj.status.state != UBSE_MEM_IMPORT_SUCCESS &&
            importObj.status.state != UBSE_MEM_IMPORT_RUNNING) {
            UBSE_LOG_DEBUG << "[MEM_CONTROLLER] Skip " << name
                           << ", state=" << static_cast<uint32_t>(importObj.status.state)
                           << ", expect UBSE_MEM_IMPORT_SUCCESS or UBSE_MEM_IMPORT_RUNNING.";
            continue;
            }
        if (importObj.algoResult.exportNumaInfos.empty()) {
            UBSE_LOG_DEBUG << "[MEM_CONTROLLER] Skip " << name << ", export info is empty.";
            continue;
        }
        const auto &exportNodeInfo = importObj.algoResult.exportNumaInfos[0];
        if (exportNodeInfo.nodeId != exportNodeId) {
            UBSE_LOG_DEBUG << "[MEM_CONTROLLER] Skip " << name << ", exportNodeId=" << exportNodeInfo.nodeId
                           << ", expect=" << exportNodeId << ".";
            continue;
        }
        UBSE_LOG_DEBUG << "[MEM_CONTROLLER] Collected handle, name=" << name << ", exportNodeId=" << exportNodeId
                       << ".";
        std::vector<uint64_t> memIds;
        for (const auto &item : importObj.status.importResults) {
            memIds.push_back(item.memId);
        }
        importHandleInfo.push_back({name, std::move(memIds)});
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] CollectImportHandleDebtInfo done, exportNodeId=" << exportNodeId
                  << ", collected=" << importHandleInfo.size() << ".";
}

UbseResult UbseQueryShareImportHandleByExportNodeId(const std::string &importNodeId, const std::string &exportNodeId,
                                                    ShareHandleInfoVec &importHandleInfo)
{
    importHandleInfo.clear();
    if (importNodeId.empty() || exportNodeId.empty()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] importNodeId or exportNodeId is empty";
        return UBSE_ERROR_INVAL;
    }
    mapLock.LockRead();
    const auto it = nodeMemDebtInfoMap.find(importNodeId);
    if (it == nodeMemDebtInfoMap.end()) {
        mapLock.UnLock();
        UBSE_LOG_INFO << "[MEM_CONTROLLER] Import node " << importNodeId << " has no debt info";
        return UBSE_OK;
    }
    const auto &debtInfo = it->second;
    CollectImportHandleDebtInfo(debtInfo.shareImportObjMap, exportNodeId, importHandleInfo);
    mapLock.UnLock();
    return UBSE_OK;
}
} // namespace ubse::mem::controller::debt