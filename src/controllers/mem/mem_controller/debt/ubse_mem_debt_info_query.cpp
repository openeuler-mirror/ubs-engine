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
#include <cstdint>
#include <string>

#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_str_util.h"
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

template <typename T>
struct ImportObjTraits;

template <>
struct ImportObjTraits<UbseMemNumaBorrowImportObj> {
    static constexpr auto name = "numa";
    static constexpr bool isNumaType = true;
};

template <>
struct ImportObjTraits<UbseMemFdBorrowImportObj> {
    static constexpr auto name = "fd";
    static constexpr bool isNumaType = false;
};

template <>
struct ImportObjTraits<UbseMemShareBorrowImportObj> {
    static constexpr auto name = "share";
    static constexpr bool isNumaType = false;
};

template <typename ImportObjType>
bool ShouldSkipImportObj(const std::string &name, const ImportObjType &importObj, const std::string &exportNodeId)
{
    constexpr auto importType = ImportObjTraits<ImportObjType>::name;
    if (importObj.status.state != UBSE_MEM_IMPORT_SUCCESS &&
        importObj.status.state != UBSE_MEM_IMPORT_RUNNING) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] [" << importType << "] Skip " << name
                       << ", state=" << static_cast<uint32_t>(importObj.status.state)
                       << ", expect UBSE_MEM_IMPORT_SUCCESS or UBSE_MEM_IMPORT_RUNNING.";
        return true;
    }
    if (importObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] [" << importType << "] Skip " << name
                       << ", exportNumaInfos is empty, importResultsSize="
                       << importObj.status.importResults.size() << ".";
        return true;
    }
    if (const auto &exportNodeInfo = importObj.algoResult.exportNumaInfos[0];
        exportNodeInfo.nodeId != exportNodeId) {
        UBSE_LOG_DEBUG << "[MEM_CONTROLLER] [" << importType << "] Skip " << name
                       << ", exportNodeId=" << exportNodeInfo.nodeId
                       << ", expect=" << exportNodeId << ".";
        return true;
    }
    return false;
}

template <typename ImportObjType, typename HandleInfoVec>
void BuildHandleInfo(const std::string &name, const ImportObjType &importObj,
                     const std::string &exportNodeId, HandleInfoVec &importHandleInfo)
{
    constexpr auto importType = ImportObjTraits<ImportObjType>::name;
    if constexpr (ImportObjTraits<ImportObjType>::isNumaType) {
        std::unordered_set<int64_t> numaIds;
        for (const auto &result : importObj.status.importResults) {
            numaIds.insert(result.numaId);
        }
        UBSE_LOG_INFO << "[MEM_CONTROLLER] [" << importType << "] Collected handle, name=" << name
                       << ", exportNodeId=" << exportNodeId
                       << ", numaIdsSize=" << numaIds.size() << ".";
        importHandleInfo.push_back({name, std::move(numaIds), importObj.req.udsInfo});
    } else {
        std::unordered_set<uint64_t> memIds;
        for (const auto &result : importObj.status.importResults) {
            memIds.insert(result.memId);
        }
        UBSE_LOG_INFO << "[MEM_CONTROLLER] [" << importType << "] Collected handle, name=" << name
                       << ", exportNodeId=" << exportNodeId
                       << ", memIdsSize=" << memIds.size() << ".";
        importHandleInfo.push_back({name, std::move(memIds), importObj.req.udsInfo});
    }
}

template <typename ImportObjType, typename HandleInfoVec>
void CollectImportHandleDebtInfo(const UbseSharedPtrMap<ImportObjType> &importObjMap,
                                 const std::string &exportNodeId, HandleInfoVec &importHandleInfo)
{
    constexpr auto importType = ImportObjTraits<ImportObjType>::name;
    for (const auto &[name, importObjPtr] : importObjMap) {
        if (!importObjPtr) {
            UBSE_LOG_DEBUG << "[MEM_CONTROLLER] [" << importType << "] Skip " << name << ", importObjPtr is null.";
            continue;
        }
        if (ShouldSkipImportObj(name, *importObjPtr, exportNodeId)) {
            continue;
        }
        BuildHandleInfo(name, *importObjPtr, exportNodeId, importHandleInfo);
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] [" << importType << "] CollectImportHandleDebtInfo done, exportNodeId="
                  << exportNodeId << ", total=" << importObjMap.size()
                  << ", collected=" << importHandleInfo.size() << ".";
}

template <typename ImportObjType, typename HandleInfoVec>
UbseResult UbseQueryImportHandleByExportNodeId(const std::string &importNodeId, const std::string &exportNodeId,
                                               HandleInfoVec &importHandleInfo)
{
    importHandleInfo.clear();
    if (importNodeId.empty() || exportNodeId.empty()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] importNodeId or exportNodeId is empty";
        return UBSE_ERROR_INVAL;
    }
    auto &ledger = UbseMemDebtLedger::GetInstance();
    auto nodeMap = ledger.GetDebtMap<ImportObjType>().FindNodeMap(importNodeId);
    if (!nodeMap) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] Import node " << importNodeId << " has no import debt info";
        return UBSE_OK;
    }
    CollectImportHandleDebtInfo(nodeMap->GetAll(), exportNodeId, importHandleInfo);
    return UBSE_OK;
}

UbseResult UbseQueryShareImportHandleByExportNodeId(const std::string &importNodeId, const std::string &exportNodeId,
                                                    ShareHandleInfoVec &importHandleInfo)
{
    return UbseQueryImportHandleByExportNodeId<UbseMemShareBorrowImportObj>(importNodeId, exportNodeId,
                                                                            importHandleInfo);
}

UbseResult UbseQueryFdImportHandleByExportNodeId(const std::string &importNodeId, const std::string &exportNodeId,
                                                 FdHandleInfoVec &importHandleInfo)
{
    return UbseQueryImportHandleByExportNodeId<UbseMemFdBorrowImportObj>(importNodeId, exportNodeId, importHandleInfo);
}

UbseResult UbseQueryNumaImportHandleByExportNodeId(const std::string &importNodeId, const std::string &exportNodeId,
                                                   NumaHandleInfoVec &importHandleInfo)
{
    return UbseQueryImportHandleByExportNodeId<UbseMemNumaBorrowImportObj>(importNodeId, exportNodeId,
                                                                           importHandleInfo);
}

static bool IsPortInEnableList(const std::string &portId, const std::set<std::string> &portList)
{
    if (portList.empty()) {
        UBSE_LOG_DEBUG << "[MEM_CONTROLLER] EnablePortList portList is empty, portId=" << portId
                       << " treated as not in list";
        return false;
    }
    bool found = portList.find(portId) != portList.end();
    UBSE_LOG_DEBUG << "[MEM_CONTROLLER] EnablePortList portId=" << portId << ", found=" << found;
    return found;
}

template <typename ImportObjType, typename HandleInfo>
static void FillHandleInfoFromImportObj(const std::string &name,
    const std::shared_ptr<const ImportObjType> &importObjPtr, HandleInfo &info)
{
    info.name = name;
    info.udsInfo = importObjPtr->req.udsInfo;
    if constexpr (ImportObjTraits<ImportObjType>::isNumaType) {
        for (const auto &result : importObjPtr->status.importResults) {
            info.numaIds.insert(result.numaId);
        }
        UBSE_LOG_DEBUG << "[MEM_CONTROLLER] FillHandleInfoFromImportObj numa type, name=" << name
                       << ", numaIds size=" << info.numaIds.size();
    } else {
        for (const auto &result : importObjPtr->status.importResults) {
            info.memIds.insert(result.memId);
        }
        UBSE_LOG_DEBUG << "[MEM_CONTROLLER] FillHandleInfoFromImportObj non-numa type, name=" << name
                       << ", memIds size=" << info.memIds.size();
    }
}

template <typename ImportObjType>
static bool HasFaultPort(const std::shared_ptr<const ImportObjType> &importObjPtr,
    uint32_t targetChipId, const std::set<std::string> &portList)
{
    for (const auto &numaInfo : importObjPtr->algoResult.importNumaInfos) {
        if (numaInfo.chipId != targetChipId) {
            continue;
        }
        if (std::string portId = std::to_string(numaInfo.portId); !IsPortInEnableList(portId, portList)) {
            UBSE_LOG_INFO << "[MEM_CONTROLLER] import obj hit fault port, chipId=" << targetChipId
                          << ", portId=" << portId;
            return true;
        }
    }
    return false;
}

template <typename ImportObjType, typename HandleInfoVec>
static void CollectPortFaultHandleInfo(const std::string &nodeId, const std::string &chipId,
                                       const std::set<std::string> &portList, HandleInfoVec &importHandInfo,
                                       const std::string &typeStr)
{
    uint32_t targetChipId = 0;
    try {
        targetChipId = static_cast<uint32_t>(std::stoul(chipId));
    } catch (const std::exception &e) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] Invalid chipId format: " << chipId;
        return;
    }

    auto &ledger = UbseMemDebtLedger::GetInstance();
    auto &importMap = ledger.GetDebtMap<ImportObjType>();
    auto nodeImportMap = importMap.FindNodeMap(nodeId);
    if (!nodeImportMap) {
        UBSE_LOG_DEBUG << "[MEM_CONTROLLER] Node " << nodeId << " has no " << typeStr << " import debt info";
        return;
    }
    for (const auto &[name, importObjPtr] : nodeImportMap->GetAll()) {
        if (!importObjPtr) {
            UBSE_LOG_DEBUG << "[MEM_CONTROLLER] " << typeStr << " import obj is null, name=" << name;
            continue;
        }
        if (importObjPtr->status.state != UBSE_MEM_IMPORT_SUCCESS &&
            importObjPtr->status.state != UBSE_MEM_IMPORT_RUNNING) {
            UBSE_LOG_DEBUG << "[MEM_CONTROLLER] " << typeStr << " import obj state not SUCCESS/RUNNING, name=" << name
                           << ", state=" << static_cast<uint32_t>(importObjPtr->status.state);
            continue;
        }
        if (HasFaultPort<ImportObjType>(importObjPtr, targetChipId, portList)) {
            typename HandleInfoVec::value_type info;
            FillHandleInfoFromImportObj(name, importObjPtr, info);
            importHandInfo.push_back(std::move(info));
        }
    }
}

UbseResult UbseQuerySharePortFaultHandleInfo(const std::string &nodeId, const std::string &chipId,
                                             const std::set<std::string> &portList, ShareHandleInfoVec &importHandInfo)
{
    importHandInfo.clear();
    if (nodeId.empty() || chipId.empty()) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] nodeId or chipId is empty";
        return UBSE_ERROR_INVAL;
    }
    CollectPortFaultHandleInfo<UbseMemShareBorrowImportObj>(nodeId, chipId, portList, importHandInfo, "share");
    UBSE_LOG_INFO << "[MEM_CONTROLLER] UbseQuerySharePortFaultHandleInfo nodeId=" << nodeId << ", chipId=" << chipId
                  << ", portList size=" << portList.size() << ", found " << importHandInfo.size() << " fault handles";
    return UBSE_OK;
}

UbseResult UbseQueryFdPortFaultHandleInfo(const std::string &nodeId, const std::string &chipId,
                                          const std::set<std::string> &portList, FdHandleInfoVec &importHandInfo)
{
    importHandInfo.clear();
    if (nodeId.empty() || chipId.empty()) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] nodeId or chipId is empty";
        return UBSE_ERROR_INVAL;
    }
    CollectPortFaultHandleInfo<UbseMemFdBorrowImportObj>(nodeId, chipId, portList, importHandInfo, "fd");
    UBSE_LOG_INFO << "[MEM_CONTROLLER] UbseQueryFdPortFaultHandleInfo nodeId=" << nodeId << ", chipId=" << chipId
                  << ", portList size=" << portList.size() << ", found " << importHandInfo.size() << " fault handles";
    return UBSE_OK;
}

UbseResult UbseQueryNumaPortFaultHandleInfo(const std::string &nodeId, const std::string &chipId,
                                            const std::set<std::string> &portList, NumaHandleInfoVec &importHandInfo)
{
    importHandInfo.clear();
    if (nodeId.empty() || chipId.empty()) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] nodeId or chipId is empty";
        return UBSE_ERROR_INVAL;
    }
    CollectPortFaultHandleInfo<UbseMemNumaBorrowImportObj>(nodeId, chipId, portList, importHandInfo, "numa");
    UBSE_LOG_INFO << "[MEM_CONTROLLER] UbseQueryNumaPortFaultHandleInfo nodeId=" << nodeId << ", chipId=" << chipId
                  << ", portList size=" << portList.size() << ", found " << importHandInfo.size() << " fault handles";
    return UBSE_OK;
}

template <typename ImportObjType>
uint32_t GetExportErrorCode(ImportObjType &importObj, const def::UbseMemIdQueryRequest &request)
{
    auto nodeInfo = nodeController::UbseNodeController::GetInstance().GetNodeById(
        importObj.algoResult.exportNumaInfos[0].nodeId);
    if (nodeInfo.nodeId.empty()) {
        UBSE_LOG_ERROR << "Failed to find exportObj by name=" << request.name
            << ", importNodeId=" << request.importNodeId << ", importMemId=" << request.importMemId
            << ", exportNodeId=" << importObj.algoResult.exportNumaInfos[0].nodeId << " is not in cluster";
        return UBSE_ENGINE_ERR_EXPORT_LEDGERING;
    }
    if (nodeInfo.clusterState == nodeController::UbseNodeClusterState::UBSE_NODE_INIT ||
        nodeInfo.clusterState == nodeController::UbseNodeClusterState::UBSE_NODE_SMOOTHING ||
        nodeInfo.clusterState == nodeController::UbseNodeClusterState::UBSE_NODE_UNKNOWN) {
            UBSE_LOG_ERROR << "Failed to find exportObj by name=" << request.name
                << ", importNodeId=" << request.importNodeId << ", importMemId=" << request.importMemId
                << ", nodeId=" << importObj.algoResult.exportNumaInfos[0].nodeId << " state="
                << static_cast<uint32_t>(nodeInfo.clusterState);
            return UBSE_ENGINE_ERR_EXPORT_LEDGERING;
    }
    UBSE_LOG_ERROR << "Failed to find exportObj by name=" << request.name
                    << ", importNodeId=" << request.importNodeId << ", importMemId=" << request.importMemId
                    << ", exportNodeId=" << importObj.algoResult.exportNumaInfos[0].nodeId
                    << " state=" << static_cast<uint32_t>(nodeInfo.clusterState);
    return UBSE_ERR_NOT_EXIST;
}

template <typename ImportObjType, typename ExportObjType>
uint32_t ValidateObjs(const std::shared_ptr<const ImportObjType> &importObjPtr,
                      const std::shared_ptr<const ExportObjType> &exportObjPtr,
                      const def::UbseMemIdQueryRequest &request)
{
    if (!importObjPtr) {
        UBSE_LOG_ERROR << "Failed to find importObj by name=" << request.name
                       << ", importNodeId=" << request.importNodeId
                       << ", importMemId=" << request.importMemId;
        return UBSE_ERR_NOT_EXIST;
    }
    const auto &importObj = *importObjPtr;
    if (importObj.exportObmmInfo.empty() || importObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "Failed to find export mem id by importNodeId=" << request.importNodeId
                       << ", importMemId=" << request.importMemId << ", name=" << request.name
                       << ", exportNumaInfosSize=" << importObj.algoResult.exportNumaInfos.size()
                       << ", exportObmmInfoSize=" << importObj.exportObmmInfo.size();
        return UBSE_ERR_NOT_EXIST;
    }
    if (!importObj.req.udsInfo.CheckPermission(request.udsInfo)) {
        UBSE_LOG_ERROR << "Permission denied. src_username=" << request.udsInfo.username
                       << ", src_uid=" << request.udsInfo.uid
                       << ", dst_username=" << importObj.req.udsInfo.username
                       << ", dst_uid=" << importObj.req.udsInfo.uid << ", importNodeId=" << request.importNodeId
                       << ", importMemId=" << request.importMemId << ", name=" << request.name;
        return UBSE_ERR_AUTH_FAILED;
    }
    if (!exportObjPtr) {
        return GetExportErrorCode(importObj, request);
    }
    UbseMemStage memStage = GetMemStageByImportObjState(importObjPtr);
    if (memStage != UbseMemStage::UBSE_CREATING && memStage != UbseMemStage::UBSE_DELETING) {
        memStage = GetMemStageByExportObjState(exportObjPtr);
    }
    if (memStage == UbseMemStage::UBSE_CREATING || memStage == UbseMemStage::UBSE_DELETING) {
        UBSE_LOG_ERROR << "resource is being borrowed or returned, name is "
                       << request.name << ", memStage=" << static_cast<uint32_t>(memStage)
                       << ", importNodeId=" << request.importNodeId << ", importMemId=" << request.importMemId;
        auto ret = (memStage == UbseMemStage::UBSE_CREATING) ? UBSE_ERR_CREATING : UBSE_ERR_DELETING;
        return ret;
    }
    return UBSE_OK;
}

template <typename ImportObjType, typename ExportObjType>
uint32_t ProcessImportObjByPtr(const std::pair<std::shared_ptr<const ImportObjType>,
                               std::shared_ptr<const ExportObjType>> &objPair,
                               const def::UbseMemIdQueryRequest &request,
                               def::UbseExportMemDesc &memDesc)
{
    auto [importObjPtr, exportObjPtr] = objPair;
    if (auto ret = ValidateObjs(importObjPtr, exportObjPtr, request); ret != UBSE_OK) {
        return ret;
    }
    const auto &importObj = *importObjPtr;
    bool isFind = false;
    int64_t index = 0;
    for (const auto &obmmInfo : importObj.status.importResults) {
        if (request.importMemId == obmmInfo.memId) {
            isFind = true;
            break;
        }
        index++;
    }
    if (!isFind || index >= importObj.exportObmmInfo.size()) {
        UBSE_LOG_ERROR << "Failed to find export mem id by importNodeId=" << request.importNodeId
                       << ", importMemId=" << request.importMemId << ", name="
                       << request.name<< ", exportNumaInfosSize="
                       << importObj.algoResult.exportNumaInfos.size() << ", index=" << index;
        return UBSE_ERR_NOT_EXIST;
    }
    if (ConvertStrToUint32(importObj.algoResult.exportNumaInfos[0].nodeId, memDesc.exportSlotId) != UBSE_OK) {
        UBSE_LOG_ERROR << "ConvertStrToUint32 failed, str=" << importObj.algoResult.exportNumaInfos[0].nodeId;
        return UBSE_ERROR;
    }
    memDesc.exportMemId = importObj.exportObmmInfo[index].memId; // importMemId的索引值和exportMemId的索引值一一对应
    return UBSE_OK;
}

uint32_t UbseMemGetMemIdByImport(const def::UbseMemIdQueryRequest &request, def::UbseExportMemDesc &memDesc)
{
    switch (static_cast<UbseMemBorrowType>(request.borrowType)) {
        case UbseMemBorrowType::FD_BORROW:
            return ProcessImportObjByPtr(
                FindBorrowObjPair<UbseMemFdBorrowImportObj, UbseMemFdBorrowExportObj>(
                    request.name, request.importNodeId),
                request, memDesc);
        case UbseMemBorrowType::NUMA_BORROW:
            return ProcessImportObjByPtr(
                FindBorrowObjPair<UbseMemNumaBorrowImportObj, UbseMemNumaBorrowExportObj>(
                    request.name, request.importNodeId),
                request, memDesc);
        case UbseMemBorrowType::SHM_ATTACH:
        case UbseMemBorrowType::SHM_BORROW: {
            auto importObj = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(
                request.importNodeId, request.name);
            auto exportObj = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>()
            .GetExportResourceByResId(request.name);
            return ProcessImportObjByPtr<UbseMemShareBorrowImportObj, UbseMemShareBorrowExportObj>(
                {importObj, exportObj}, request, memDesc);
        }
        default:
            UBSE_LOG_ERROR << "Unsupported borrow type=" << static_cast<uint32_t>(request.borrowType)
                           << ",name=" << request.name << ",importNodeId=" << request.importNodeId
                           << ",importMemId=" << request.importMemId;
            return UBSE_ERROR_INVAL;
    }
    return UBSE_ERROR;
}
} // namespace ubse::mem::controller::debt