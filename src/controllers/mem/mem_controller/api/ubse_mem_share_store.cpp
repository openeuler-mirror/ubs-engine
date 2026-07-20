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

#include "ubse_mem_share_store.h"

#include <securec.h>

#include "ubse_logger.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_mem_global_ledger_summary.h"
#include "ubse_mem_global_ledger_summary_store.h"
#include "ubse_node.h"
#include "ubse_node_controller.h"
#include "ubse_topo_util.h"
#include "ubse_str_util.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");

using ubse::adapter_plugins::mmi::UbseMemObmmInfo;
using namespace ubse::mem::controller::debt;
using namespace ubse::nodeController;

namespace {
std::string ExportNodeIdOf(const UbseMemShareBorrowExportObj &obj)
{
    if (obj.algoResult.exportNumaInfos.empty()) {
        return {};
    }
    return obj.algoResult.exportNumaInfos[0].nodeId;
}
} // namespace

// ==================== CascadeMasterStore（全量对象） ====================

bool CascadeMasterStore::ExistBorrow(const std::string &name)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    auto allExportNodeMaps = ledger.GetDebtMap<UbseMemShareBorrowExportObj>().GetAllNodeMaps();
    for (const auto &[nodeId, nodeMap] : allExportNodeMaps) {
        if (!nodeMap) {
            continue;
        }
        for (const auto &[resId, objPtr] : nodeMap->GetAll()) {
            if (objPtr && objPtr->req.name == name && objPtr->status.state != UBSE_MEM_EXPORT_DESTROYED) {
                return true;
            }
        }
    }
    auto allImportNodeMaps = ledger.GetDebtMap<UbseMemShareBorrowImportObj>().GetAllNodeMaps();
    for (const auto &[nodeId, nodeMap] : allImportNodeMaps) {
        if (!nodeMap) {
            continue;
        }
        for (const auto &[resId, objPtr] : nodeMap->GetAll()) {
            if (objPtr && objPtr->req.name == name && objPtr->status.state != UBSE_MEM_IMPORT_DESTROYED) {
                return true;
            }
        }
    }
    return false;
}

bool CascadeMasterStore::ExistAttach(const std::string &importNodeId, const std::string &name)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    return ledger.GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(importNodeId, name) != nullptr;
}

UbseResult CascadeMasterStore::LoadExport(const std::string &name, UbseMemShareBorrowExportObj &out)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    auto ptr = ledger.GetDebtMap<UbseMemShareBorrowExportObj>().GetExportResourceByResId(name);
    if (!ptr) {
        return UBSE_ERR_NOT_EXIST;
    }
    out = *ptr;
    return UBSE_OK;
}

UbseResult CascadeMasterStore::LoadImport(const std::string &importNodeId, const std::string &name,
                                       UbseMemShareBorrowImportObj &out)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    auto ptr = ledger.GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(importNodeId, name);
    if (!ptr) {
        return UBSE_ERR_NOT_EXIST;
    }
    out = *ptr;
    return UBSE_OK;
}

UbseResult CascadeMasterStore::LoadAllImports(const std::string &name,
                                           std::vector<UbseMemShareBorrowImportObj> &out)
{
    out.clear();
    auto &ledger = UbseMemDebtLedger::GetInstance();
    auto allNodeMaps = ledger.GetDebtMap<UbseMemShareBorrowImportObj>().GetAllNodeMaps();
    for (const auto &[nodeId, nodeMap] : allNodeMaps) {
        if (!nodeMap) {
            continue;
        }
        auto ptr = nodeMap->Get(name);
        if (ptr) {
            out.push_back(*ptr);
        }
    }
    return UBSE_OK;
}

void CascadeMasterStore::PutExport(const UbseMemShareBorrowExportObj &obj)
{
    auto exportNodeId = ExportNodeIdOf(obj);
    if (exportNodeId.empty()) {
        UBSE_LOG_WARN << "PutExport ignored, empty exportNumaInfos, name=" << obj.req.name;
        return;
    }
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource(
        exportNodeId, obj.req.name, obj);
}

void CascadeMasterStore::PutImport(const UbseMemShareBorrowImportObj &obj)
{
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource(
        obj.importNodeId, obj.req.name, obj);
}

void CascadeMasterStore::UpdateExportState(const UbseMemShareBorrowExportObj &obj, const UbseMemState &state)
{
    auto exportNodeId = ExportNodeIdOf(obj);
    if (exportNodeId.empty()) {
        return;
    }
    auto copy = obj;
    copy.status.state = state;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource(
        exportNodeId, copy.req.name, copy);
}

void CascadeMasterStore::UpdateImportState(const UbseMemShareBorrowImportObj &obj, const UbseMemState &state)
{
    auto copy = obj;
    copy.status.state = state;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource(
        copy.importNodeId, copy.req.name, copy);
}

void CascadeMasterStore::UpdateExportMemIds(const UbseMemShareBorrowExportObj &obj)
{
    // 全量账本中 memId 已随对象整体落盘，PutExport 即包含 memId，无需单独回填。
    PutExport(obj);
}

void CascadeMasterStore::UpdateImportMemIds(const UbseMemShareBorrowImportObj &obj)
{
    PutImport(obj);
}

void CascadeMasterStore::RemoveExport(const UbseMemShareBorrowExportObj &obj)
{
    auto exportNodeId = ExportNodeIdOf(obj);
    if (exportNodeId.empty()) {
        return;
    }
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().RemoveResource(
        exportNodeId, obj.req.name);
}

void CascadeMasterStore::RemoveImport(const UbseMemShareBorrowImportObj &obj)
{
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().RemoveResource(
        obj.importNodeId, obj.req.name);
}

void CascadeMasterStore::ForEachExport(IShareStore::ExportVisitor visitor)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    auto allNodeMaps = ledger.GetDebtMap<UbseMemShareBorrowExportObj>().GetAllNodeMaps();
    for (const auto &[nodeId, nodeMap] : allNodeMaps) {
        if (!nodeMap) {
            continue;
        }
        for (const auto &[name, objPtr] : nodeMap->GetAll()) {
            if (objPtr) {
                visitor(nodeId, name, *objPtr);
            }
        }
    }
}

void CascadeMasterStore::ForEachImport(IShareStore::ImportVisitor visitor)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    auto allNodeMaps = ledger.GetDebtMap<UbseMemShareBorrowImportObj>().GetAllNodeMaps();
    for (const auto &[nodeId, nodeMap] : allNodeMaps) {
        if (!nodeMap) {
            continue;
        }
        for (const auto &[name, objPtr] : nodeMap->GetAll()) {
            if (objPtr) {
                visitor(nodeId, name, *objPtr);
            }
        }
    }
}

namespace {

std::string GetChipInfoBySocketId(const ubse::nodeController::UbseNodeInfo &nodeInfo, uint32_t socketId)
{
    for (const auto &[loc, cpuInfo] : nodeInfo.cpuInfos) {
        if (cpuInfo.socketId == socketId) {
            UBSE_LOG_INFO << "chipId=" << cpuInfo.chipId;
            return cpuInfo.chipId;
        }
    }
    UBSE_LOG_ERROR << "Can't find chipId in nodeInfo, socketId=" << socketId << " node Id=" << nodeInfo.nodeId;
    return "";
}

void GetPortCnaByPortId(const ubse::nodeController::UbseNodeInfo &remoteInfo, const std::string &importChipId,
                        const uint32_t portId, uint32_t &portCna)
{
    for (const auto &cpuInfo : remoteInfo.cpuInfos) {
        for (const auto &[key, portInfo] : cpuInfo.second.portInfos) {
            if (portInfo.remoteChipId != importChipId || portInfo.portId != std::to_string(portId)) {
                continue;
            }
            portCna = portInfo.portCna;
            return;
        }
    }
    portCna = UINT32_MAX;
}

void GetMinPortInfoByCpuInfo(const ubse::nodeController::UbseNodeInfo &remoteInfo, const std::string &importChipId,
                             uint32_t &minPortId, uint32_t &minPortCna)
{
    minPortId = UINT32_MAX;
    minPortCna = UINT32_MAX;
    for (const auto &cpuInfo : remoteInfo.cpuInfos) {
        for (const auto &[key, portInfo] : cpuInfo.second.portInfos) {
            if (portInfo.remoteChipId != importChipId) {
                continue;
            }
            uint32_t selfPortId{};
            auto ret = ubse::utils::ConvertStrToUint32(portInfo.portId, selfPortId);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "portGroupIdStr(" << portInfo.portId << ") is invalid.";
                minPortId = UINT32_MAX;
                return;
            }
            if (minPortId > selfPortId) {
                minPortId = selfPortId;
                minPortCna = portInfo.portCna;
            }
        }
    }
}

uint32_t GetPortInfo(const std::string &importNodeId, const UbseMemShareBorrowImportObj &importObj,
                     const std::string &remoteNode, uint32_t &portId, uint32_t &portCna)
{
    auto nodeInfo = ubse::nodeController::UbseNodeController::GetInstance().GetNodeById(importNodeId);
    auto remoteInfo = ubse::nodeController::UbseNodeController::GetInstance().GetNodeById(remoteNode);
    std::string importChipId{};
    ubse::nodeController::UbseNodeMemCnaInfoInput cnaInput{};
    cnaInput.borrowNodeId = importNodeId;
    cnaInput.exportNodeId = remoteNode;
    cnaInput.exportSocketId = std::to_string(importObj.algoResult.exportNumaInfos[0].socketId);
    ubse::nodeController::UbseNodeMemCnaInfoOutput cnaOutput;
    if (auto ret = ubse::nodeController::UbseNodeMemGetTopologyCnaInfo(cnaInput, cnaOutput); ret != 0) {
        UBSE_LOG_ERROR << "Failed to get cna, borrowNodeId=" << cnaInput.borrowNodeId
                       << ", exportNodeId=" << cnaInput.exportNodeId << ", socketId=" << cnaInput.exportSocketId;
        return UBSE_ERROR;
    }

    uint32_t borrowSocketId{};
    auto ret = ubse::utils::ConvertStrToUint32(cnaOutput.borrowSocketId, borrowSocketId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "portGroupIdStr(" << cnaOutput.borrowSocketId << ") is invalid.";
        return ret;
    }

    importChipId = GetChipInfoBySocketId(nodeInfo, borrowSocketId);
    if (importChipId.empty()) {
        return UBSE_ERROR;
    }
    if (importObj.req.lenderInfo.portId != UINT32_MAX) {
        GetPortCnaByPortId(remoteInfo, importChipId, portId, portCna);
    } else {
        GetMinPortInfoByCpuInfo(remoteInfo, importChipId, portId, portCna);
    }
    UBSE_LOG_INFO << "minPortCna=" << portCna << ", min portId=" << portId;
    if (portCna == UINT32_MAX) {
        UBSE_LOG_ERROR << "Failed Get minPortCna from topoInfo";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

} // namespace

uint32_t CascadeMasterStore::GetCnaTopo(const UbseMemShareAttachReq &req,
                                         const UbseMemShareBorrowExportObj &exportObj,
                                         UbseMemShareBorrowImportObj &importObj)
{
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "exportObj.algoResult.exportNumaInfos is empty";
        return UBSE_ERROR;
    }
    auto remoteNode = exportObj.algoResult.exportNumaInfos[0].nodeId;
    if (exportObj.algoResult.exportNumaInfos[0].nodeId == req.importNodeId) {
        return UBSE_OK;
    }

    UBSE_LOG_INFO << "req info: importNodeId=" << req.importNodeId << ", exportNodeId=" << remoteNode
                  << " export socketId=" << exportObj.algoResult.exportNumaInfos[0].socketId;
    auto ret = GetCnaInfoWhenImport(exportObj.algoResult.exportNumaInfos[0].nodeId, req.importNodeId, importObj);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get cna info when import, " << ubse::log::FormatRetCode(ret)
                       << ", requestId=" << req.requestId;
        return UBSE_ERROR;
    }
    // 多路径情况下，会给dcna重新赋值，当指定port时以指定port为准，否则选择直连导入socket的最小端口号，作为单路径路由表的配置
    if (ubse::utils::IsSameSocketMultiPortTopo()) {
        uint32_t minPortId = UINT32_MAX;
        uint32_t minPortCna{};
        if (GetPortInfo(req.importNodeId, importObj, remoteNode, minPortId, minPortCna) != UBSE_OK) {
            return UBSE_ERROR;
        }
        for (auto &obmmInfo : importObj.exportObmmInfo) {
            obmmInfo.desc.dcna = minPortCna;
            obmmInfo.desc.marId = minPortId / 4; // minPortId / 4 能得到marId
        }
    }
    return UBSE_OK;
}

// ==================== GlobalMasterStore（摘要项） ====================

bool GlobalMasterStore::ExistBorrow(const std::string &name)
{
    return UbseGlobalLedgerSummaryStore::GetInstance().ContainsBorrowName(name);
}

bool GlobalMasterStore::ExistAttach(const std::string &importNodeId, const std::string &name)
{
    return UbseGlobalLedgerSummaryStore::GetInstance().ContainsAttachName(importNodeId, name);
}

UbseResult GlobalMasterStore::LoadExport(const std::string &name, UbseMemShareBorrowExportObj &out)
{
    return UbseGlobalLedgerSummaryStore::GetInstance().GetExportItem(name, out);
}

UbseResult GlobalMasterStore::LoadImport(const std::string &importNodeId, const std::string &name,
                                          UbseMemShareBorrowImportObj &out)
{
    return UbseGlobalLedgerSummaryStore::GetInstance().GetImportItem(name, importNodeId, out);
}

UbseResult GlobalMasterStore::LoadAllImports(const std::string &name,
                                              std::vector<UbseMemShareBorrowImportObj> &out)
{
    out.clear();
    std::vector<std::pair<std::string, UbseGlobalLedgerSummaryItem>> items;
    UbseGlobalLedgerSummaryStore::GetInstance().GetAllImportItems(items, name);
    for (const auto &[nodeId, item] : items) {
        UbseMemShareBorrowImportObj obj{};
        obj.importNodeId = nodeId;
        obj.req.name = item.name;
        obj.algoResult.blockSize = item.blockSize;
        obj.status.state = item.state;
        obj.req.udsInfo = item.userInfo;
        if (memcpy_s(obj.req.usrInfo, UBSE_MAX_USR_INFO_LEN, item.usrInfo, UBSE_MAX_USR_INFO_LEN) != EOK) {
            UBSE_LOG_WARN << "copy usrInfo failed when load all imports, name=" << item.name;
        }
        for (const auto &memId : item.memids) {
            UbseMemImportResult importResult{};
            importResult.memId = memId;
            obj.status.importResults.push_back(importResult);
        }
        out.push_back(std::move(obj));
    }
    return UBSE_OK;
}

void GlobalMasterStore::PutExport(const UbseMemShareBorrowExportObj &obj)
{
    auto exportNodeId = ExportNodeIdOf(obj);
    if (exportNodeId.empty()) {
        UBSE_LOG_WARN << "PutExport(summary) ignored, empty exportNumaInfos, name=" << obj.req.name;
        return;
    }
    UbseGlobalLedgerSummaryItem exportItem;
    exportItem.name = obj.req.name;
    exportItem.blockSize = obj.algoResult.blockSize;
    exportItem.state = obj.status.state;
    exportItem.numaInfos = obj.algoResult.exportNumaInfos;
    exportItem.userInfo = obj.req.udsInfo;
    if (memcpy_s(exportItem.usrInfo, UBSE_MAX_USR_INFO_LEN, obj.req.usrInfo, UBSE_MAX_USR_INFO_LEN) != EOK) {
        UBSE_LOG_WARN << "copy usrInfo failed when put export summary, name=" << obj.req.name;
    }
    exportItem.shmAnonymous = obj.req.shmAnonymous;
    for (const auto &node : obj.req.shmRegion.nodelist) {
        exportItem.nodelist.emplace_back(node.nodeId);
    }
    UbseGlobalLedgerSummaryStore::GetInstance().PutNodeExportItem(exportNodeId, exportItem);
}

void GlobalMasterStore::PutImport(const UbseMemShareBorrowImportObj &obj)
{
    UbseGlobalLedgerSummaryItem importItem;
    importItem.name = obj.req.name;
    importItem.state = obj.status.state;
    importItem.userInfo = obj.req.udsInfo;
    if (memcpy_s(importItem.usrInfo, UBSE_MAX_USR_INFO_LEN, obj.req.usrInfo, UBSE_MAX_USR_INFO_LEN) != EOK) {
        UBSE_LOG_WARN << "copy usrInfo failed when put import summary, name=" << obj.req.name;
    }
    importItem.blockSize = obj.algoResult.blockSize;
    for (const auto &node : obj.req.shmRegion.nodelist) {
        importItem.nodelist.emplace_back(node.nodeId);
    }
    UbseGlobalLedgerSummaryStore::GetInstance().PutNodeImportItem(obj.importNodeId, importItem);
}

void GlobalMasterStore::UpdateExportState(const UbseMemShareBorrowExportObj &obj, const UbseMemState &state)
{
    auto exportNodeId = ExportNodeIdOf(obj);
    if (exportNodeId.empty()) {
        return;
    }
    UbseGlobalLedgerSummaryStore::GetInstance().UpdateNodeExportItem(exportNodeId, obj.req.name, state);
}

void GlobalMasterStore::UpdateImportState(const UbseMemShareBorrowImportObj &obj, const UbseMemState &state)
{
    UbseGlobalLedgerSummaryStore::GetInstance().UpdateNodeImportItem(obj.importNodeId, obj.req.name, state);
}

void GlobalMasterStore::UpdateExportMemIds(const UbseMemShareBorrowExportObj &obj)
{
    auto exportNodeId = ExportNodeIdOf(obj);
    if (exportNodeId.empty()) {
        return;
    }
    std::vector<uint16_t> memIds;
    std::vector<UbMemFaultType> faultTypes;
    for (const auto &obmmInfo : obj.status.exportObmmInfo) {
        memIds.push_back(static_cast<uint16_t>(obmmInfo.memId));
        faultTypes.push_back(obmmInfo.memIdStatus);
    }
    UbseGlobalLedgerSummaryStore::GetInstance().UpdateNodeExportItemMemIds(exportNodeId, obj.req.name, memIds, faultTypes);
}

void GlobalMasterStore::UpdateImportMemIds(const UbseMemShareBorrowImportObj &obj)
{
    std::vector<uint16_t> memIds;
    for (const auto &importResult : obj.status.importResults) {
        memIds.push_back(static_cast<uint16_t>(importResult.memId));
    }
    UbseGlobalLedgerSummaryStore::GetInstance().UpdateNodeImportItemMemIds(obj.importNodeId, obj.req.name, memIds, {});
}

void GlobalMasterStore::RemoveExport(const UbseMemShareBorrowExportObj &obj)
{
    auto exportNodeId = ExportNodeIdOf(obj);
    if (exportNodeId.empty()) {
        return;
    }
    UbseGlobalLedgerSummaryStore::GetInstance().RemoveNodeExportItem(exportNodeId, obj.req.name);
}

void GlobalMasterStore::RemoveImport(const UbseMemShareBorrowImportObj &obj)
{
    UbseGlobalLedgerSummaryStore::GetInstance().RemoveNodeImportItem(obj.importNodeId, obj.req.name);
}

void GlobalMasterStore::ForEachExport(IShareStore::ExportVisitor visitor)
{
    UbseGlobalNodeLedgerSummaryTable summaries;
    if (UbseGlobalLedgerSummaryStore::GetInstance().GetAllNodeSummaries(summaries) != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get all node summaries";
        return;
    }
    for (const auto &[nodeId, summary] : summaries) {
        for (const auto &[name, item] : summary.shmSummary.exportItems) {
            UbseMemShareBorrowExportObj exportObj;
            exportObj.req.name = item.name;
            exportObj.algoResult.blockSize = item.blockSize;
            exportObj.algoResult.exportNumaInfos = item.numaInfos;
            exportObj.req.size = 0;
            for (const auto &numaInfo : item.numaInfos) {
                exportObj.req.size += numaInfo.size;
            }
            exportObj.status.state = item.state;
            exportObj.req.udsInfo = item.userInfo;
            if (memcpy_s(exportObj.req.usrInfo, UBSE_MAX_USR_INFO_LEN, item.usrInfo, UBSE_MAX_USR_INFO_LEN) != EOK) {
                UBSE_LOG_WARN << "copy usrInfo failed when foreach export, name=" << item.name;
            }
            exportObj.req.shmAnonymous = item.shmAnonymous;
            for (size_t i = 0; i < item.memids.size(); ++i) {
                UbseMemObmmInfo obmmInfo{};
                obmmInfo.memId = item.memids[i];
                if (i < item.faultTypes.size()) {
                    obmmInfo.memIdStatus = item.faultTypes[i];
                }
                exportObj.status.exportObmmInfo.push_back(obmmInfo);
            }
            visitor(nodeId, name, exportObj);
        }
    }
}

void GlobalMasterStore::ForEachImport(IShareStore::ImportVisitor visitor)
{
    UbseGlobalNodeLedgerSummaryTable summaries;
    if (UbseGlobalLedgerSummaryStore::GetInstance().GetAllNodeSummaries(summaries) != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get all node summaries";
        return;
    }
    for (const auto &[nodeId, summary] : summaries) {
        for (const auto &[name, item] : summary.shmSummary.importItems) {
            UbseMemShareBorrowImportObj importObj;
            importObj.importNodeId = nodeId;
            importObj.req.name = item.name;
            importObj.algoResult.blockSize = item.blockSize;
            importObj.status.state = item.state;
            importObj.req.udsInfo = item.userInfo;
            if (memcpy_s(importObj.req.usrInfo, UBSE_MAX_USR_INFO_LEN, item.usrInfo, UBSE_MAX_USR_INFO_LEN) != EOK) {
                UBSE_LOG_WARN << "copy usrInfo failed when foreach import, name=" << item.name;
            }
            for (const auto &memId : item.memids) {
                UbseMemImportResult importResult{};
                importResult.memId = memId;
                importObj.status.importResults.push_back(importResult);
            }
            visitor(nodeId, name, importObj);
        }
    }
}

uint32_t GlobalMasterStore::GetCnaTopo(const UbseMemShareAttachReq &req,
                                             const UbseMemShareBorrowExportObj &exportObj,
                                             UbseMemShareBorrowImportObj &importObj)
{
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "exportObj.algoResult.exportNumaInfos is empty";
        return UBSE_ERROR;
    }
    auto remoteNode = exportObj.algoResult.exportNumaInfos[0].nodeId;
    if (exportObj.algoResult.exportNumaInfos[0].nodeId == req.importNodeId) {
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "req info: importNodeId=" << req.importNodeId << ", exportNodeId=" << remoteNode
                  << " export socketId=" << exportObj.algoResult.exportNumaInfos[0].socketId;
    auto ret = GetCnaInfoWhenImportClos(exportObj.algoResult.exportNumaInfos[0].nodeId, req.importNodeId, importObj);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get cna info when import, " << ubse::log::FormatRetCode(ret)
                       << ", requestId=" << req.requestId;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

} // namespace ubse::mem::controller
