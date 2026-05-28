/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#include "ubse_mem_controller_ledger.h"

#include <unordered_map>

#include <ubse_node.h>
#include <cstdint>
#include <string>
#include <thread>
#include "adapter_plugins/mti/ubse_topology_interface.h"

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_debt_info.h"

#include "api/ubse_mem_controller_share_api.h"
#include "logging_lock_guard.h"
#include "ubse_context.h"
#include "ubse_election_module.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_controller_ledger_filter.h"
#include "ubse_mem_controller_msg.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_mem_def.h"
#include "ubse_mem_global_ledger_report.h"
#include "ubse_mem_update_obj_state.simpo.h"
#include "ubse_mem_util.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_util.h"
#include "ubse_security_module.h"

namespace ubse::mem::controller {
using namespace ubse::mem::util;
using namespace ubse::mem::controller;
using namespace ubse::nodeController;
using ubse::election::UbseElectionModule;
using namespace ubse::mem::controller::debt;
using namespace ubse::security;

using namespace ubse::context;
UBSE_DEFINE_THIS_MODULE("ubse");
const std::string ClusterHandlerKey = "NODE_CLUSTER_HDL";
const int DEFAULT_SLEEP_SECONDS = 299;
const uint32_t LEDGE_RETRY_DURATION = 2;
const uint32_t MAX_RETRIES = 20;
const uint32_t BASE_SLEEP_TIME = 500;
const uint32_t LEDGER_RUNNING_WAIT_INTERVAL = 1;
const std::string CRITICAL_ERR_PATH_PREFIX = "/sys/devices/system/node/node";
const std::string CRITICAL_ERR_FILE = "/critical_err";

enum class RemoteNumaStatus {
    AVAILABLE = 0,
    UNAVAILABLE = 1,
    UNKNOWN = 2,
};

std::mutex mtx_target_ledger_global;

bool CheckNodeIsMaster()
{
    auto electionModule = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get electionModule.";
        return false;
    }
    if (!electionModule->IsLeader()) {
        // 非主节点、终止对账操作
        return false;
    }
    return true;
}

uint32_t GetExponentialBackOffSleepTime(uint32_t count)
{
    if (count < MAX_RETRIES) {
        return BASE_SLEEP_TIME * (1 + count);
    }
    return BASE_SLEEP_TIME * MAX_RETRIES;
}

void GetLedgerByNodeId(std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap,
                       const NodeMemDebtInfo &masterDebtInfo, const std::string &nodeId,
                       const std::string &targetNodeId)
{
    NodeMemDebtInfo agentDebtInfo{};
    std::string currentNodeId = GetCurNodeId();
    if (currentNodeId == nodeId) {
        allDebtInfoMap[nodeId] = masterDebtInfo;
    } else {
        // 账本目标节点采集失败时，重试，直到数据采集成功为止；若下一次对账周期此次还未采集成功，下一次smoothing直接退出，继续等待本次smoothing
        uint32_t count = 0;
        while (!g_globalStop.load() && count < MAX_RETRIES) {
            ubse::nodeController::UbseNodeInfo ubseNodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
            if (ubseNodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_PRE_BMC ||
                ubseNodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT ||
                ubseNodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_UNKNOWN) {
                UBSE_LOG_INFO << "node=" << nodeId << "skip ledger, state=" << int(ubseNodeInfo.clusterState);
                break;
            }
            if (!CheckNodeIsMaster()) {
                UBSE_LOG_INFO << "current node not master, skip collect ledger.";
                break;
            }
            auto ret = CollectLedge(nodeId, agentDebtInfo);
            if (ret == UBSE_OK) {
                allDebtInfoMap[nodeId] = agentDebtInfo;
                break;
            }
            UBSE_LOG_WARN << "nodeId=" << nodeId << " collect ledge failed, will retry, "
                          << FormatRetCode(ret);
            std::this_thread::sleep_for(std::chrono::milliseconds(NO_500));
            count++;
        }
    }
}

UbseResult GetTargetLedgerByNodeId(std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap,
                                   const NodeMemDebtInfo &masterDebtInfo, const std::string &targetNodeId)
{
    NodeMemDebtInfo agentDebtInfo{};
    std::string currentNodeId = GetCurNodeId();
    if (currentNodeId == targetNodeId) {
        allDebtInfoMap[targetNodeId] = masterDebtInfo;
        return UBSE_OK;
    } else {
        // 账本目标节点采集失败时，直接返回失败
        if (!CheckNodeIsMaster()) {
            UBSE_LOG_INFO << "current node not master, skip collect ledger.";
            return UBSE_OK;
        }
        auto ret = CollectLedge(targetNodeId, agentDebtInfo);
        if (ret == UBSE_OK) {
            allDebtInfoMap[targetNodeId] = agentDebtInfo;
        }
        return ret;
    }
}

UbseResult CollectAllLedger(std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap,
                            const NodeMemDebtInfo &masterDebtInfo, const std::string &targetNodeId)
{
    std::vector<UbseRoleInfo> roleInfos;
    auto ret = UbseNodeGetLinkUpNodes(roleInfos);
    for (const auto &nodeInfo : roleInfos) {
        ubse::nodeController::UbseNodeInfo ubseNodeInfo =
            UbseNodeController::GetInstance().GetNodeById(nodeInfo.nodeId);
        if (ubseNodeInfo.nodeId.empty()) {
            UBSE_LOG_WARN << "ubseNodeInfo nodeId is empty, nodeId =" << nodeInfo.nodeId;
            continue;
        }
        if (ubseNodeInfo.nodeId != targetNodeId && ubseNodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_FAULT &&
            ubseNodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_PRE_BMC &&
            ubseNodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_UNKNOWN) {
            GetLedgerByNodeId(allDebtInfoMap, masterDebtInfo, nodeInfo.nodeId, targetNodeId);
        } else {
            UBSE_LOG_INFO << "skip ledger for nodeId=" << nodeInfo.nodeId
                          << ", state = " << int(ubseNodeInfo.clusterState) << "target =" << targetNodeId;
        }
        if (g_globalStop.load()) {
            UBSE_LOG_WARN << "process exit, stop ledger";
            return UBSE_OK;
        }
    }
    ret = GetTargetLedgerByNodeId(allDebtInfoMap, masterDebtInfo, targetNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "nodeId=" << targetNodeId << " collect ledge failed, " << FormatRetCode(ret);
    }
    return ret;
}

NodeMemDebtInfo GetMasterCtxLedger(const std::string &nodeId)
{
    const auto map = GetNodeMemDebtInfoMap();
    if (const auto it = map.find(nodeId); it != map.end()) {
        return it->second;
    }
    return {};
}

template <typename ImportObj, typename ExportObjMap>
UbseResult MasterHandleSingleDebtHelper(const ImportObj &importObj, const ExportObjMap &exportObjMap,
                                        const std::string &name, const std::string &nodeId,
                                        UbseMemBorrowType borrowType)
{
    auto ret = UBSE_OK;
    std::string exportDebtName;
    // 使用 std::decay_t 去除引用和 const 属性，确保匹配核心类型
    if constexpr (std::is_same_v<std::decay_t<decltype(importObj)>, UbseMemShareBorrowImportObj>) {
        exportDebtName = name;
    } else {
        exportDebtName = name + "_" + nodeId;
    }

    if (exportObjMap.find(exportDebtName) != exportObjMap.end()) {
        return ret;
    }

    bool isValid = true;
    for (const auto &decoderval : importObj.status.decoderResult) {
        if (!decoderval.valid) {
            isValid = false;
            break;
        }
    }

    if (!isValid) {
        int retry = 10;
        UBSE_LOG_INFO << "Start to invalidate import debt, name=" << name
                      << ", importNodeId=" << nodeId
                      << ", borrowType=" << int(borrowType);
        while (retry > 0) {
            retry--;
            ret = SendInvalidateSingleImportDebtRpc(nodeId, name, borrowType);
            if (ret == UBSE_OK) {
                break;
            }
        }
    }
    return ret;
}

UbseResult MasterHandleSingleImportDebt(const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap,
                                        const std::string &targetNodeId)
{
    auto ret = UBSE_OK;

    auto processDebt = [&allDebtInfoMap, &ret, &targetNodeId](const auto& importMap,
                                                             const auto& getExportMap,
                                                             const auto& getImportNode,
                                                             UbseMemBorrowType borrowType) {
        for (const auto &[name, importObj] : importMap) {
            if (importObj.algoResult.exportNumaInfos.empty()) {
                UBSE_LOG_ERROR << "exportNumaInfos is empty, skip debt: " << name;
                continue;
            }

            std::string importNode = getImportNode(importObj);
            std::string exportNode = importObj.algoResult.exportNumaInfos[0].nodeId;
            if (allDebtInfoMap.find(exportNode) == allDebtInfoMap.end()) {
                continue;
            }
            if (importNode != targetNodeId && exportNode != targetNodeId) {
                continue;
            }

            ret |= MasterHandleSingleDebtHelper(importObj, getExportMap(allDebtInfoMap.at(exportNode)),
                                                name, importNode, borrowType);
        }
    };

    for (const auto &[_, debtInfo] : allDebtInfoMap) {
        processDebt(debtInfo.fdImportObjMap,
                    [](const auto& exportDebtInfo) { return exportDebtInfo.fdExportObjMap; },
                    [](const auto& importObj) { return importObj.req.importNodeId; },
                    UbseMemBorrowType::FD_BORROW);

        processDebt(debtInfo.numaImportObjMap,
                    [](const auto& exportDebtInfo) { return exportDebtInfo.numaExportObjMap; },
                    [](const auto& importObj) { return importObj.req.importNodeId; },
                    UbseMemBorrowType::NUMA_BORROW);
        processDebt(debtInfo.addrImportObjMap,
                    [](const auto& exportDebtInfo) { return exportDebtInfo.addrExportObjMap; },
                    [](const auto& importObj) { return importObj.req.importNodeId; },
                    UbseMemBorrowType::ADDR_BORROW);

        processDebt(debtInfo.shareImportObjMap,
                    [](const auto& exportDebtInfo) { return exportDebtInfo.shareExportObjMap; },
                    [](const auto& importObj) { return importObj.importNodeId; },
                    UbseMemBorrowType::SHM_BORROW);
    }

    return ret;
}

template <typename ImportObjMap>
UbseResult AgentInvalidateImportDebtHelper(ImportObjMap &importObjMap,
                                           const std::string &name)
{
    if (importObjMap.find(name) == importObjMap.end()) {
        UBSE_LOG_INFO << "ImportObjMap not found name=" << name;
        return UBSE_OK;
    }
    auto& debtObj = importObjMap.at(name);
    uint8_t decoderId = 0;
    // 使用 std::decay_t 去除引用和 const 属性，确保匹配核心类型
    if constexpr (std::is_same_v<std::decay_t<decltype(debtObj)>, UbseMemShareBorrowImportObj>) {
        decoderId = debtObj.req.ubseMemPrivData.cacheableFlag == 1 ? 0 : 1;
    }

    auto ret = AgentInvalidateDecoderEntry(debtObj.algoResult.attachSocketId, debtObj.status, decoderId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "InvalidateDecoderEntry failed, " << FormatRetCode(ret);
        return ret;
    }
    return ret;
}

UbseResult AgentInvalidateImportDebt(const std::string &name,
                                     UbseMemBorrowType type)
{
    auto agentDebtInfo = GetNodeMemDebtInfoMap();
    ubse::nodeController::UbseNodeInfo curNode = UbseNodeController::GetInstance().GetCurNode();
    auto &debtInfo = agentDebtInfo.at(curNode.nodeId);
    uint32_t ret = UBSE_ERROR;
    switch (type) {
        case UbseMemBorrowType::FD_BORROW:
            ret = AgentInvalidateImportDebtHelper(debtInfo.fdImportObjMap, name);
            break;
        case UbseMemBorrowType::NUMA_BORROW:
            ret = AgentInvalidateImportDebtHelper(debtInfo.numaImportObjMap, name);
            break;
        case UbseMemBorrowType::ADDR_BORROW:
            ret = AgentInvalidateImportDebtHelper(debtInfo.addrImportObjMap, name);
            break;
        case UbseMemBorrowType::SHM_BORROW:
            ret = AgentInvalidateImportDebtHelper(debtInfo.shareImportObjMap, name);
            break;
        default:
            break;
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "InvalidateImportDebt failed, " << FormatRetCode(ret);
        return ret;
    }
    UbseMemDebtLedger::GetInstance().LoadFromNodeMemDebtInfo(curNode.nodeId, debtInfo);
    return ret;
}

UbseResult LedgerHandler(const ubse::nodeController::UbseNodeInfo &node)
{
    if (node.clusterState != UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "nodeId=" << node.nodeId << "start ledger.";
    UbseResult ret = UBSE_OK;
    auto nodeId = node.nodeId;
    if (!CheckNodeIsMaster()) {
        UBSE_LOG_INFO << "current node not master, skip ledger.";
        return UBSE_OK;
    }
    auto masterDebtInfo = GetMasterCtxLedger(nodeId);
    // 获取全量账本
    std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    ret |= CollectAllLedger(allDebtInfoMap, masterDebtInfo, nodeId);
    if (!CheckNodeIsMaster()) {
        UBSE_LOG_INFO << "current node not master, skip ledger.";
        return UBSE_OK;
    }

    NodeMemDebtInfo agentDebtInfo{};
    std::string currentNodeId = GetCurNodeId();
    agentDebtInfo = allDebtInfoMap[node.nodeId];
    ret |= FdBorrowLedger(node.nodeId, masterDebtInfo, agentDebtInfo, allDebtInfoMap);
    ret |= NumaBorrowLedger(node.nodeId, masterDebtInfo, agentDebtInfo, allDebtInfoMap);
    ret |= AddrBorrowLedger(node.nodeId, masterDebtInfo, agentDebtInfo, allDebtInfoMap);
    ret |= ShmBorrowLedger(node.nodeId, masterDebtInfo, agentDebtInfo);
    allDebtInfoMap[currentNodeId] = GetMasterCtxLedger(currentNodeId);
    ret |= MasterHandleSingleImportDebt(allDebtInfoMap, nodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << node.nodeId << " ledger failed, " << FormatRetCode(ret);
    } else {
        UBSE_LOG_INFO << "nodeId=" << node.nodeId << " ledger success.";
        // Create async thread to execute MasterNotifySmapNumaStatus
        auto resourceExecutor = GetExecutor("ubseMemController");
        if (resourceExecutor == nullptr) {
            UBSE_LOG_ERROR << "Failed to get ubseMemController";
            return UBSE_ERROR;
        }
        // 使用 [map = std::move(obj)] 语法将所有权移入 Lambda
        resourceExecutor->Execute([localMap = std::move(allDebtInfoMap), nodeId]()  {
            UBSE_LOG_INFO << "Start async thread to notify smap numa status for nodeId=" << nodeId;
            MasterNotifyRemoteNumaStatus(nodeId, localMap);
        });
        auto summaryRet = SubmitNodeLedgerSummary(nodeId);
        if (summaryRet != UBSE_OK) {
            UBSE_LOG_ERROR << "submit global ledger summary failed, nodeId=" << node.nodeId << ", "
                           << FormatRetCode(summaryRet);
            ret |= summaryRet;
        }
    }
    return ret;
}

UbseResult FdBorrowLedger(const std::string &nodeId, const NodeMemDebtInfo &masterDebtInfo,
                          const NodeMemDebtInfo &agentDebtInfo,
                          const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap)
{
    std::vector<UbseMemFdBorrowExportObj> masterRunningExportObjs{};
    std::vector<UbseMemFdBorrowExportObj> masterNotRunningExportObjs{};
    std::vector<UbseMemFdBorrowExportObj> agentNotRunningExportObjs{};
    FilterRunningFdExport(TransFdExportList(masterDebtInfo.fdExportObjMap),
                          TransFdExportList(agentDebtInfo.fdExportObjMap), masterRunningExportObjs,
                          masterNotRunningExportObjs, agentNotRunningExportObjs);
    std::vector<UbseMemFdBorrowExportObj> masterDiffFdExports{};
    std::vector<UbseMemFdBorrowExportObj> agentDiffFdExports{};
    FilterFdDifferentExportSet(masterNotRunningExportObjs, agentNotRunningExportObjs, masterDiffFdExports,
                               agentDiffFdExports);

    auto bothExistsExportObj = FilterBothExistsFdExport(masterNotRunningExportObjs, agentNotRunningExportObjs);

    std::vector<UbseMemFdBorrowImportObj> masterRunningImportObjs{};
    std::vector<UbseMemFdBorrowImportObj> masterNotRunningImportObjs{};
    std::vector<UbseMemFdBorrowImportObj> agentNotRunningImportObjs{};
    FilterRunningFdImport(TransFdImportList(masterDebtInfo.fdImportObjMap),
                          TransFdImportList(agentDebtInfo.fdImportObjMap), masterRunningImportObjs,
                          masterNotRunningImportObjs, agentNotRunningImportObjs);

    std::vector<UbseMemFdBorrowImportObj> masterDiffFdImports{};
    std::vector<UbseMemFdBorrowImportObj> agentDiffFdImports{};
    FilterFdDifferentImportSet(masterNotRunningImportObjs, agentNotRunningImportObjs, masterDiffFdImports,
                               agentDiffFdImports);

    auto ret = MasterDiffFdExportHandler(nodeId, masterDiffFdExports);
    ret |= MasterDiffFdImportHandler(nodeId, masterDiffFdImports);
    ret |= BothFdExportHandler(nodeId, bothExistsExportObj, allDebtInfoMap);
    ret |= AgentDiffFdExportHandler(nodeId, agentDiffFdExports, allDebtInfoMap);
    ret |= AgentDiffFdImportHandler(nodeId, agentDiffFdImports);
    // 若 master存在运行态账本，再次去从节点获取一次账本，以从节点账本状态刷新master账本
    if (!masterRunningExportObjs.empty() || !masterRunningImportObjs.empty()) {
        ret |= MasterRunningFdExportHandler(nodeId, masterRunningExportObjs);
        ret |= MasterRunningFdImportHandler(nodeId, masterRunningImportObjs);
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << nodeId << " ledger fd borrow failed, " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult NumaBorrowLedger(const std::string &nodeId, const NodeMemDebtInfo &masterDebtInfo,
                            const NodeMemDebtInfo &agentDebtInfo,
                            const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap)
{
    std::vector<UbseMemNumaBorrowExportObj> masterRunningExportObjs{};
    std::vector<UbseMemNumaBorrowExportObj> masterNotRunningExportObjs{};
    std::vector<UbseMemNumaBorrowExportObj> agentNotRunningExportObjs{};
    FilterRunningNumaExport(TransNumaExportList(masterDebtInfo.numaExportObjMap),
                            TransNumaExportList(agentDebtInfo.numaExportObjMap), masterRunningExportObjs,
                            masterNotRunningExportObjs, agentNotRunningExportObjs);
    std::vector<UbseMemNumaBorrowExportObj> masterDiffNumaExports{};
    std::vector<UbseMemNumaBorrowExportObj> agentDiffNumaExports{};
    FilterNumaDifferentExportSet(masterNotRunningExportObjs, agentNotRunningExportObjs, masterDiffNumaExports,
                                 agentDiffNumaExports);

    auto bothExistsExportObj = FilterBothExistsNumaExport(masterNotRunningExportObjs, agentNotRunningExportObjs);

    std::vector<UbseMemNumaBorrowImportObj> masterRunningImportObjs{};
    std::vector<UbseMemNumaBorrowImportObj> masterNotRunningImportObjs{};
    std::vector<UbseMemNumaBorrowImportObj> agentNotRunningImportObjs{};
    FilterRunningNumaImport(TransNumaImportList(masterDebtInfo.numaImportObjMap),
                            TransNumaImportList(agentDebtInfo.numaImportObjMap), masterRunningImportObjs,
                            masterNotRunningImportObjs, agentNotRunningImportObjs);

    std::vector<UbseMemNumaBorrowImportObj> masterDiffNumaImports{};
    std::vector<UbseMemNumaBorrowImportObj> agentDiffNumaImports{};
    FilterNumaDifferentImportSet(masterNotRunningImportObjs, agentNotRunningImportObjs, masterDiffNumaImports,
                                 agentDiffNumaImports);

    auto ret = MasterDiffNumaExportHandler(nodeId, masterDiffNumaExports);
    ret |= MasterDiffNumaImportHandler(nodeId, masterDiffNumaImports);
    ret |= BothNumaExportHandler(nodeId, bothExistsExportObj, allDebtInfoMap);
    ret |= AgentDiffNumaExportHandler(nodeId, agentDiffNumaExports, allDebtInfoMap);
    ret |= AgentDiffNumaImportHandler(nodeId, agentDiffNumaImports);
    if (!masterRunningExportObjs.empty() || !masterRunningImportObjs.empty()) {
        ret |= MasterRunningNumaExportHandler(nodeId, masterRunningExportObjs);
        ret |= MasterRunningNumaImportHandler(nodeId, masterRunningImportObjs);
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << nodeId << " ledger numa borrow failed, " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult AddrBorrowLedger(const std::string &nodeId, const NodeMemDebtInfo &masterDebtInfo,
                            const NodeMemDebtInfo &agentDebtInfo,
                            const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap)
{
    std::vector<UbseMemAddrBorrowExportObj> masterRunningExportObjs{};
    std::vector<UbseMemAddrBorrowExportObj> masterNotRunningExportObjs{};
    std::vector<UbseMemAddrBorrowExportObj> agentNotRunningExportObjs{};
    FilterRunningAddrExport(TransAddrExportList(masterDebtInfo.addrExportObjMap),
                            TransAddrExportList(agentDebtInfo.addrExportObjMap), masterRunningExportObjs,
                            masterNotRunningExportObjs, agentNotRunningExportObjs);
    std::vector<UbseMemAddrBorrowExportObj> masterDiffAddrExports{};
    std::vector<UbseMemAddrBorrowExportObj> agentDiffAddrExports{};
    FilterAddrDifferentExportSet(masterNotRunningExportObjs, agentNotRunningExportObjs, masterDiffAddrExports,
                                 agentDiffAddrExports);

    auto bothExistsExportObj = FilterBothExistsAddrExport(masterNotRunningExportObjs, agentNotRunningExportObjs);

    std::vector<UbseMemAddrBorrowImportObj> masterRunningImportObjs{};
    std::vector<UbseMemAddrBorrowImportObj> masterNotRunningImportObjs{};
    std::vector<UbseMemAddrBorrowImportObj> agentNotRunningImportObjs{};
    FilterRunningAddrImport(TransAddrImportList(masterDebtInfo.addrImportObjMap),
                            TransAddrImportList(agentDebtInfo.addrImportObjMap), masterRunningImportObjs,
                            masterNotRunningImportObjs, agentNotRunningImportObjs);

    std::vector<UbseMemAddrBorrowImportObj> masterDiffAddrImports{};
    std::vector<UbseMemAddrBorrowImportObj> agentDiffAddrImports{};
    FilterAddrDifferentImportSet(masterNotRunningImportObjs, agentNotRunningImportObjs, masterDiffAddrImports,
                                 agentDiffAddrImports);

    auto ret = MasterDiffAddrExportHandler(nodeId, masterDiffAddrExports);
    ret |= MasterDiffAddrImportHandler(nodeId, masterDiffAddrImports);
    ret |= BothAddrExportHandler(nodeId, bothExistsExportObj, allDebtInfoMap);
    ret |= AgentDiffAddrExportHandler(nodeId, agentDiffAddrExports, allDebtInfoMap);
    ret |= AgentDiffAddrImportHandler(nodeId, agentDiffAddrImports);
    if (!masterRunningExportObjs.empty() || !masterRunningImportObjs.empty()) {
        ret |= MasterRunningAddrExportHandler(nodeId, masterRunningExportObjs);
        ret |= MasterRunningAddrImportHandler(nodeId, masterRunningImportObjs);
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << nodeId << " ledger addr borrow failed, " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult ShmBorrowLedger(const std::string &nodeId, const NodeMemDebtInfo &masterDebtInfo,
                           const NodeMemDebtInfo &agentDebtInfo)
{
    std::vector<UbseMemShareBorrowExportObj> masterRunningExportObjs{};
    std::vector<UbseMemShareBorrowExportObj> masterNotRunningExportObjs{};
    std::vector<UbseMemShareBorrowExportObj> agentNotRunningExportObjs{};
    FilterRunningShareExport(TransShareExportList(masterDebtInfo.shareExportObjMap),
                             TransShareExportList(agentDebtInfo.shareExportObjMap), masterRunningExportObjs,
                             masterNotRunningExportObjs, agentNotRunningExportObjs);
    std::vector<UbseMemShareBorrowExportObj> masterDiffShmExports{};
    std::vector<UbseMemShareBorrowExportObj> agentDiffShmExports{};
    FilterShareDifferentExportSet(masterNotRunningExportObjs, agentNotRunningExportObjs, masterDiffShmExports,
                                  agentDiffShmExports);

    std::vector<UbseMemShareBorrowImportObj> masterRunningImportObjs{};
    std::vector<UbseMemShareBorrowImportObj> masterNotRunningImportObjs{};
    std::vector<UbseMemShareBorrowImportObj> agentNotRunningImportObjs{};
    FilterRunningShareImport(TransShareImportList(masterDebtInfo.shareImportObjMap),
                             TransShareImportList(agentDebtInfo.shareImportObjMap), masterRunningImportObjs,
                             masterNotRunningImportObjs, agentNotRunningImportObjs);

    std::vector<UbseMemShareBorrowImportObj> masterDiffShmImports{};
    std::vector<UbseMemShareBorrowImportObj> agentDiffShmImports{};
    FilterShareDifferentImportSet(masterNotRunningImportObjs, agentNotRunningImportObjs, masterDiffShmImports,
                                  agentDiffShmImports);

    auto ret = MasterDiffShareExportHandler(nodeId, masterDiffShmExports);
    ret |= MasterDiffShareImportHandler(nodeId, masterDiffShmImports);
    ret |= AgentDiffShareExportHandler(nodeId, agentDiffShmExports);
    ret |= AgentDiffShareImportHandler(nodeId, agentDiffShmImports);
    if (!masterRunningExportObjs.empty() || !masterRunningImportObjs.empty()) {
        ret |= MasterRunningShareExportHandler(nodeId, masterRunningExportObjs);
        ret |= MasterRunningShareImportHandler(nodeId, masterRunningImportObjs);
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << nodeId << " ledger share borrow failed, " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult MasterDiffFdExportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowExportObj> objs)
{
    UbseResult ret = UBSE_OK;
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff fd export name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff fd export name=" << obj.req.name << ", start to destroy";
        obj.status.state = UbseMemState::UBSE_MEM_EXPORT_DESTROYED;
        AddFdExport(obj);
    }
    return UBSE_OK;
}

UbseResult MasterDiffFdImportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowImportObj> objs)
{
    // 若master存在 单导出，agent不存在单导出
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff fd import name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED ||
            obj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff fd import name=" << obj.req.name << ", start to destroy";
        obj.status.state = UbseMemState::UBSE_MEM_IMPORT_DESTROYED;
        AddFdImport(obj);
    }
    return UBSE_OK;
}

UbseResult MasterDiffNumaExportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowExportObj> objs)
{
    // 若master存在 单导出，agent不存在单导出
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff numa export name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff numa export name=" << obj.req.name
                      << ", start to destroy";
        obj.status.state = UbseMemState::UBSE_MEM_EXPORT_DESTROYED;
        AddNumaExport(obj);
    }
    return UBSE_OK;
}

UbseResult MasterDiffNumaImportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowImportObj> objs)
{
    // 若master存在 单导出，agent不存在单导出
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff numa import name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED ||
            obj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff numa import name=" << obj.req.name
                      << ", start to destroy";
        obj.status.state = UbseMemState::UBSE_MEM_IMPORT_DESTROYED;
        AddNumaImport(obj);
    }
    return UBSE_OK;
}

UbseResult MasterDiffAddrExportHandler(const std::string &nodeId, std::vector<UbseMemAddrBorrowExportObj> objs)
{
    // 若master存在 单导出，agent不存在单导出
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff addr export name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff addr numa export name=" << obj.req.name
                      << ", start to destroy";
        obj.status.state = UbseMemState::UBSE_MEM_EXPORT_DESTROYED;
        AddAddrExport(obj);
    }
    return UBSE_OK;
}

UbseResult MasterDiffAddrImportHandler(const std::string &nodeId, std::vector<UbseMemAddrBorrowImportObj> objs)
{
    // 若master存在 单导出，agent不存在单导出
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff addr import name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED ||
            obj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff addr import name=" << obj.req.name
                      << ", start to destroy";
        obj.status.state = UbseMemState::UBSE_MEM_IMPORT_DESTROYED;
        AddAddrImport(obj);
    }
    return UBSE_OK;
}

UbseResult MasterDiffShareExportHandler(const std::string &nodeId, std::vector<UbseMemShareBorrowExportObj> objs)
{
    // 若master存在 单导出，agent不存在单导出
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff share export name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff share numa export name=" << obj.req.name
                      << ", start to destroy";
        obj.status.state = UbseMemState::UBSE_MEM_EXPORT_DESTROYED;
        AddShareExport(obj);
    }
    return UBSE_OK;
}

UbseResult MasterDiffShareImportHandler(const std::string &nodeId, std::vector<UbseMemShareBorrowImportObj> objs)
{
    // 若master存在 单导出，agent不存在单导出
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff share import name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED ||
            obj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " master diff share import name=" << obj.req.name
                      << ", start to destroy";
        obj.status.state = UbseMemState::UBSE_MEM_IMPORT_DESTROYED;
        AddShareImport(obj);
    }
    return UBSE_OK;
}

void HandleLocalFdImport(UbseMemFdBorrowExportObj obj, const std::string &importNodeId,
                         const NodeMemDebtInfoMap &nodeMemDebtInfoMap)
{
    UBSE_LOG_INFO << "fd export name=" << obj.req.name << ", curr node is import node=" << importNodeId;
    UbseMemFdBorrowImportObj importObj{};
    std::string resKey = obj.req.name;
    if (nodeMemDebtInfoMap.find(importNodeId) != nodeMemDebtInfoMap.end() &&
    nodeMemDebtInfoMap.at(importNodeId).fdImportObjMap.find(resKey) !=
        nodeMemDebtInfoMap.at(importNodeId).fdImportObjMap.end()) {
        importObj = nodeMemDebtInfoMap.at(importNodeId).fdImportObjMap.at(resKey);
        }
    if (importObj.req.name.empty()) {
        // 上下文中不存在导入对象
        UBSE_LOG_INFO << "fd export name=" << obj.req.name << ", curr node is import node=" << importNodeId
                      << ", context not found import resource, start to destroy.";
        DeleteFdExport(obj);
        return;
    }
    UBSE_LOG_INFO << "fd export name=" << obj.req.name << ", curr node is import node=" << importNodeId
                  << ", import status=" << importObj.status.state;
    if (importObj.status.state == UBSE_MEM_EXPORT_DESTROYED || importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        UBSE_LOG_INFO << "fd export name=" << obj.req.name << ", curr node is import node=" << importNodeId
                      << ",  import resource destroyed, start to destroy.";
        DeleteFdExport(obj);
    }
}

void HandleRemoteFdImport(UbseMemFdBorrowExportObj obj, const std::string &exportId, const std::string &importNodeId,
                          const NodeMemDebtInfoMap &nodeMemDebtInfoMap,
                          const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap)
{
    UBSE_LOG_INFO << "fd export name=" << obj.req.name << ", remote import node=" << importNodeId;
    auto node = UbseNodeController::GetInstance().GetNodeById(importNodeId);
    UBSE_LOG_INFO << "fd export name=" << obj.req.name << ", start to query import nodeId=" << importNodeId;
    if (!node.nodeId.empty() && node.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT) {
        UBSE_LOG_WARN << "nodeId=" << exportId << " both fd export=" << obj.req.name << " import node=" << importNodeId
                      << " fault, start to destroy";
        DeleteFdExport(obj);
        return;
    }
    UbseMemFdBorrowImportObj importObj{};
    auto ret = QueryFdImportObj(importNodeId, obj.req.name, importObj, allDebtInfoMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "req name=" << obj.req.name << " query import nodeId=" << importNodeId << " failed, "
                       << FormatRetCode(ret);
        // 查询失败，跳过本次账本，等待下次处理
        return;
    }
    if (!importObj.req.name.empty()) {
        UBSE_LOG_INFO << "nodeId=" << exportId << " both fd export name=" << obj.req.name
                      << " has import obj, state=" << importObj.status.state;
        if (importObj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED ||
            importObj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            UBSE_LOG_INFO << "nodeId=" << exportId << " both fd export name=" << obj.req.name
                          << " peer import obj, state=destroyed start to destroy";
            DeleteFdExport(obj);
        }
    } else {
        if (!IsMasterExitsRunningFdImportObj(nodeMemDebtInfoMap, importNodeId, obj.req.name)) {
            UBSE_LOG_INFO << "nodeId=" << exportId << " both fd export name=" << obj.req.name
                          << " context not exists running peer import obj record, start to destroy";
            DeleteFdExport(obj);
        }
    }
}

UbseResult BothFdExportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowExportObj> objs,
                               const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap)
{
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " both fd export name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        if (obj.algoResult.importNumaInfos.size() == 0) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << " both fd export=" << obj.req.name << " has no import numa info";
            DeleteFdExport(obj);
            continue;
        }
        std::string remoteImportNodeId = obj.algoResult.importNumaInfos[0].nodeId;
        if (remoteImportNodeId == GetCurNodeId()) {
            HandleLocalFdImport(obj, remoteImportNodeId, nodeMemDebtInfoMap);
        } else {
            HandleRemoteFdImport(obj, nodeId, remoteImportNodeId, nodeMemDebtInfoMap, allDebtInfoMap);
        }
    }
    return UBSE_OK;
}

void HandleLocalNumaImport(UbseMemNumaBorrowExportObj obj, const std::string &importNodeId,
                           const NodeMemDebtInfoMap &nodeMemDebtInfoMap)
{
    UBSE_LOG_INFO << "numa export name=" << obj.req.name << ", curr node is import node=" << importNodeId;
    std::string resKey = obj.req.name;
    UbseMemNumaBorrowImportObj importObj{};
    if (nodeMemDebtInfoMap.find(importNodeId) != nodeMemDebtInfoMap.end() &&
    nodeMemDebtInfoMap.at(importNodeId).numaImportObjMap.find(resKey) !=
        nodeMemDebtInfoMap.at(importNodeId).numaImportObjMap.end()) {
        importObj = nodeMemDebtInfoMap.at(importNodeId).numaImportObjMap.at(resKey);
        }
    if (importObj.req.name.empty()) {
        // 上下文中不存在导入对象
        UBSE_LOG_INFO << "numa export name=" << obj.req.name << ", curr node is import node=" << importNodeId
                      << ", context not found import resource, start to destroy.";
        DeleteNumaExport(obj);
        return;
    }
    UBSE_LOG_INFO << "numa export name=" << obj.req.name << ", curr node is import node=" << importNodeId
                  << ", import status=" << importObj.status.state;
    if (importObj.status.state == UBSE_MEM_EXPORT_DESTROYED || importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        UBSE_LOG_INFO << "numa export name=" << obj.req.name << ", curr node is import node=" << importNodeId
                      << ",  import resource destroyed, start to destroy.";
        DeleteNumaExport(obj);
    }
}

void HandleRemoteNumaImport(UbseMemNumaBorrowExportObj obj, const std::string &exportId,
                            const std::string &importNodeId, const NodeMemDebtInfoMap &nodeMemDebtInfoMap,
                            const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap)
{
    UBSE_LOG_INFO << "numa export name=" << obj.req.name << ", remote import node=" << importNodeId;
    auto node = UbseNodeController::GetInstance().GetNodeById(importNodeId);
    UBSE_LOG_INFO << "numa export name=" << obj.req.name << ", start to query import nodeId=" << importNodeId;
    if (!node.nodeId.empty() && node.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT) {
        UBSE_LOG_WARN << "nodeId=" << exportId << " both numa export=" << obj.req.name
                      << " import node=" << importNodeId << " fault, start to destroy";
        DeleteNumaExport(obj);
        return;
    }
    UbseMemNumaBorrowImportObj importObj{};
    auto ret = QueryNumaImportObj(importNodeId, obj.req.name, importObj, allDebtInfoMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "req name=" << obj.req.name << " query import nodeId=" << importNodeId << " failed, "
                       << FormatRetCode(ret);
        // 查询失败，跳过本次账本，等待下次处理
        return;
    }
    if (!importObj.req.name.empty()) {
        UBSE_LOG_INFO << "nodeId=" << exportId << " both numa export name=" << obj.req.name
                      << " has import obj, state=" << importObj.status.state;
        if (importObj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED ||
            importObj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            UBSE_LOG_INFO << "nodeId=" << exportId << " both numa export name=" << obj.req.name
                          << " peer import obj, state=destroyed start to destroy";
            DeleteNumaExport(obj);
        }
    } else {
        if (!IsMasterExitsRunningNumaImportObj(nodeMemDebtInfoMap, importNodeId, obj.req.name)) {
            UBSE_LOG_INFO << "nodeId=" << exportId << " both numa export name=" << obj.req.name
                          << " context not exists running peer import obj record, start to destroy";
            DeleteNumaExport(obj);
        }
    }
}

UbseResult BothNumaExportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowExportObj> objs,
                                 const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap)
{
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " both numa export name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }

        if (obj.algoResult.importNumaInfos.size() == 0) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << " both numa export=" << obj.req.name << " has no import numa info";
            DeleteNumaExport(obj);
            continue;
        }
        std::string remoteImportNodeId = obj.algoResult.importNumaInfos[0].nodeId;
        if (remoteImportNodeId == GetCurNodeId()) {
            HandleLocalNumaImport(obj, remoteImportNodeId, nodeMemDebtInfoMap);
        } else {
            HandleRemoteNumaImport(obj, nodeId, remoteImportNodeId, nodeMemDebtInfoMap, allDebtInfoMap);
        }
    }
    return UBSE_OK;
}

void HandleLocalAddrImport(UbseMemAddrBorrowExportObj obj, const std::string &importNodeId,
                           const NodeMemDebtInfoMap &nodeMemDebtInfoMap)
{
    UBSE_LOG_INFO << "addr export name=" << obj.req.name << ", curr node is import node=" << importNodeId;
    std::string resKey = obj.req.name;
    UbseMemAddrBorrowImportObj importObj{};
    if (nodeMemDebtInfoMap.find(importNodeId) != nodeMemDebtInfoMap.end() &&
    nodeMemDebtInfoMap.at(importNodeId).addrImportObjMap.find(resKey) !=
        nodeMemDebtInfoMap.at(importNodeId).addrImportObjMap.end()) {
        importObj = nodeMemDebtInfoMap.at(importNodeId).addrImportObjMap.at(resKey);
        }
    if (importObj.req.name.empty()) {
        // 上下文中不存在导入对象
        UBSE_LOG_INFO << "addr export name=" << obj.req.name << ", curr node is import node=" << importNodeId
                      << ", context not found import resource, start to destroy.";
        DeleteAddrExport(obj);
        return;
    }
    UBSE_LOG_INFO << "addr export name=" << obj.req.name << ", curr node is import node=" << importNodeId
                  << ", import status=" << importObj.status.state;
    if (importObj.status.state == UBSE_MEM_EXPORT_DESTROYED || importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        UBSE_LOG_INFO << "addr export name=" << obj.req.name << ", curr node is import node=" << importNodeId
                      << ",  import resource destroyed, start to destroy.";
        DeleteAddrExport(obj);
    }
}

void HandleRemoteAddrImport(UbseMemAddrBorrowExportObj obj, const std::string &exportId,
                            const std::string &importNodeId, const NodeMemDebtInfoMap &nodeMemDebtInfoMap,
                            const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap)
{
    UBSE_LOG_INFO << "addr export name=" << obj.req.name << ", remote import node=" << importNodeId;
    auto node = UbseNodeController::GetInstance().GetNodeById(importNodeId);
    UBSE_LOG_INFO << "addr export name=" << obj.req.name << ", start to query import nodeId=" << importNodeId;
    if (!node.nodeId.empty() && node.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT) {
        UBSE_LOG_WARN << "nodeId=" << exportId << " both addr export=" << obj.req.name
                      << " import node=" << importNodeId << " fault, start to destroy";
        DeleteAddrExport(obj);
        return;
    }
    UbseMemAddrBorrowImportObj importObj{};
    auto ret = QueryAddrImportObj(importNodeId, obj.req.name, importObj, allDebtInfoMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "req name=" << obj.req.name << " query import nodeId=" << importNodeId << " failed, "
                       << FormatRetCode(ret);
        // 查询失败，跳过本次账本，等待下次处理
        return;
    }
    if (!importObj.req.name.empty()) {
        UBSE_LOG_INFO << "nodeId=" << exportId << " both addr export name=" << obj.req.name
                      << " has import obj, state=" << importObj.status.state;
        if (importObj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED ||
            importObj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            UBSE_LOG_INFO << "nodeId=" << exportId << " both addr export name=" << obj.req.name
                          << " peer import obj, state=destroyed start to destroy";
            DeleteAddrExport(obj);
        }
    } else {
        if (!IsMasterExitsRunningAddrImportObj(nodeMemDebtInfoMap, importNodeId, obj.req.name)) {
            UBSE_LOG_INFO << "nodeId=" << exportId << " both addr export name=" << obj.req.name
                          << " context not exists running peer import obj record, start to destroy";
            DeleteAddrExport(obj);
        }
    }
}

UbseResult BothAddrExportHandler(const std::string &nodeId, std::vector<UbseMemAddrBorrowExportObj> objs,
                                 const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap)
{
    UbseResult ret = UBSE_OK;
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " both addr export name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }

        std::string remoteImportNodeId = obj.req.importNodeId;
        // 查询导入对象是否存在
        if (remoteImportNodeId == GetCurNodeId()) {
            HandleLocalAddrImport(obj, remoteImportNodeId, nodeMemDebtInfoMap);
        } else {
            HandleRemoteAddrImport(obj, nodeId, remoteImportNodeId, nodeMemDebtInfoMap, allDebtInfoMap);
        }
    }
    return UBSE_OK;
}

bool IsMasterExitsRunningFdImportObj(const NodeMemDebtInfoMap &nodeMemDebtInfoMap, const std::string &importNodeId,
                                     const std::string &name)
{
    std::string resKey = name;
    if (nodeMemDebtInfoMap.find(importNodeId) != nodeMemDebtInfoMap.end()) {
        if (nodeMemDebtInfoMap.at(importNodeId).fdImportObjMap.find(resKey) !=
            nodeMemDebtInfoMap.at(importNodeId).fdImportObjMap.end()) {
            // 在对账export去导入端查询import节点，对端不存在后，若master侧存在running状态的导入账本，不删除
            auto obj = nodeMemDebtInfoMap.at(importNodeId).fdImportObjMap.at(resKey);
            if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_RUNNING) {
                UBSE_LOG_INFO << "master node ctx exists running fd importNodeId=" << importNodeId << ", name=" << name
                              << "state=" << static_cast<uint32_t>(obj.status.state);
                return true;
            }
            UBSE_LOG_INFO << "master node ctx exists other state fd importNodeId=" << importNodeId << ", name=" << name
                          << "state=" << static_cast<uint32_t>(obj.status.state);
            return false;
        }
    }
    UBSE_LOG_INFO << "master node ctx not exists fd importNodeId=" << importNodeId << ", name=" << name;
    return false;
}

void HandleAgentDiffFdExportWithRemoteImportNode(UbseMemFdBorrowExportObj obj, const std::string &nodeId,
                                                 const std::string &remoteImportNodeId,
                                                 const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap)
{
    UBSE_LOG_INFO << "agent diff fd export name=" << obj.req.name
                  << ", curr node is import node=" << remoteImportNodeId;
    auto node = UbseNodeController::GetInstance().GetNodeById(remoteImportNodeId);
    if (!node.nodeId.empty() && node.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " agent diff fd export=" << obj.req.name
                      << " import node=" << remoteImportNodeId << " fault, start to destroy";
        DeleteFdExport(obj);
        return;
    }
    UbseMemFdBorrowImportObj importObj{};
    UBSE_LOG_INFO << "agent diff fd export name=" << obj.req.name
                  << ", start to query import nodeId=" << remoteImportNodeId;
    auto ret = QueryFdImportObj(remoteImportNodeId, obj.req.name, importObj, allDebtInfoMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "req name=" << obj.req.name << " query import nodeId=" << remoteImportNodeId << " failed, "
                       << FormatRetCode(ret);
        return;
    }
    if (importObj.req.name.empty()) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff fd export name=" << obj.req.name
                      << " has no import obj, start to destroy";
        DeleteFdExport(obj);
        return;
    }
    UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff fd export name=" << obj.req.name
                  << " has import obj, state=" << importObj.status.state;
    if (importObj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED ||
        importObj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff fd export name=" << obj.req.name
                      << " peer import obj destroyed, start to destroy";
        DeleteFdExport(obj);
        return;
    }
}

UbseResult AgentDiffFdExportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowExportObj> objs,
                                    const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap)
{
    UbseResult ret = UBSE_OK;
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff fd export name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        AddFdExport(obj);

        if (obj.algoResult.importNumaInfos.size() == 0) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << " agent diff fd export=" << obj.req.name
                          << " has no import numa info";
            DeleteFdExport(obj);
            continue;
        }
        std::string remoteImportNodeId = obj.algoResult.importNumaInfos[0].nodeId;
        // 查询导入对象是否存在
        if (remoteImportNodeId == GetCurNodeId()) {
            HandleLocalFdImport(obj, remoteImportNodeId, nodeMemDebtInfoMap);
        } else {
            HandleAgentDiffFdExportWithRemoteImportNode(obj, nodeId, remoteImportNodeId, allDebtInfoMap);
        }
    }
    return UBSE_OK;
}

UbseResult AgentDiffFdImportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowImportObj> objs)
{
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff fd import name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED ||
            obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff fd import name=" << obj.req.name << " start to restore";
        AddFdImport(obj);
    }
    return UBSE_OK;
}

bool IsMasterExitsRunningNumaImportObj(const NodeMemDebtInfoMap &nodeMemDebtInfoMap, const std::string &importNodeId,
                                       const std::string &name)
{
    std::string resKey = name;
    if (nodeMemDebtInfoMap.find(importNodeId) != nodeMemDebtInfoMap.end()) {
        if (nodeMemDebtInfoMap.at(importNodeId).numaImportObjMap.find(resKey) !=
            nodeMemDebtInfoMap.at(importNodeId).numaImportObjMap.end()) {
            // 在对账export去导入端查询import节点，对端不存在后，若master侧存在running状态的导入账本，不删除
            auto obj = nodeMemDebtInfoMap.at(importNodeId).numaImportObjMap.at(resKey);
            if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_RUNNING) {
                UBSE_LOG_INFO << "master node ctx exists running numa importNodeId=" << importNodeId
                              << ", name=" << name << "state=" << static_cast<uint32_t>(obj.status.state);
                return true;
            }
            UBSE_LOG_INFO << "master node ctx exists other state numa importNodeId=" << importNodeId
                          << ", name=" << name << "state=" << static_cast<uint32_t>(obj.status.state);
            return false;
        }
    }
    UBSE_LOG_INFO << "master node ctx not exists numa importNodeId=" << importNodeId << ", name=" << name;
    return false;
}

void HandleAgentDiffNumaExportWithRemoteImportNode(
    UbseMemNumaBorrowExportObj obj, const std::string &nodeId, const std::string &remoteImportNodeId,
    const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap)
{
    UBSE_LOG_INFO << "agent diff numa export name=" << obj.req.name
                  << ", curr node is import node=" << remoteImportNodeId;
    auto node = UbseNodeController::GetInstance().GetNodeById(remoteImportNodeId);
    if (!node.nodeId.empty() && node.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " agent diff numa export=" << obj.req.name
                      << " import node=" << remoteImportNodeId << " fault, start to destroy";
        DeleteNumaExport(obj);
        return;
    }
    UbseMemNumaBorrowImportObj importObj{};
    UBSE_LOG_INFO << "agent diff numa export name=" << obj.req.name
                  << ", start to query import nodeId=" << remoteImportNodeId;
    auto ret = QueryNumaImportObj(remoteImportNodeId, obj.req.name, importObj, allDebtInfoMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "req name=" << obj.req.name << " query import nodeId=" << remoteImportNodeId << " failed, "
                       << FormatRetCode(ret);
        return;
    }
    if (importObj.req.name.empty()) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff numa export name=" << obj.req.name
                      << " has no import obj, start to destroy";
        DeleteNumaExport(obj);
        return;
    }
    UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff numa export name=" << obj.req.name
                  << " has import obj, state=" << importObj.status.state;
    if (importObj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED ||
        importObj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff numa export name=" << obj.req.name
                      << " peer import obj destroyed, start to destroy";
        DeleteNumaExport(obj);
        return;
    }
}

UbseResult AgentDiffNumaExportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowExportObj> objs,
                                      const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap)
{
    UbseResult ret = UBSE_OK;
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff numa export name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        AddNumaExport(obj);

        if (obj.algoResult.importNumaInfos.size() == 0) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << " agent diff numa export=" << obj.req.name
                          << " has no import numa info";
            DeleteNumaExport(obj);
            return UBSE_OK;
        }
        std::string remoteImportNodeId = obj.algoResult.importNumaInfos[0].nodeId;
        if (remoteImportNodeId == GetCurNodeId()) {
            HandleLocalNumaImport(obj, remoteImportNodeId, nodeMemDebtInfoMap);
        } else {
            HandleAgentDiffNumaExportWithRemoteImportNode(obj, nodeId, remoteImportNodeId, allDebtInfoMap);
        }
    }
    return UBSE_OK;
}

UbseResult AgentDiffNumaImportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowImportObj> objs)
{
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff numa import name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED ||
            obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff numa import name=" << obj.req.name << " start to restore";
        AddNumaImport(obj);
    }
    return UBSE_OK;
}

bool IsMasterExitsRunningAddrImportObj(const NodeMemDebtInfoMap &nodeMemDebtInfoMap, const std::string &importNodeId,
                                       const std::string &name)
{
    std::string resKey = name;
    if (nodeMemDebtInfoMap.find(importNodeId) != nodeMemDebtInfoMap.end()) {
        if (nodeMemDebtInfoMap.at(importNodeId).addrImportObjMap.find(resKey) !=
            nodeMemDebtInfoMap.at(importNodeId).addrImportObjMap.end()) {
            // 在对账export去导入端查询import节点，对端不存在后，若master侧存在running状态的导入账本，不删除
            auto obj = nodeMemDebtInfoMap.at(importNodeId).addrImportObjMap.at(resKey);
            if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_RUNNING) {
                UBSE_LOG_INFO << "master node ctx exists running addr importNodeId=" << importNodeId
                              << ", name=" << name << "state=" << static_cast<uint32_t>(obj.status.state);
                return true;
            }
            UBSE_LOG_INFO << "master node ctx exists other state addr importNodeId=" << importNodeId
                          << ", name=" << name << "state=" << static_cast<uint32_t>(obj.status.state);
            return false;
        }
    }
    UBSE_LOG_INFO << "master node ctx not exists addr importNodeId=" << importNodeId << ", name=" << name;
    return false;
}

void HandleAgentDiffAddrExportWithRemoteImportNode(
    UbseMemAddrBorrowExportObj obj, const std::string &nodeId, const std::string &remoteImportNodeId,
    const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap)
{
    UBSE_LOG_INFO << "agent diff addr export name=" << obj.req.name
                  << ", curr node is import node=" << remoteImportNodeId;
    auto node = UbseNodeController::GetInstance().GetNodeById(remoteImportNodeId);
    if (!node.nodeId.empty() && node.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " agent diff addr export=" << obj.req.name
                      << " import node=" << remoteImportNodeId << " fault, start to destroy";
        DeleteAddrExport(obj);
        return;
    }
    UbseMemAddrBorrowImportObj importObj{};
    UBSE_LOG_INFO << "agent diff addr export name=" << obj.req.name
                  << ", start to query import nodeId=" << remoteImportNodeId;
    auto ret = QueryAddrImportObj(remoteImportNodeId, obj.req.name, importObj, allDebtInfoMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "req name=" << obj.req.name << " query import nodeId=" << remoteImportNodeId << " failed, "
                       << FormatRetCode(ret);
        return;
    }
    if (importObj.req.name.empty()) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff addr export name=" << obj.req.name
                      << " has no import obj, start to destroy";
        DeleteAddrExport(obj);
        return;
    }
    UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff addr export name=" << obj.req.name
                  << " has import obj, state=" << importObj.status.state;
    if (importObj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED ||
        importObj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff addr export name=" << obj.req.name
                      << " peer import obj destroyed, start to destroy";
        DeleteAddrExport(obj);
        return;
    }
}

UbseResult AgentDiffAddrExportHandler(const std::string &nodeId, std::vector<UbseMemAddrBorrowExportObj> objs,
                                      const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap)
{
    UbseResult ret = UBSE_OK;
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff addr export name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        AddAddrExport(obj);

        std::string remoteImportNodeId = obj.req.importNodeId;
        // 查询导入对象是否存在
        if (remoteImportNodeId == GetCurNodeId()) {
            HandleLocalAddrImport(obj, remoteImportNodeId, nodeMemDebtInfoMap);
        } else {
            HandleAgentDiffAddrExportWithRemoteImportNode(obj, nodeId, remoteImportNodeId, allDebtInfoMap);
        }
    }
    return UBSE_OK;
}

UbseResult AgentDiffAddrImportHandler(const std::string &nodeId, std::vector<UbseMemAddrBorrowImportObj> objs)
{
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff addr import name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED ||
            obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff addr import name=" << obj.req.name << " start to restore";
        AddAddrImport(obj);
    }
    return UBSE_OK;
}

UbseResult AgentDiffShareExportHandler(const std::string &nodeId, std::vector<UbseMemShareBorrowExportObj> objs)
{
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff share export name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff share export name=" << obj.req.name << " start to restore";
        for (size_t i = 0; i < obj.req.shmRegion.nodelist.size(); i++) {
            // nodeId = slotId = index+1
            obj.req.shmRegion.nodelist[i].nodeId = std::to_string(obj.req.shmRegion.nodelist[i].index + 1);
            UBSE_LOG_INFO << "nodeId=" << nodeId << ", resId=" << obj.req.name
                          << " export shm nodeId=" << obj.req.shmRegion.nodelist[i].nodeId;
        }
        AddShareExport(obj);
    }
    return UBSE_OK;
}

UbseResult AgentDiffShareImportHandler(const std::string &nodeId, std::vector<UbseMemShareBorrowImportObj> objs)
{
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    for (auto obj : objs) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff share import name=" << obj.req.name
                      << " state=" << TransState(obj.status.state);
        if (obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " agent diff share import name=" << obj.req.name << " start to restore";
        for (size_t i = 0; i < obj.req.shmRegion.nodelist.size(); i++) {
            obj.req.shmRegion.nodelist[i].nodeId = std::to_string(obj.req.shmRegion.nodelist[i].index + 1);
            UBSE_LOG_INFO << "nodeId=" << nodeId << ", resId=" << obj.req.name
                          << " import shm nodeId=" << obj.req.shmRegion.nodelist[i].nodeId;
        }
        AddShareImport(obj);
        nodeMemDebtInfoMap[nodeId].shareImportObjMap[obj.req.name] = obj;
    }
    return UBSE_OK;
}

// 判定当前的导入账本 是否处于导出流程中
bool IsProcessExport(UbseMemState state)
{
    return state == UBSE_MEM_EXPORT_RUNNING || state == UBSE_MEM_EXPORT_DESTROYING;
}

template <class ImportObj, class ExportObj>
bool IsImportRunningLastExportSuccess(const std::string &nodeId, const ImportObj &obj, const ExportObj &exportObj,
                                      const char *resourceType)
{
    if (exportObj.status.state == adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS) {
        if (obj.isLastExportSuccess) {
            UBSE_LOG_INFO << "nodeId=" << nodeId << " " << resourceType << " running import name=" << obj.req.name
                          << ", master state=export success, last export also success, should destroy.";
            // 更新对应节点对象状态为删除状态
            message::UbseMemUpdateObjState updateObj{};
            message::UbseMemUpdateObjStatePtr updateObjPtr = new (std::nothrow) message::UbseMemUpdateObjState();
            if (updateObjPtr == nullptr) {
                UBSE_LOG_ERROR << "Failed to new mem update obj state.";
                return UBSE_ERROR_NULLPTR;
            }
            updateObjPtr->objType = resourceType;
            updateObjPtr->obj = obj;
            auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
            if (comModule == nullptr) {
                UBSE_LOG_ERROR << "Failed to get com module.";
                return UBSE_ERROR_NULLPTR;
            }
            SendParam sendParam(obj.req.importNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                                static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_UPDATE_OBJ_STATE_CALLBACK));
            ubse::com::UbseComCallback callback;
            auto ret = comModule->RpcAsyncSend(sendParam, updateObjPtr, callback);
            if (ret == UBSE_OK) {
                UBSE_LOG_INFO << "nodeId=" << nodeId << " " << resourceType << " running import name=" << obj.req.name
                              << ",update import node state rpc send success.";
                return ret;
            }

            return true;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " " << resourceType << " running import name=" << obj.req.name
                      << ", master state=export success, update import last export success.";
        auto copy = obj;
        copy.isLastExportSuccess = true;
        UbseMemDebtLedger::GetInstance().GetDebtMap<ImportObj>().PutResource(obj.req.importNodeId, obj.req.name, copy);
    }
    return false;
}

// 当导入账本 处于导出流程时（export_running, export_destroying） 判定是否需要删除此账本
bool IsFdImportRunningObjExcess(const std::string &nodeId, const UbseMemFdBorrowImportObj &obj)
{
    // 当导入对象状态处于export_destroying时，系统正在执行归还导出操作，判定此时可以删除
    if (obj.status.state == UBSE_MEM_EXPORT_DESTROYING) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " fd running import name=" << obj.req.name
                      << ", master state=export destroying, should destroy.";
        return true;
    }
    // 导入账本的 export_running状态仅存在于主节点；需要根据导出账本的状态来判定是否需要保留
    if (obj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " fd running import name=" << obj.req.name
                      << ", master state=export running, has no export, should destroy.";
        return true;
    }
    UbseMemFdBorrowExportObj exportObj =
        UbseFdExportObjGet(obj.algoResult.exportNumaInfos[0].nodeId, obj.req.name, nodeId);
    if (exportObj.req.name.empty()) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " fd running import name=" << obj.req.name
                      << ", master state=export running, has no export debt";
        return true;
    }
    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " fd running import name=" << obj.req.name
                      << ", master state=export running, export obj destroy.";
        return true;
    }

    if (IsImportRunningLastExportSuccess(nodeId, obj, exportObj, "fd")) {
        return true;
    }
    return false;
}

bool IsNumaImportRunningObjExcess(const std::string &nodeId, const UbseMemNumaBorrowImportObj &obj)
{
    // 当导入对象状态处于export_destroying时，系统正在执行归还导出操作，判定此时可以删除
    if (obj.status.state == UBSE_MEM_EXPORT_DESTROYING) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " numa running import name=" << obj.req.name
                      << ", master state=export destroying, should destroy.";
        return true;
    }
    // 导入账本的 export_running状态仅存在于主节点；需要根据导出账本的状态来判定是否需要保留
    if (obj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " numa running import name=" << obj.req.name
                      << ", master state=export running, has no export, should destroy.";
        return true;
    }
    // 需要根据主节点的导出账本状态推测后续是否会继续执行导入操作
    UbseMemNumaBorrowExportObj exportObj =
        UbseNumaExportObjGet(obj.algoResult.exportNumaInfos[0].nodeId, obj.req.name, nodeId);
    if (exportObj.req.name.empty()) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " numa running import name=" << obj.req.name
                      << ", master state=export running, has no export debt";
        return true;
    }
    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " numa running import name=" << obj.req.name
                      << ", master state=export running, export obj destroy.";
        return true;
    }

    if (IsImportRunningLastExportSuccess(nodeId, obj, exportObj, "numa")) {
        return true;
    }
    return false;
}

bool IsAddrImportRunningObjExcess(const std::string &nodeId, const UbseMemAddrBorrowImportObj &obj)
{
    // 当导入对象状态处于export_destroying时，系统正在执行归还导出操作，判定此时可以删除
    if (obj.status.state == UBSE_MEM_EXPORT_DESTROYING) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " addr running import name=" << obj.req.name
                      << ", master state=export destroying, should destroy.";
        return true;
    }
    // 导入账本的 export_running状态仅存在于主节点；需要根据导出账本的状态来判定是否需要保留
    if (obj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " addr running import name=" << obj.req.name
                      << ", master state=export running, has no export, should destroy.";
        return true;
    }
    // 需要根据主节点的导出账本状态推测后续是否会继续执行导入操作
    UbseMemAddrBorrowExportObj exportObj =
        UbseAddrExportObjGet(obj.algoResult.exportNumaInfos[0].nodeId, obj.req.name, nodeId);
    if (exportObj.req.name.empty()) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " addr running import name=" << obj.req.name
                      << ", master state=export running, has no export debt";
        return true;
    }
    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " addr running import name=" << obj.req.name
                      << ", master state=export running, export obj destroy.";
        return true;
    }

    if (IsImportRunningLastExportSuccess(nodeId, obj, exportObj, "addr")) {
        return true;
    }

    return false;
}

UbseResult MasterRunningFdExportHandler(const std::string &nodeId,
                                        const std::vector<UbseMemFdBorrowExportObj> &masterRunningObjs)
{
    UbseResult ret = UBSE_OK;
    for (auto obj : masterRunningObjs) {
        auto lock = LoggingLockGuard(GenerateExportObjKey(obj.req.name, obj.req.importNodeId));
        UBSE_LOG_INFO << "nodeId=" << nodeId << " fd running export=" << obj.req.name
                      << " master state=" << TransState(obj.status.state);
        UbseMemFdBorrowExportObj ctxObj = UbseFdExportObjGet(nodeId, obj.req.name, obj.req.importNodeId);
        if (ctxObj.req.name.empty()) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " fd running export=" << obj.req.name << " not exists";
            continue;
        }
        if (ctxObj.status.state != obj.status.state) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << " fd running export=" << obj.req.name
                          << " state update, new state=" << TransState(ctxObj.status.state) << ", wait next ledger.";
            continue;
        }
        UbseMemFdBorrowExportObj agentObj{};
        ret = QueryFdExport(
            def::
            UbseMemDebtQueryRequest{name : obj.req.name, importNodeId : obj.req.importNodeId, exportNodeId : nodeId},
            agentObj);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " fd running export=" << obj.req.name << " query agent failed.";
            continue;
        }
        if (agentObj.req.name.empty()) {
            UBSE_LOG_INFO << "nodeId=" << nodeId << " fd running export name=" << obj.req.name
                          << ", master state=" << TransState(obj.status.state) << ", agent not found, will destroy.";
            obj.status.state = UBSE_MEM_EXPORT_DESTROYED;
            AddFdExport(obj);
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " fd running export=" << obj.req.name
                      << ", master state=" << TransState(obj.status.state)
                      << ", agent state=" << TransState(agentObj.status.state);
        if (obj.status.state != agentObj.status.state) {
            obj.status = agentObj.status;
            AddFdExport(obj);
        }
    }
    return UBSE_OK;
}

UbseResult MasterRunningFdImportHandler(const std::string &nodeId,
                                        const std::vector<UbseMemFdBorrowImportObj> &masterRunningObjs)
{
    UbseResult ret = UBSE_OK;
    for (auto obj : masterRunningObjs) {
        auto lock = LoggingLockGuard(GenerateExportObjKey(obj.req.name, obj.req.importNodeId));
        UBSE_LOG_INFO << "nodeId=" << nodeId << " fd running import=" << obj.req.name
                      << " master state=" << TransState(obj.status.state);
        UbseMemFdBorrowImportObj ctxObj = UbseFdImportObjGet(nodeId, obj.req.name);
        if (ctxObj.req.name.empty()) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " fd running import=" << obj.req.name << " not exists";
            continue;
        }
        if (ctxObj.status.state != obj.status.state) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << " fd running import=" << obj.req.name
                          << " state update, new state=" << TransState(ctxObj.status.state) << ", wait next ledger.";
            continue;
        }
        if (IsProcessExport(obj.status.state)) {
            if (IsFdImportRunningObjExcess(nodeId, obj)) {
                obj.status.state = UBSE_MEM_IMPORT_DESTROYED;
                AddFdImport(obj);
            }
            continue;
        }
        UbseMemFdBorrowImportObj agentObj{};
        ret = QueryFdImport(def::UbseMemDebtQueryRequest{name : obj.req.name, importNodeId : nodeId}, agentObj);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " fd running import=" << obj.req.name << " query agent failed.";
            continue;
        }
        if (agentObj.req.name.empty()) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " fd running import name=" << obj.req.name
                           << ", master state=" << TransState(obj.status.state) << ", agent not found, will destroy.";
            obj.status.state = UBSE_MEM_IMPORT_DESTROYED;
            AddFdImport(obj);
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " fd running import name=" << obj.req.name
                      << ", master state=" << TransState(obj.status.state)
                      << ", agent state=" << TransState(agentObj.status.state);
        if (obj.status.state != agentObj.status.state) {
            obj.status = agentObj.status;
            AddFdImport(obj);
        }
    }
    return UBSE_OK;
}

UbseResult MasterRunningNumaExportHandler(const std::string &nodeId,
                                          const std::vector<UbseMemNumaBorrowExportObj> &masterRunningObjs)
{
    UbseResult ret = UBSE_OK;
    for (auto obj : masterRunningObjs) {
        auto lock = LoggingLockGuard(GenerateExportObjKey(obj.req.name, obj.req.importNodeId));
        UBSE_LOG_INFO << "nodeId=" << nodeId << " numa running export=" << obj.req.name
                      << " master state=" << TransState(obj.status.state);
        UbseMemNumaBorrowExportObj ctxObj = UbseNumaExportObjGet(nodeId, obj.req.name, obj.req.importNodeId);
        if (ctxObj.req.name.empty()) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " numa running export=" << obj.req.name << " not exists";
            continue;
        }
        if (ctxObj.status.state != obj.status.state) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << " numa running export=" << obj.req.name
                          << " state update, new state=" << TransState(ctxObj.status.state) << ", wait next ledger.";
            continue;
        }
        UbseMemNumaBorrowExportObj agentObj{};
        ret = QueryNumaExport(
            def::
            UbseMemDebtQueryRequest{name : obj.req.name, importNodeId : obj.req.importNodeId, exportNodeId : nodeId},
            agentObj);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " numa running export=" << obj.req.name << " query agent failed.";
            continue;
        }
        if (agentObj.req.name.empty()) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " numa running export name=" << obj.req.name
                           << ", master state=" << TransState(obj.status.state) << ", agent not found, will destroy.";
            obj.status.state = UBSE_MEM_EXPORT_DESTROYED;
            AddNumaExport(obj);
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " numa running export name=" << obj.req.name
                      << ", master state=" << TransState(obj.status.state)
                      << ", agent state=" << TransState(agentObj.status.state);
        if (obj.status.state != agentObj.status.state) {
            obj.status = agentObj.status;
            AddNumaExport(obj);
        }
    }
    return UBSE_OK;
}

UbseResult MasterRunningNumaImportHandler(const std::string &nodeId,
                                          const std::vector<UbseMemNumaBorrowImportObj> &masterRunningObjs)
{
    UbseResult ret = UBSE_OK;
    for (auto obj : masterRunningObjs) {
        auto lock = LoggingLockGuard(GenerateExportObjKey(obj.req.name, obj.req.importNodeId));
        UBSE_LOG_INFO << "nodeId=" << nodeId << " numa running import=" << obj.req.name
                      << " master state=" << TransState(obj.status.state);
        UbseMemNumaBorrowImportObj ctxObj = UbseNumaImportObjGet(nodeId, obj.req.name);
        if (ctxObj.req.name.empty()) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " numa running import=" << obj.req.name << " not exists";
            continue;
        }
        if (ctxObj.status.state != obj.status.state) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << " numa running import=" << obj.req.name
                          << " state update, new state=" << TransState(ctxObj.status.state) << ", wait next ledger.";
            continue;
        }
        if (IsProcessExport(obj.status.state)) {
            if (IsNumaImportRunningObjExcess(nodeId, obj)) {
                obj.status.state = UBSE_MEM_IMPORT_DESTROYED;
                AddNumaImport(obj);
            }
            continue;
        }
        UbseMemNumaBorrowImportObj agentObj{};
        ret = QueryNumaImport(def::UbseMemDebtQueryRequest{name : obj.req.name, importNodeId : nodeId}, agentObj);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " numa running import=" << obj.req.name << " query agent failed.";
            continue;
        }
        if (agentObj.req.name.empty()) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " numa running import name=" << obj.req.name
                           << ", master state=" << TransState(obj.status.state) << ", agent not found, will destroy.";
            obj.status.state = UBSE_MEM_IMPORT_DESTROYED;
            AddNumaImport(obj);
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " numa running import name=" << obj.req.name
                      << ", master state=" << TransState(obj.status.state)
                      << ", agent state=" << TransState(agentObj.status.state);
        if (obj.status.state != agentObj.status.state) {
            obj.status = agentObj.status;
            AddNumaImport(obj);
        }
    }
    return UBSE_OK;
}

UbseResult MasterRunningAddrExportHandler(const std::string &nodeId,
                                          const std::vector<UbseMemAddrBorrowExportObj> &masterRunningObjs)
{
    UbseResult ret = UBSE_OK;
    for (auto obj : masterRunningObjs) {
        auto lock = LoggingLockGuard(GenerateExportObjKey(obj.req.name, obj.req.importNodeId));
        UBSE_LOG_INFO << "nodeId=" << nodeId << " addr running export=" << obj.req.name
                      << " master state=" << TransState(obj.status.state);
        UbseMemAddrBorrowExportObj ctxObj = UbseAddrExportObjGet(nodeId, obj.req.name, obj.req.importNodeId);
        if (ctxObj.req.name.empty()) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " addr running export=" << obj.req.name << " not exists";
            continue;
        }
        if (ctxObj.status.state != obj.status.state) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << " addr running export=" << obj.req.name
                          << " state update, new state=" << TransState(ctxObj.status.state) << ", wait next ledger.";
            continue;
        }
        UbseMemAddrBorrowExportObj agentObj{};
        ret = QueryAddrExport(
            def::
            UbseMemDebtQueryRequest{name : obj.req.name, importNodeId : obj.req.importNodeId, exportNodeId : nodeId},
            agentObj);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " addr running export=" << obj.req.name << " query agent failed.";
            continue;
        }
        if (agentObj.req.name.empty()) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " addr running export name=" << obj.req.name
                           << ", master state=" << TransState(obj.status.state) << ", agent not found, will destroy.";
            obj.status.state = UBSE_MEM_EXPORT_DESTROYED;
            AddAddrExport(obj);
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " addr running export name=" << obj.req.name
                      << ", master state=" << TransState(obj.status.state)
                      << ", agent state=" << TransState(agentObj.status.state);
        if (obj.status.state != agentObj.status.state) {
            obj.status = agentObj.status;
            AddAddrExport(obj);
        }
    }
    return UBSE_OK;
}

UbseResult MasterRunningAddrImportHandler(const std::string &nodeId,
                                          const std::vector<UbseMemAddrBorrowImportObj> &masterRunningObjs)
{
    UbseResult ret = UBSE_OK;
    for (auto obj : masterRunningObjs) {
        auto lock = LoggingLockGuard(GenerateExportObjKey(obj.req.name, obj.req.importNodeId));
        UBSE_LOG_INFO << "nodeId=" << nodeId << " addr running import=" << obj.req.name
                      << " master state=" << TransState(obj.status.state);
        UbseMemAddrBorrowImportObj ctxObj = UbseAddrImportObjGet(nodeId, obj.req.name);
        if (ctxObj.req.name.empty()) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " addr running import=" << obj.req.name << " not exists";
            continue;
        }
        if (ctxObj.status.state != obj.status.state) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << " addr running import=" << obj.req.name
                          << " state update, new state=" << TransState(ctxObj.status.state) << ", wait next ledger.";
            continue;
        }
        if (IsProcessExport(obj.status.state)) {
            if (IsAddrImportRunningObjExcess(nodeId, obj)) {
                obj.status.state = UBSE_MEM_IMPORT_DESTROYED;
                AddAddrImport(obj);
            }
            continue;
        }
        UbseMemAddrBorrowImportObj agentObj{};
        ret = QueryAddrImport(def::UbseMemDebtQueryRequest{name : obj.req.name, importNodeId : nodeId}, agentObj);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " addr running import=" << obj.req.name << " query agent failed.";
            continue;
        }
        if (agentObj.req.name.empty()) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " addr running import name=" << obj.req.name
                           << ", master state=" << TransState(obj.status.state) << ", agent not found, will destroy.";
            obj.status.state = UBSE_MEM_IMPORT_DESTROYED;
            AddAddrImport(obj);
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " addr running import name=" << obj.req.name
                      << ", master state=" << TransState(obj.status.state)
                      << ", agent state=" << TransState(agentObj.status.state);
        if (obj.status.state != agentObj.status.state) {
            obj.status = agentObj.status;
            AddAddrImport(obj);
        }
    }
    return UBSE_OK;
}

UbseResult MasterRunningShareExportHandler(const std::string &nodeId,
                                           const std::vector<UbseMemShareBorrowExportObj> &masterRunningObjs)
{
    UbseResult ret = UBSE_OK;
    for (auto obj : masterRunningObjs) {
        auto lock = LoggingLockGuard(obj.req.name);
        UBSE_LOG_INFO << "nodeId=" << nodeId << " share running export=" << obj.req.name
                      << " master state=" << TransState(obj.status.state);
        UbseMemShareBorrowExportObj ctxObj = UbseShareExportObjGet(nodeId, obj.req.name);
        if (ctxObj.req.name.empty()) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " share running export=" << obj.req.name << " not exists";
            continue;
        }
        if (ctxObj.status.state != obj.status.state) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << " share running export=" << obj.req.name
                          << " state update, new state=" << TransState(ctxObj.status.state) << ", wait next ledger.";
            continue;
        }
        UbseMemShareBorrowExportObj agentObj{};
        ret = QueryShareExport(def::UbseMemDebtQueryRequest{name : obj.req.name, exportNodeId : nodeId}, agentObj);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " share running export=" << obj.req.name << " query agent failed.";
            continue;
        }
        if (agentObj.req.name.empty()) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " share running export name=" << obj.req.name
                           << ", master state=" << TransState(obj.status.state) << ", agent not found, will destroy.";
            obj.status.state = UBSE_MEM_EXPORT_DESTROYED;
            AddShareExport(obj);
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " share running export name=" << obj.req.name
                      << ", master state=" << TransState(obj.status.state)
                      << ", agent state=" << TransState(agentObj.status.state);
        if (obj.status.state != agentObj.status.state) {
            obj.status = agentObj.status;
            AddShareExport(obj);
        }
    }
    return UBSE_OK;
}

UbseResult MasterRunningShareImportHandler(const std::string &nodeId,
                                           const std::vector<UbseMemShareBorrowImportObj> &masterRunningObjs)
{
    UbseResult ret = UBSE_OK;
    for (auto obj : masterRunningObjs) {
        auto lock = LoggingLockGuard(GenerateExportObjKey(obj.req.name, nodeId));
        UBSE_LOG_INFO << "nodeId=" << nodeId << " share running import=" << obj.req.name
                      << " master state=" << TransState(obj.status.state);
        UbseMemShareBorrowImportObj ctxObj = UbseShareImportObjGet(nodeId, obj.req.name);
        if (ctxObj.req.name.empty()) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " share running import=" << obj.req.name << " not exists";
            continue;
        }
        if (ctxObj.status.state != obj.status.state) {
            UBSE_LOG_WARN << "nodeId=" << nodeId << " share running import=" << obj.req.name
                          << " state update, new state=" << TransState(ctxObj.status.state) << ", wait next ledger.";
            continue;
        }
        UbseMemShareBorrowImportObj agentObj{};
        ret = QueryShareImport(def::UbseMemDebtQueryRequest{name : obj.req.name, importNodeId : nodeId}, agentObj);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " share running import=" << obj.req.name << " query agent failed.";
            continue;
        }
        if (agentObj.req.name.empty()) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " share running import name=" << obj.req.name
                           << ", master state=" << TransState(obj.status.state) << ", agent not found, will destroy.";
            obj.status.state = UBSE_MEM_IMPORT_DESTROYED;
            AddShareImport(obj);
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeId << " share running import name=" << obj.req.name
                      << ", master state=" << TransState(obj.status.state)
                      << ", agent state=" << TransState(agentObj.status.state);
        if (obj.status.state != agentObj.status.state) {
            obj.status = agentObj.status;
            AddShareImport(obj);
        }
    }
    return UBSE_OK;
}

// 收集导出账本中的共享内存信息
std::unordered_map<std::string, std::unordered_map<std::string, size_t>> collectExportInfo(
    const NodeMemDebtInfoMap &debtMap)
{
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> refCountMap;
    for (const auto &[nodeId, nodeDebt] : debtMap) {
        for (const auto &[resId, exportObj] : nodeDebt.shareExportObjMap) {
            const std::string &name = resId;
            if (exportObj.algoResult.exportNumaInfos.empty()) {
                continue;
            }
            const std::string &baseNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
            if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING ||
                exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
                continue;
            }
            refCountMap[name][baseNodeId] = 0;
        }
    }
    return refCountMap;
}

// 更新引用计数
void incrementRefCount(std::unordered_map<std::string, std::unordered_map<std::string, size_t>> &refCountMap,
                       const std::string &name, const std::string &baseNodeId)
{
    auto nameIt = refCountMap.find(name);
    if (nameIt != refCountMap.end()) {
        auto baseIt = nameIt->second.find(baseNodeId);
        if (baseIt != nameIt->second.end()) {
            baseIt->second++;
        }
    }
}
// 更新引用计数
void updateRefCount(std::unordered_map<std::string, std::unordered_map<std::string, size_t>> &refCountMap,
                    const NodeMemDebtInfoMap &debtMap)
{
    for (const auto &[importNodeId, nodeDebt] : debtMap) {
        for (const auto &[resId, importObj] : nodeDebt.shareImportObjMap) {
            const std::string &name = resId;
            if (importObj.algoResult.exportNumaInfos.empty()) {
                continue;
            }
            const std::string &baseNodeId = importObj.algoResult.exportNumaInfos[0].nodeId;
            if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
                continue;
            }
            incrementRefCount(refCountMap, name, baseNodeId);
        }
    }
}

// 返回共享内存引用计数map
std::unordered_map<std::string, std::unordered_map<std::string, size_t>> CountShareMemoryRefCount(
    const NodeMemDebtInfoMap &debtMap)
{
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> refCountMap = collectExportInfo(debtMap);
    updateRefCount(refCountMap, debtMap);
    return refCountMap;
}

// 判断指定 name 的所有 baseNodeId 的引用计数是否都为 0
bool IsAllRefCountZero(const std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> &baseMap,
                       const std::unordered_map<std::string, std::unordered_map<std::string, size_t>> &refCountMap,
                       const std::string &name)
{
    // 先查找 name 在 refCountMap 中是否存在
    auto nameIt = refCountMap.find(name);
    if (nameIt == refCountMap.end()) {
        return false; // 如果 name 不存在，说明引用计数可能为零，或者结构异常
    }

    const auto &refCounts = nameIt->second;

    for (const auto &[baseNodeId, objs] : baseMap) {
        auto baseNodeIt = refCounts.find(baseNodeId);
        if (baseNodeIt == refCounts.end() || baseNodeIt->second != 0) {
            return false;
        }
    }

    return true;
}

// 收集所有共享内存对象
std::vector<UbseMemShareBorrowExportObj> CollectAllExportObjs(
    const std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> &baseMap)
{
    std::vector<UbseMemShareBorrowExportObj> allObjs;
    for (const auto &[baseNodeId, objs] : baseMap) {
        for (const auto &obj : objs) {
            allObjs.emplace_back(obj);
        }
    }
    return allObjs;
}

// 判断对象是否在 toClean 列表中
bool IsObjInVector(const UbseMemShareBorrowExportObj &obj, const std::vector<UbseMemShareBorrowExportObj> &vec)
{
    for (const auto &item : vec) {
        if (item.req.name == obj.req.name &&
            item.algoResult.exportNumaInfos[0].nodeId == obj.algoResult.exportNumaInfos[0].nodeId) {
            return true;
        }
    }
    return false;
}

// 清理所有 anonymous 标记的对象
void CleanAnonymousObjs(const std::vector<UbseMemShareBorrowExportObj> &allObjs,
                        std::vector<UbseMemShareBorrowExportObj> &toClean)
{
    for (const auto &obj : allObjs) {
        if (obj.req.shmAnonymous) {
            toClean.emplace_back(obj);
        }
    }
}

// 过滤掉已清理的对象，保留未清理的
std::vector<UbseMemShareBorrowExportObj> FilterRemainingObjs(const std::vector<UbseMemShareBorrowExportObj> &allObjs,
                                                             const std::vector<UbseMemShareBorrowExportObj> &toClean)
{
    std::vector<UbseMemShareBorrowExportObj> remainingObjs;
    for (const auto &obj : allObjs) {
        if (!IsObjInVector(obj, toClean)) {
            remainingObjs.emplace_back(obj);
        }
    }
    return remainingObjs;
}

// 保留一个对象，其余清理
void CleanExtraObjs(const std::vector<UbseMemShareBorrowExportObj> &remainingObjs,
                    std::vector<UbseMemShareBorrowExportObj> &toClean)
{
    if (remainingObjs.size() > 1) {
        for (size_t i = 1; i < remainingObjs.size(); ++i) {
            toClean.emplace_back(remainingObjs[i]);
        }
    }
}

// 清理引用计数为 0 的对象
void CleanRefCountZeroObjs(const std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> &baseMap,
                           const std::unordered_map<std::string, std::unordered_map<std::string, size_t>> &refCountMap,
                           const std::string &name, std::vector<UbseMemShareBorrowExportObj> &toClean)
{
    if (refCountMap.find(name) == refCountMap.end()) {
        return;
    }
    const auto &refCounts = refCountMap.at(name); // 安全使用 at
    for (const auto &[baseNodeId, objs] : baseMap) {
        if (refCounts.find(baseNodeId) == refCounts.end()) {
            continue;
        }
        if (refCounts.at(baseNodeId) == 0) {
            for (const auto &obj : objs) {
                toClean.emplace_back(obj);
            }
        }
    }
}

// 多个同名内存得到需要清理的对象 判断逻辑
void CheckAndCleanMultiBaseNode(
    const std::string &name, const std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> &baseMap,
    const std::unordered_map<std::string, std::unordered_map<std::string, size_t>> &refCountMap,
    std::vector<UbseMemShareBorrowExportObj> &toClean)
{
    if (IsAllRefCountZero(baseMap, refCountMap, name)) {
        // 收集所有对象
        std::vector<UbseMemShareBorrowExportObj> allObjs = CollectAllExportObjs(baseMap);
        if (allObjs.empty()) {
            return; // 没有对象，直接返回
        }
        // 第一步：清理所有 anonymous 标记的对象
        CleanAnonymousObjs(allObjs, toClean);
        // 第二步：过滤掉已清理的对象，保留未清理的
        std::vector<UbseMemShareBorrowExportObj> remainingObjs = FilterRemainingObjs(allObjs, toClean);
        // 第三步：如果还有对象，保留一个，其余清理
        CleanExtraObjs(remainingObjs, toClean);
    } else {
        // 引用计数不全为 0，清理所有引用计数为 0 的段
        CleanRefCountZeroObjs(baseMap, refCountMap, name, toClean);
    }
}

void CheckAndCleanSingleBaseNode(
    const std::string &name, const std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> &baseMap,
    const std::unordered_map<std::string, std::unordered_map<std::string, size_t>> &refCountMap,
    std::vector<UbseMemShareBorrowExportObj> &toClean)
{
    if (baseMap.empty()) {
        UBSE_LOG_WARN << "map empty.";
        return;
    }
    // 获取第一个节点及其对象
    const auto &[baseNodeId, objs] = *baseMap.begin();
    if (refCountMap.find(name) == refCountMap.end() ||
        refCountMap.at(name).find(baseNodeId) == refCountMap.at(name).end()) {
        UBSE_LOG_WARN << "name" << name << " baseNodeId" << baseNodeId << " not found.";
        return;
    }

    size_t count = refCountMap.at(name).at(baseNodeId);
    if (count == 0 && !objs.empty() && objs[0].req.shmAnonymous) {
        toClean.emplace_back(objs[0]);
    }
}

void ExecuteShareMemoryClean(const std::vector<UbseMemShareBorrowExportObj> &toClean)
{
    if (toClean.empty()) {
        UBSE_LOG_INFO << "No shared memory to clean.";
        return;
    }
    for (const auto &obj : toClean) {
        UBSE_LOG_INFO << "Deleting shared memory: name=" << obj.req.name
                      << ", baseNodeId=" << obj.algoResult.exportNumaInfos[0].nodeId;
        // 调用删除接口删除共享内存对象
        uint32_t result = DeleteShareExport(obj);
    }
}

// 收集导出账本中所有共享内存对象，按 name 和 baseNodeId 分组
UbseMemShareBorrowExportObjMap CollectExportObjsByBaseNode(const NodeMemDebtInfoMap &debtMap)
{
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>>>
        nameBaseToObjsMap;
    for (const auto &[nodeId, nodeDebt] : debtMap) {
        for (const auto &[resId, exportObj] : nodeDebt.shareExportObjMap) {
            const std::string &name = resId;
            if (exportObj.algoResult.exportNumaInfos.empty()) {
                continue;
            }
            const std::string &baseNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
            if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING ||
                exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
                continue;
            }
            nameBaseToObjsMap[name][baseNodeId].emplace_back(exportObj);
        }
    }
    return nameBaseToObjsMap;
}

// 自定义比较函数
bool CompareUbseMemShareBorrowExportObj(const UbseMemShareBorrowExportObj &obj1,
                                        const UbseMemShareBorrowExportObj &obj2)
{
    // 检查 exportNumaInfos 是否为空
    if (obj1.algoResult.exportNumaInfos.empty() || obj2.algoResult.exportNumaInfos.empty()) {
        return false; // 如果为空，返回 false
    }
    // 比较 req.name 和 req.algoResult.exportNumaInfos[0].nodeId
    return obj1.req.name == obj2.req.name &&
           obj1.algoResult.exportNumaInfos[0].nodeId == obj2.algoResult.exportNumaInfos[0].nodeId;
}

std::vector<UbseMemShareBorrowExportObj> GetIntersection(
    const std::vector<UbseMemShareBorrowExportObj> &originalToClean,
    const std::vector<UbseMemShareBorrowExportObj> &currentToClean)
{
    std::vector<UbseMemShareBorrowExportObj> intersection;
    for (const auto &obj : currentToClean) {
        if (std::find_if(originalToClean.begin(), originalToClean.end(),
                         [&obj](const UbseMemShareBorrowExportObj &obj1) {
                             return CompareUbseMemShareBorrowExportObj(obj1, obj);
                         }) != originalToClean.end()) {
            intersection.push_back(obj);
        }
    }
    return intersection;
}

// 处理 toClean 的逻辑
void ProcessCurrentCleanList(const NodeMemDebtInfoMap &debtMap, std::vector<UbseMemShareBorrowExportObj> &toClean)
{
    auto refCountMap = CountShareMemoryRefCount(debtMap);
    auto nameExportObjs = CollectExportObjsByBaseNode(debtMap);
    for (const auto &[name, exportObjs] : nameExportObjs) {
        if (exportObjs.size() > 1) {
            CheckAndCleanMultiBaseNode(name, exportObjs, refCountMap, toClean);
        } else {
            CheckAndCleanSingleBaseNode(name, exportObjs, refCountMap, toClean);
        }
    }
}

std::mutex g_globalMutex;
bool CleanShmTimer(int sleep_seconds)
{
    std::unique_lock<std::mutex> lock(g_globalMutex);
    if (g_globalCv.wait_for(lock, std::chrono::seconds(sleep_seconds), [] { return g_globalStop.load(); })) {
        return false; // 被停止信号中断
    }
    return !g_globalStop.load(); // 返回是否被中断
}

void HandleClean(const std::vector<UbseMemShareBorrowExportObj> &originalToClean)
{
    UBSE_LOG_INFO << "[CleanShm] start to HandleClean.";
    // 等待5分钟
    if (!CleanShmTimer(DEFAULT_SLEEP_SECONDS)) {
        return;
    }
    // 加锁
    UbseNodeControllerLockMgr::WriteLock(ClusterHandlerKey);
    // originalToClean：表示原始的(5分钟前)清理对象列表;currentToClean：表示当前的清理对象列表
    std::vector<UbseMemShareBorrowExportObj> currentToClean;
    NodeMemDebtInfoMap currentDebtMap = GetNodeMemDebtInfoMap();
    // 分别计算5分钟前和当前的清理对象列表
    ProcessCurrentCleanList(currentDebtMap, currentToClean);
    //  计算交集，即最终需要清理的对象列表
    auto toClean = GetIntersection(originalToClean, currentToClean);
    if (!toClean.empty() && !g_globalStop.load()) {
        UBSE_LOG_INFO << "[CleanShm] Collected " << toClean.size() << " shared objects to clean.";
        ExecuteShareMemoryClean(toClean);
    }
    // 释放锁
    UbseNodeControllerLockMgr::WriteUnLock(ClusterHandlerKey);
}

void CleanShmZeroImportHandler()
{
    if (!CheckNodeIsMaster()) {
        return;
    }
    UBSE_LOG_INFO << "[CleanShm] CleanShm started.";
    if (g_globalStop.load()) {
        return;
    }
    std::vector<UbseMemShareBorrowExportObj> originalToClean;
    NodeMemDebtInfoMap originalDebtMap = GetNodeMemDebtInfoMap();
    ProcessCurrentCleanList(originalDebtMap, originalToClean);
    if (originalToClean.empty()) {
        UBSE_LOG_INFO << "[CleanShm] no export obj to clean, CleanShm finished.";
        return;
    }
    HandleClean(originalToClean);
    UBSE_LOG_INFO << "[CleanShm] CleanShm finished.";
}

// 获取共享内存的导出对象映射
std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> GetExportObjsByBaseNode(
    const NodeMemDebtInfoMap &debtMap, const std::string &name)
{
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> baseToObjs;

    for (const auto &[nodeId, nodeDebt] : debtMap) {
        for (const auto &[resId, exportObj] : nodeDebt.shareExportObjMap) {
            if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING ||
                exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
                continue;
            }
            if (resId == name && !exportObj.algoResult.exportNumaInfos.empty()) {
                const std::string &baseNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
                baseToObjs[baseNodeId].emplace_back(exportObj);
            }
        }
    }
    return baseToObjs;
}

// 收集 baseNodeId 和对应的引用计数
std::vector<std::pair<std::string, size_t>> CollectBaseRefCount(
    const std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> &baseToObjs,
    const std::unordered_map<std::string, std::unordered_map<std::string, size_t>> &refCountMap,
    const std::string &name)
{
    std::vector<std::pair<std::string, size_t>> baseRefCount;
    baseRefCount.reserve(baseToObjs.size());
    // 检查 name 是否为空
    if (name.empty()) {
        UBSE_LOG_WARN << "name is empty, skip collecting ref count.";
        return baseRefCount;
    }
    // 检查 refCountMap 中是否存在该 name
    auto nameIt = refCountMap.find(name);
    if (nameIt == refCountMap.end()) {
        UBSE_LOG_WARN << "name=" << name << " not found in refCountMap, skip collecting ref count.";
        return baseRefCount;
    }
    for (const auto &[baseNodeId, objs] : baseToObjs) {
        // 检查 baseNodeId 是否为空
        if (baseNodeId.empty()) {
            UBSE_LOG_WARN << "baseNodeId is empty, skip this entry.";
            continue;
        }
        auto baseIt = nameIt->second.find(baseNodeId);
        if (baseIt != nameIt->second.end()) {
            baseRefCount.emplace_back(baseNodeId, baseIt->second);
        } else {
            UBSE_LOG_WARN << "baseNodeId=" << baseNodeId << " not found in refCountMap for name=" << name;
        }
    }

    return baseRefCount;
}

// 找出最大引用计数的 baseNodeId
std::string FindMaxRefCountBaseNode(const std::vector<std::pair<std::string, size_t>> &baseRefCount)
{
    size_t maxRefCount = 0;
    std::string maxBaseNodeId;
    for (const auto &[baseNodeId, count] : baseRefCount) {
        if (count >= maxRefCount) {
            maxRefCount = count;
            maxBaseNodeId = baseNodeId;
        }
    }
    UBSE_LOG_INFO << "maxRefCount=" << maxRefCount << ", maxBaseNodeId=" << maxBaseNodeId;
    return maxBaseNodeId;
}

// 获取与 baseNodeId 匹配的导出对象
UbseMemShareBorrowExportObj GetExportObjByBaseNode(
    const std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> &baseToObjs,
    const std::string &baseNodeId)
{
    for (const auto &[nodeId, objs] : baseToObjs) {
        if (nodeId == baseNodeId && !objs.empty()) {
            return objs[0];
        }
    }
    UBSE_LOG_WARN << "no export obj to match";
    return UbseMemShareBorrowExportObj{};
}

// 判断导入对象是否匹配指定的 name 和 baseNodeId
bool IsImportObjMatch(const UbseMemShareBorrowImportObj &importObj, const std::string &name,
                      const std::string &baseNodeId)
{
    if (importObj.algoResult.exportNumaInfos.empty()) {
        return false;
    }
    const std::string &importBaseNodeId = importObj.algoResult.exportNumaInfos[0].nodeId;
    return importObj.req.name == name && importBaseNodeId == baseNodeId;
}

// 从单个节点的导入账本中收集匹配的导入对象
std::vector<UbseMemShareBorrowImportObj> CollectImportObjsFromNode(
    const std::unordered_map<std::string, UbseMemShareBorrowImportObj> &importMap, const std::string &name,
    const std::string &baseNodeId)
{
    std::vector<UbseMemShareBorrowImportObj> matchedObjs;

    for (const auto &[resId, importObj] : importMap) {
        if (IsImportObjMatch(importObj, name, baseNodeId)) {
            matchedObjs.emplace_back(importObj);
        }
    }
    UBSE_LOG_INFO << "matchedObjs=" << matchedObjs.size();
    return matchedObjs;
}

// 获取name下所有导入对象的子函数
std::vector<std::shared_ptr<const UbseMemShareBorrowImportObj>> GetAllImportObjsByName(const std::string &name)
{
    std::vector<std::shared_ptr<const UbseMemShareBorrowImportObj>> allImportObjs;
    auto &ledger = UbseMemDebtLedger::GetInstance();
    auto allNodeMaps = ledger.GetDebtMap<UbseMemShareBorrowImportObj>().GetAllNodeMaps();
    for (const auto &[nodeId, nodeMap] : allNodeMaps) {
        auto importObjPtr = nodeMap->Get(name);
        if (importObjPtr && importObjPtr->status.state != UBSE_MEM_IMPORT_DESTROYED) {
            allImportObjs.emplace_back(importObjPtr);
        }
    }
    UBSE_LOG_INFO << "Total import objects found for name=" << name << ": " << allImportObjs.size();
    return allImportObjs;
}

// 收集指定 name 的导出对象，按 baseNodeId 分组
std::unordered_map<std::string, std::shared_ptr<const UbseMemShareBorrowExportObj>> CollectExportObjsByName(
    const std::string &name)
{
    auto baseToExportObjs = std::unordered_map<std::string, std::shared_ptr<const UbseMemShareBorrowExportObj>>();

    auto &ledger = UbseMemDebtLedger::GetInstance();
    auto allExportNodeMaps = ledger.GetDebtMap<UbseMemShareBorrowExportObj>().GetAllNodeMaps();

    for (const auto &[nodeId, nodeMap] : allExportNodeMaps) {
        auto exportObjPtr = nodeMap->Get(name);
        if (!exportObjPtr) {
            continue;
        }
        if (exportObjPtr->status.state == UBSE_MEM_EXPORT_DESTROYING ||
            exportObjPtr->status.state == UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        if (!exportObjPtr->algoResult.exportNumaInfos.empty()) {
            const std::string &baseNodeId = exportObjPtr->algoResult.exportNumaInfos[0].nodeId;
            baseToExportObjs[baseNodeId] = exportObjPtr;
        }
    }

    return baseToExportObjs;
}

void CollectImportObjsAndRefCount(
    const std::string &name, std::unordered_map<std::string, size_t> &refCountByBaseNode,
    std::unordered_map<std::string, std::vector<std::shared_ptr<const UbseMemShareBorrowImportObj>>> &baseToImportObjs)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    auto allImportNodeMaps = ledger.GetDebtMap<UbseMemShareBorrowImportObj>().GetAllNodeMaps();

    for (const auto &[nodeId, nodeMap] : allImportNodeMaps) {
        auto allResources = nodeMap->GetAll();
        for (const auto &[resId, importObjPtr] : allResources) {
            if (resId != name) {
                continue;
            }
            if (importObjPtr->status.state == UBSE_MEM_IMPORT_DESTROYED) {
                continue;
            }
            if (!importObjPtr->algoResult.exportNumaInfos.empty()) {
                const std::string &baseNodeId = importObjPtr->algoResult.exportNumaInfos[0].nodeId;
                refCountByBaseNode[baseNodeId]++;
                baseToImportObjs[baseNodeId].emplace_back(importObjPtr);
            }
        }
    }
}

// 找出最大引用计数的 baseNodeId
std::string FindMaxRefCountBaseNode(
    const std::unordered_map<std::string, std::shared_ptr<const UbseMemShareBorrowExportObj>> &baseToExportObjs,
    const std::unordered_map<std::string, size_t> &refCountByBaseNode)
{
    std::string selectedBaseNodeId;
    size_t maxRefCount = 0;

    for (const auto &[baseNodeId, objs] : baseToExportObjs) {
        size_t count = refCountByBaseNode.count(baseNodeId) ? refCountByBaseNode.at(baseNodeId) : 0;
        if (count >= maxRefCount) {
            maxRefCount = count;
            selectedBaseNodeId = baseNodeId;
        }
    }

    return selectedBaseNodeId;
}

UbseMemShareExportWithImports GetMaxRefCountExportObj(const std::string &name)
{
    // 1. 收集该 name 相关的导出对象，按 baseNodeId 分组
    auto baseToExportObjs = CollectExportObjsByName(name);
    // 2. 如果没有找到对应的导出对象，返回该 name 的所有导入对象
    if (baseToExportObjs.empty()) {
        UBSE_LOG_INFO << "name=" << name << " no shared memory object found, returning all import objects";
        return {nullptr, GetAllImportObjsByName(name)};
    }

    // 3. 收集该 name 相关的导入对象，计算引用计数并按 baseNodeId 分组
    std::unordered_map<std::string, size_t> refCountByBaseNode;
    std::unordered_map<std::string, std::vector<std::shared_ptr<const UbseMemShareBorrowImportObj>>> baseToImportObjs;
    CollectImportObjsAndRefCount(name, refCountByBaseNode, baseToImportObjs);

    // 4. 找出最大引用计数的 baseNodeId
    std::string selectedBaseNodeId = FindMaxRefCountBaseNode(baseToExportObjs, refCountByBaseNode);

    UBSE_LOG_INFO << "name=" << name << " selectedBaseNodeId=" << selectedBaseNodeId << " maxRefCount="
                  << (refCountByBaseNode.count(selectedBaseNodeId) ? refCountByBaseNode.at(selectedBaseNodeId) : 0);

    // 5. 构建返回结果
    auto exportObjPtr = baseToExportObjs[selectedBaseNodeId];
    auto importObjPtrs = std::vector<std::shared_ptr<const UbseMemShareBorrowImportObj>>();
    if (baseToImportObjs.count(selectedBaseNodeId)) {
        importObjPtrs = std::move(baseToImportObjs[selectedBaseNodeId]);
    }

    UBSE_LOG_INFO << "name=" << name << " baseNodeId=" << selectedBaseNodeId << " found " << importObjPtrs.size()
                  << " import objects.";
    return {exportObjPtr, importObjPtrs};
}

struct PairHash {
    std::size_t operator()(const std::pair<uint32_t, uint32_t>& p) const
    {
        return std::hash<uint64_t>{}((static_cast<uint64_t>(p.first) << NO_32) | p.second);
    }
};

static std::unordered_set<std::pair<uint32_t, uint32_t>, PairHash> GetLinkUpPorts(const std::string &targetNodeId)
{
    auto allLinkInfos = nodeController::UbseNodeController::GetInstance().UbseGetDirConnectInfo();
    std::unordered_set<std::pair<uint32_t, uint32_t>, PairHash> linkUpPorts;
    for (const auto &[_, physicalLink] : allLinkInfos) {
        // interfaceName非空，本端端口UP
        if (std::to_string(physicalLink.slotId) == targetNodeId && !physicalLink.interfaceName.empty()) {
            UBSE_LOG_INFO << "physicalLink=" << physicalLink.interfaceName << ", chipId=" << physicalLink.chipId
                          << ", portId=" << physicalLink.portId;
            linkUpPorts.insert({physicalLink.chipId, physicalLink.portId});
        }
        if (std::to_string(physicalLink.peerSlotId) == targetNodeId && !physicalLink.peerInterfaceName.empty()) {
            UBSE_LOG_INFO << "physicalLink=" << physicalLink.peerInterfaceName << ", peerChipId="
                          << physicalLink.peerChipId << ", peerPortId=" << physicalLink.peerPortId;
            linkUpPorts.insert({physicalLink.peerChipId, physicalLink.peerPortId});
        }
    }
    return linkUpPorts;
}

static void BuildLinkToNumaMap(
    const std::string &targetNodeId,
    const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap,
    std::unordered_map<std::pair<uint32_t, uint32_t>, int64_t, PairHash> &linkToNumaMap,
    std::set<int64_t> &invalidRemoteNumaIds,
    std::set<int64_t> &unknownRemoteNumaIds)
{
    for (const auto &[nodeId, nodeDebt] : allDebtInfoMap) {
        if (nodeId != targetNodeId) {
            continue;
        }
        for (const auto &[resId, importObjs] : nodeDebt.numaImportObjMap) {
            if (importObjs.algoResult.importNumaInfos.empty() || importObjs.status.importResults.empty() ||
                importObjs.algoResult.exportNumaInfos.empty()) {
                continue;
            }
            // 当remoteNuma存在不完整账本，则下发该numa状态为unknown
            if (allDebtInfoMap.find(importObjs.algoResult.exportNumaInfos[0].nodeId) == allDebtInfoMap.end()) {
                unknownRemoteNumaIds.insert(importObjs.status.importResults[0].numaId);
            } else {
                // 当remoteNuma存在单导入账本，则发下该numa状态为不可用
                const auto &exportNodeDebt = allDebtInfoMap.at(
                    importObjs.algoResult.exportNumaInfos[0].nodeId).numaExportObjMap;
                const std::string exportResId = resId + "_" + nodeId;
                if (exportNodeDebt.find(exportResId) == exportNodeDebt.end()) {
                    invalidRemoteNumaIds.insert(importObjs.status.importResults[0].numaId);
                }
            }

            auto key = std::make_pair(importObjs.algoResult.importNumaInfos[0].chipId,
                                      importObjs.algoResult.importNumaInfos[0].portId);
            linkToNumaMap[key] = importObjs.status.importResults[0].numaId;
        }
    }
}

static std::vector<std::pair<int64_t, int>> BuildNumaStatus(
    const std::unordered_map<std::pair<uint32_t, uint32_t>, int64_t, PairHash> &linkToNumaMap,
    const std::set<int64_t> &invalidRemoteNumaIds,
    const std::set<int64_t> &unknownRemoteNumaIds,
    const std::unordered_set<std::pair<uint32_t, uint32_t>, PairHash> &linkUpPorts)
{
    std::vector<std::pair<int64_t, int>> numaStatus;
    numaStatus.reserve(linkToNumaMap.size());
    for (const auto [linkPair, numaId] : linkToNumaMap) {
        // 0表示可用，1表示不可用，2表示状态未知
        if (linkUpPorts.find(linkPair) == linkUpPorts.end()) {
            numaStatus.push_back({numaId, static_cast<int>(RemoteNumaStatus::UNAVAILABLE)});
        } else {
            if (invalidRemoteNumaIds.find(numaId) != invalidRemoteNumaIds.end()) {
                numaStatus.push_back({numaId, static_cast<int>(RemoteNumaStatus::UNAVAILABLE)});
            } else if (unknownRemoteNumaIds.find(numaId) != unknownRemoteNumaIds.end()) {
                numaStatus.push_back({numaId, static_cast<int>(RemoteNumaStatus::UNKNOWN)});
            } else {
                numaStatus.push_back({numaId, static_cast<int>(RemoteNumaStatus::AVAILABLE)});
            }
        }
    }
    return numaStatus;
}

void MasterNotifyRemoteNumaStatus(const std::string &targetNodeId,
                                  const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap)
{
    auto linkUpPorts = GetLinkUpPorts(targetNodeId);

    std::unordered_map<std::pair<uint32_t, uint32_t>, int64_t, PairHash> linkToNumaMap;
    std::set<int64_t> invalidRemoteNumaIds;
    std::set<int64_t> unknownRemoteNumaIds;
    BuildLinkToNumaMap(targetNodeId, allDebtInfoMap, linkToNumaMap, invalidRemoteNumaIds, unknownRemoteNumaIds);

    auto numaStatus = BuildNumaStatus(linkToNumaMap, invalidRemoteNumaIds, unknownRemoteNumaIds,
                                      linkUpPorts);
    if (numaStatus.empty()) {
        return;
    }
    auto ret = NotifyRemoteNumaStatus(targetNodeId, numaStatus);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "QueryRemoteNumaStatus failed, ret=" << ret;
        return;
    }
}

UbseResult AgentModifyRemoteNumaStatus(const std::vector<std::pair<int64_t, int>> &numaStatus)
{
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Failed to get ubseMemController";
        return UBSE_ERROR;
    }

    resourceExecutor->Execute([numaStatus]() {
        std::vector<__u32> caps{CAP_DAC_OVERRIDE};
        UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
        for (const auto &[numaId, status] : numaStatus) {
            if (numaId < 0 || numaId > TOPOLOGY_MAX_TOTAL_NUMA) {
                UBSE_LOG_ERROR << "Invalid numa id=" << numaId << ", status=" << status;
                continue;
            }

            // numa状态未知，不更改numa状态
            if (status == static_cast<int>(RemoteNumaStatus::UNKNOWN)) {
                continue;
            }

            std::string criticalErrFilePath = CRITICAL_ERR_PATH_PREFIX + std::to_string(numaId) + CRITICAL_ERR_FILE;

            std::ofstream criticalErrFileStream(criticalErrFilePath, std::ios::out | std::ios::trunc);
            if (!criticalErrFileStream.is_open()) {
                UBSE_LOG_ERROR << "Failed to open critical_err file, numaId=" << numaId;
                continue;
            }

            std::string content = (status == static_cast<int>(RemoteNumaStatus::AVAILABLE)) ? "0" : "1";
            UBSE_LOG_INFO << "Write numa status to critical_err file, numaId=" << numaId << ", status=" << status;

            criticalErrFileStream << content;
            criticalErrFileStream.close();

            if (criticalErrFileStream.fail()) {
                UBSE_LOG_ERROR << "Failed to write critical_err file, numaId=" << numaId;
            }
        }
        UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
    });
    return UBSE_OK;
}

} // namespace ubse::mem::controller
