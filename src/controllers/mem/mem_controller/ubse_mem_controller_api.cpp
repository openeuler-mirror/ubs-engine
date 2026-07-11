/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/
#include "ubse_mem_controller_api.h"
#include <unistd.h>
#include <cstdint>

#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_election_module.h"
#include "ubse_error.h"
#include "ubse_event.h"
#include "ubse_logger.h"
#include "ubse_logger_audit.h"
#include "ubse_mem_account.h"
#include "ubse_mem_api.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_controller_ledger.h"
#include "ubse_mem_controller_master_online_handler.h"
#include "ubse_mem_controller_msg.h"
#include "ubse_mem_controller_pre_online.h"
#include "ubse_mem_debt_info.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_mem_def.h"
#include "ubse_mem_prehandle_manager.h"
#include "ubse_mem_scheduler.h"
#include "ubse_mem_sign_verifier.h"
#include "ubse_mem_util.h"
#include "ubse_mmi_interface.h"
#include "ubse_mmi_module.h"
#include "ubse_node.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_module.h"
#include "ubse_obmm_utils.h"
#include "ubse_request_id_util.h"
#include "ubse_str_util.h"
#include "ubse_thread_pool_module.h"
#include "ubse_timer.h"
#include "ubse_topo_util.h"
#include "api/ubse_mem_controller_api_common.h"
#include "message/node_mem_debtInfo_query_req_simpo.h"
#include "message/node_mem_debt_info_simpo.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::election;
using namespace ubse::log;
using namespace ubse::mem::scheduler;
using namespace ubse::nodeController;
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::task_executor;
using namespace ubse::com;
using namespace message;
using namespace ubse::mmi;
using namespace ubse::mem::strategy;
using namespace ubse::event;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::utils;
using namespace ::api::server;
using namespace ubse::nodeController;
using namespace ubse::timer;
using namespace ubse::mem::util;
using namespace ubse::mem::controller::debt;

const uint32_t UBSE_MEM_SHM_CLEAN_INTERVAL = 300; // 中心侧共享内存零引用清零周期；单位秒
const std::string UBSE_MEM_SHM_CLEAN_TIMER = "UbseMemShmClean";

void RegisterNodeCtlNotify()
{
    UbseNodeController::GetInstance().RegLocalStateNotifyHandler(LoadLocalAllObjs);
    UbseNodeController::GetInstance().RegClusterStateNotifyHandler(PreOnlineHandler);
    UbseNodeController::GetInstance().RegClusterStateNotifyHandler(LedgerHandler);
    std::string eventId = UBSE_EVENT_CLUSTER_TOPOLOGY_CHANGE;
    UbseSubEvent(eventId, LcneTopologyChangeHandler, HIGH);
    auto taskExecutor = GetExecutor("ubseMemController");
    if (taskExecutor == nullptr) {
        UBSE_LOG_WARN << "mem executor empty.";
        return;
    }
    UbseTimerHandlerRegister(
        UBSE_MEM_SHM_CLEAN_TIMER,
        [taskExecutor]() -> UbseResult {
            if (taskExecutor == nullptr) {
                UBSE_LOG_WARN << "mem executor empty, skip clean shm";
                return UBSE_OK;
            }
            taskExecutor->Execute([]() -> void { CleanShmZeroImportHandler(); });
            return UBSE_OK;
        },
        UBSE_MEM_SHM_CLEAN_INTERVAL);
}

uint32_t CreateTaskExecutor()
{
    auto executorModule = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (executorModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get executor module.";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = executorModule->Create("ubseMemController", 18, 1000);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to create executor.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t Init()
{
    UbseResult ret = UBSE_OK;
    if (ret = ubse::mem::scheduler::Init(); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to init scheduler, " << FormatRetCode(ret);
        return ret;
    }
    UbseMemControllerMasterOnlineHandler::Initial();
    return UBSE_OK;
}

void UnInit()
{
    UbseMemControllerMasterOnlineHandler::Uninitial();
    std::string eventId = UBSE_EVENT_CLUSTER_TOPOLOGY_CHANGE;
    UbseUnSubEvent(eventId, LcneTopologyChangeHandler);
}

void Stop()
{
    PreOnLineUnInit();
    UbseTimerHandlerUnregister(UBSE_MEM_SHM_CLEAN_TIMER);
}

void LoadObjState(NodeMemDebtInfo& nodeMemDebtInfo)
{
    for (const auto& item : nodeMemDebtInfo.fdImportObjMap) {
        nodeMemDebtInfo.fdImportObjMap[item.first].status.state = UBSE_MEM_IMPORT_SUCCESS;
    }
    for (const auto& item : nodeMemDebtInfo.fdExportObjMap) {
        nodeMemDebtInfo.fdExportObjMap[item.first].status.state = UBSE_MEM_EXPORT_SUCCESS;
    }
    for (const auto& item : nodeMemDebtInfo.numaExportObjMap) {
        nodeMemDebtInfo.numaExportObjMap[item.first].status.state = UBSE_MEM_EXPORT_SUCCESS;
    }
    for (const auto& item : nodeMemDebtInfo.numaImportObjMap) {
        nodeMemDebtInfo.numaImportObjMap[item.first].status.state = UBSE_MEM_IMPORT_SUCCESS;
    }
    for (const auto& item : nodeMemDebtInfo.shareImportObjMap) {
        nodeMemDebtInfo.shareImportObjMap[item.first].status.state = UBSE_MEM_IMPORT_SUCCESS;
    }
    for (const auto& item : nodeMemDebtInfo.shareExportObjMap) {
        nodeMemDebtInfo.shareExportObjMap[item.first].status.state = UBSE_MEM_EXPORT_SUCCESS;
    }
    for (const auto& item : nodeMemDebtInfo.addrExportObjMap) {
        nodeMemDebtInfo.addrExportObjMap[item.first].status.state = UBSE_MEM_EXPORT_SUCCESS;
    }
    for (const auto& item : nodeMemDebtInfo.addrImportObjMap) {
        nodeMemDebtInfo.addrImportObjMap[item.first].status.state = UBSE_MEM_IMPORT_SUCCESS;
    }
}

bool SignShareExportForRecovery(UbseMemShareBorrowExportObj& exportObj)
{
    constexpr uint32_t maxRetries = 5;
    if (!IsHighSafety()) {
        return true;
    }
    for (uint32_t retry = 0; retry <= maxRetries; ++retry) {
        if (const auto ret = UbseMemSignVerifier::Sign("share", exportObj.req.trustRingData.reqSignedData,
                                                       exportObj.req.trustRingData.trustRingId);
            ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to sign request, " << FormatRetCode(ret) << ", name=" << exportObj.req.name;
            continue;
        }
        UbseExportSignReq trustReq{exportObj.req.trustRingData.reqSignedData, "share", exportObj.status.exportObmmInfo,
                                   exportObj.req.trustRingData.trustRingId};
        if (const auto ret = UbseMemSignVerifier::SignAndVerify(trustReq, exportObj.req.trustRingData.lendSignedDatas);
            ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to sign lend information, " << FormatRetCode(ret)
                           << ", name=" << exportObj.req.name << ", trustRingId=" << trustReq.trustRingId;
            sleep(SEND_RETRY_DURATION);
            continue;
        }
        return true;
    }
    return false;
}

uint32_t LoadLocalAllObjs(const ubse::nodeController::UbseNodeInfo& node)
{
    UBSE_LOG_INFO << "local node state change, state=" << static_cast<uint32_t>(node.localState);
    if (node.localState == UbseNodeLocalState::UBSE_NODE_READY) {
        UBSE_LOG_INFO << "local node ready, skip load obj";
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "local node state change, start load obj";
    NodeMemDebtInfo nodeMemDebtInfo{};
    if (auto ret = UbseMmiInterface::GetInstance().GetObjData(nodeMemDebtInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to load all objs from obmm.";
        return ret;
    }
    for (auto& exportObj : nodeMemDebtInfo.shareExportObjMap) {
        for (auto& nodeInfo : exportObj.second.req.shmRegion.nodelist) {
            nodeInfo.nodeId = std::to_string(nodeInfo.index + 1);
        }
        if (!SignShareExportForRecovery(exportObj.second)) {
            return UBSE_ERROR;
        }
    }
    for (auto& importObj : nodeMemDebtInfo.shareImportObjMap) {
        for (auto& nodeInfo : importObj.second.req.shmRegion.nodelist) {
            nodeInfo.nodeId = std::to_string(nodeInfo.index + 1);
        }
    }
    LoadObjState(nodeMemDebtInfo);
    UbseMemDebtLedger::GetInstance().LoadFromNodeMemDebtInfo(node.nodeId, nodeMemDebtInfo);

    return UBSE_OK;
}

uint16_t GetMarId(const std::string& portGroupIdStr)
{
    if (portGroupIdStr.empty()) {
        UBSE_LOG_WARN << "The portGroupIdStr=" << portGroupIdStr;
        return 0;
    }
#ifdef UB_ENVIRONMENT
    uint64_t value{};
    auto ret = ubse::utils::ConvertStrToUint64(portGroupIdStr, value);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "portGroupIdStr(" << portGroupIdStr << ") is invalid.";
        return INVALID_VALUE16;
    }
    if (value > UINT16_MAX) {
        UBSE_LOG_ERROR << "portGroupIdStr(" << portGroupIdStr << ") is invalid.";
        return INVALID_VALUE16;
    }
    const auto marId = static_cast<uint16_t>(value) / 4u;
    return marId;
#else
    return 0;
#endif
}

void FillAttachSocketId(UbseMemBorrowImportBaseObj& importObj, const bool isFdOrAddr,
                        const UbseNodeMemCnaInfoOutput& cnaOutput)
{
    try {
        importObj.algoResult.attachSocketId = std::stol(cnaOutput.borrowSocketId);
    } catch (const std::invalid_argument& e) {
        importObj.algoResult.attachSocketId = 0;
        UBSE_LOG_ERROR << "Invalid argument: " << e.what();
    } catch (const std::out_of_range& e) {
        importObj.algoResult.attachSocketId = 0;
        UBSE_LOG_ERROR << "Out of range: " << e.what();
    }
}

struct PortCnaResult {
    bool found{false};
    uint32_t dcna{0};
    std::string borrowPortId; // 导入端端口(remotePortId)
    std::string portId;       // 导出端端口
};

// 从export节点的portInfos中选择端口并解析dcna和borrowPortId:
// specifiedPortId != UINT32_MAX时使用指定端口, 否则选择连接到importNodeId且portCna最小的端口
static PortCnaResult SelectPortCna(const std::string& exportNodeId, const std::string& exportSocketIdStr,
                                   const std::string& importNodeId, uint32_t specifiedPortId)
{
    uint32_t socketId{};
    if (ConvertStrToUint32(exportSocketIdStr, socketId) != UBSE_OK) {
        UBSE_LOG_INFO << "Invalid exportSocketId: " << exportSocketIdStr;
        return {};
    }
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(exportNodeId);
    const UbseCpuInfo* cpu = nullptr;
    for (const auto& cpuInfo : nodeInfo.cpuInfos) {
        if (cpuInfo.second.socketId == socketId) {
            cpu = &cpuInfo.second;
            break;
        }
    }
    if (cpu == nullptr) {
        UBSE_LOG_WARN << "socket " << socketId << " not found in cpuInfo";
        return {};
    }
    if (specifiedPortId != UINT32_MAX) {
        for (const auto& portInfo : cpu->portInfos) {
            if (portInfo.second.portId == std::to_string(specifiedPortId) &&
                portInfo.second.portStatus == PortStatus::UP && portInfo.second.remoteSlotId == importNodeId) {
                UBSE_LOG_INFO << "Found specified port " << specifiedPortId << " dcna=" << portInfo.second.portCna
                              << " borrowPortId=" << portInfo.second.remotePortId;
                return {true, portInfo.second.portCna, portInfo.second.remotePortId, portInfo.second.portId};
            }
        }
        UBSE_LOG_WARN << "Specified port " << specifiedPortId << " not found";
        return {};
    }
    // 未指定端口: 选择连接到importNodeId且对端portId最小的端口
    PortCnaResult result;
    uint32_t minRemotePortId = UINT32_MAX;
    for (const auto& portInfo : cpu->portInfos) {
        if (portInfo.second.portStatus == PortStatus::UP && portInfo.second.remoteSlotId == importNodeId) {
            uint32_t remotePortId = 0;
            if (ConvertStrToUint32(portInfo.second.remotePortId, remotePortId) == UBSE_OK &&
                remotePortId < minRemotePortId) {
                minRemotePortId = remotePortId;
                result.dcna = portInfo.second.portCna;
                result.borrowPortId = portInfo.second.remotePortId;
                result.portId = portInfo.second.portId;
                result.found = true;
            }
        }
    }
    if (result.found) {
        UBSE_LOG_INFO << "Selected min port dcna=" << result.dcna << " borrowPortId=" << result.borrowPortId
                      << " portId=" << result.portId;
    }
    return result;
}

void FillImportNumaPortAndChipId(const std::string& exportNodeId, int exportSocketId, const std::string& importNodeId,
                                 std::vector<UbseMemDebtNumaInfo>& importNumaInfos, uint32_t specifiedPortId)
{
    if (importNumaInfos.empty()) {
        return;
    }
    auto result = SelectPortCna(exportNodeId, std::to_string(exportSocketId), importNodeId, specifiedPortId);
    for (auto& numaInfo : importNumaInfos) {
        auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(numaInfo.nodeId);
        if (nodeInfo.nodeId.empty()) {
            continue;
        }
        if (result.found) {
            ConvertStrToUint32(result.borrowPortId, numaInfo.portId);
        }
        for (const auto& cpuInfo : nodeInfo.cpuInfos) {
            if (cpuInfo.second.socketId == numaInfo.socketId) {
                ConvertStrToUint32(cpuInfo.second.chipId, numaInfo.chipId);
                break;
            }
        }
    }
}

uint32_t GetCnaInfoForNumaBorrow(const std::string& exportNodeId, const std::string& importNodeId,
                                 UbseMemNumaBorrowImportObj& importObj)
{
    UBSE_LOG_INFO << "importObj.req.linkInfo.lenderPort=" << importObj.req.linkInfo.lenderPort;
    UbseNodeMemCnaInfoInput cnaInput;
    cnaInput.exportNodeId = exportNodeId;
    cnaInput.borrowNodeId = importNodeId;
    if (!importObj.algoResult.exportNumaInfos.empty()) {
        cnaInput.exportSocketId = std::to_string(importObj.algoResult.exportNumaInfos[0].socketId);
    }
    if ((cnaInput.exportNodeId.empty() || cnaInput.borrowNodeId == cnaInput.exportNodeId) &&
        !importObj.algoResult.exportNumaInfos.empty()) {
        cnaInput.exportNodeId = importObj.algoResult.exportNumaInfos[0].nodeId;
    }
    UBSE_LOG_INFO << "exportSocketId=" << cnaInput.exportSocketId;
    UbseNodeMemCnaInfoOutput cnaOutput;
    if (auto ret = UbseNodeMemGetTopologyCnaInfo(cnaInput, cnaOutput); ret != 0) {
        UBSE_LOG_ERROR << "Failed to get cna, borrowNodeId=" << cnaInput.borrowNodeId
                       << ", exportNodeId=" << cnaInput.exportNodeId;
        return ret;
    }
    uint32_t dcna = cnaOutput.exportNodeCna;
    std::string borrowPortId = cnaOutput.portGroupId;
    if (auto result = SelectPortCna(cnaInput.exportNodeId, cnaOutput.exportSocketId, cnaInput.borrowNodeId,
                                    static_cast<uint32_t>(importObj.req.linkInfo.lenderPort));
        result.found) {
        dcna = result.dcna;
        borrowPortId = result.borrowPortId;
    }
    for (auto& newObmmDesc : importObj.exportObmmInfo) {
        // mar_id为port_id除4。port 0-3对应mar_id 0，port 4-7对应mar_id 1, port 8对应mar_id 2
        newObmmDesc.desc.scna = cnaOutput.borrowNodeCna;
        newObmmDesc.desc.dcna = dcna;
        newObmmDesc.desc.marId = GetMarId(borrowPortId);
        uint32_t srcSocketId = 0;
        auto ret = ConvertStrToUint32(cnaOutput.borrowSocketId, srcSocketId);
        ret |= UbseNodeController::GetInstance().GetEid(cnaInput.borrowNodeId, srcSocketId, newObmmDesc.desc.seid);
        if ((newObmmDesc.desc.scna == INVALID_VALUE16) || (newObmmDesc.desc.dcna == INVALID_VALUE16) ||
            ret != UBSE_OK) {
            UBSE_LOG_ERROR << "failed to get cna or mar id or eid, scna=" << newObmmDesc.desc.scna
                           << ", marId=" << newObmmDesc.desc.marId << ", dcna=" << newObmmDesc.desc.dcna
                           << ", seid=" << newObmmDesc.desc.seid << ", deid=" << newObmmDesc.desc.deid;
            return UBSE_ERROR;
        }
    }
    UBSE_LOG_INFO << "Obmm portGroupId=" << borrowPortId << ", scna=" << cnaOutput.borrowNodeCna << ", dcna=" << dcna;
    return UBSE_OK;
}

uint32_t GetCnaInfoWhenImport(const std::string& exportNodeId, const std::string& importNodeId,
                              UbseMemBorrowImportBaseObj& importObj, const bool isFdOrAddr, uint32_t specifiedPortId)
{
    UbseNodeMemCnaInfoInput cnaInput;
    cnaInput.exportNodeId = exportNodeId;
    cnaInput.borrowNodeId = importNodeId;
    if (!importObj.algoResult.exportNumaInfos.empty()) {
        cnaInput.exportSocketId = std::to_string(importObj.algoResult.exportNumaInfos[0].socketId);
    }
    if ((cnaInput.exportNodeId.empty() || cnaInput.borrowNodeId == cnaInput.exportNodeId) &&
        !importObj.algoResult.exportNumaInfos.empty()) {
        cnaInput.exportNodeId = importObj.algoResult.exportNumaInfos[0].nodeId;
    }
    UBSE_LOG_INFO << "exportSocketId=" << cnaInput.exportSocketId;
    UbseNodeMemCnaInfoOutput cnaOutput;
    if (auto ret = UbseNodeMemGetTopologyCnaInfo(cnaInput, cnaOutput); ret != 0) {
        UBSE_LOG_ERROR << "Failed to get cna, borrowNodeId=" << cnaInput.borrowNodeId
                       << ", exportNodeId=" << cnaInput.exportNodeId;
        return UBSE_ERROR;
    }
    FillAttachSocketId(importObj, isFdOrAddr, cnaOutput);
    uint32_t dcna = cnaOutput.exportNodeCna;
    std::string borrowPortId = cnaOutput.portGroupId;
    if (auto result =
            SelectPortCna(cnaInput.exportNodeId, cnaInput.exportSocketId, cnaInput.borrowNodeId, specifiedPortId);
        result.found) {
        dcna = result.dcna;
        borrowPortId = result.borrowPortId;
    } else if (specifiedPortId != UINT32_MAX) {
        UBSE_LOG_ERROR << "Specified port " << specifiedPortId << " not found";
        return UBSE_ERROR;
    }
    for (auto& newObmmDesc : importObj.exportObmmInfo) {
        // mar_id为port_id除4。port 0-3对应mar_id 0，port 4-7对应mar_id 1, port 8对应mar_id 2
        newObmmDesc.desc.scna = cnaOutput.borrowNodeCna;
        newObmmDesc.desc.dcna = dcna;
        newObmmDesc.desc.marId = GetMarId(borrowPortId);
        uint32_t srcSocketId = 0;
        auto ret = ConvertStrToUint32(cnaOutput.borrowSocketId, srcSocketId);
        ret |= UbseNodeController::GetInstance().GetEid(cnaInput.borrowNodeId, srcSocketId, newObmmDesc.desc.seid);
        if ((newObmmDesc.desc.scna == INVALID_VALUE16) || (newObmmDesc.desc.dcna == INVALID_VALUE16) ||
            ret != UBSE_OK) {
            UBSE_LOG_ERROR << "failed to get cna or mar id or eid, scna=" << newObmmDesc.desc.scna
                           << ", marId=" << newObmmDesc.desc.marId << ", dcna=" << newObmmDesc.desc.dcna
                           << ", seid=" << newObmmDesc.desc.seid << ", deid=" << newObmmDesc.desc.deid;
            return UBSE_ERROR;
        }
    }
    UBSE_LOG_INFO << "Obmm portGroupId=" << borrowPortId << ", scna=" << cnaOutput.borrowNodeCna << ", dcna=" << dcna;
    return UBSE_OK;
}

template <typename T>
void ProcessNodeMapWithHandler(UbseMemDebtLedger& ledger, const std::string& nodeId, std::function<void(T&)> handler)
{
    auto nodeMap = ledger.GetDebtMap<T>().FindNodeMap(nodeId);
    if (!nodeMap) {
        return;
    }
    auto allResources = nodeMap->GetAll();
    for (const auto& [name, _] : allResources) {
        nodeMap->Modify(name, handler);
    }
}

void UpdateSchedulerCache(const std::string& nodeId)
{
    UbseMemNodeObjChangeHandler(nodeController::UbseNodeController::GetInstance().GetCurNode());
    auto& ledger = UbseMemDebtLedger::GetInstance();

    ProcessNodeMapWithHandler<UbseMemFdBorrowExportObj>(ledger, nodeId, UbseMemFdExportObjStateChangeHandler);
    ProcessNodeMapWithHandler<UbseMemFdBorrowImportObj>(ledger, nodeId, UbseMemFdImportObjStateChangeHandler);
    ProcessNodeMapWithHandler<UbseMemNumaBorrowExportObj>(ledger, nodeId, UbseMemNumaExportObjStateChangeHandler);
    ProcessNodeMapWithHandler<UbseMemNumaBorrowImportObj>(ledger, nodeId, UbseMemNumaImportObjStateChangeHandler);
    ProcessNodeMapWithHandler<UbseMemShareBorrowExportObj>(ledger, nodeId, UbseMemShmExportObjStateChangeHandler);
    ProcessNodeMapWithHandler<UbseMemShareBorrowImportObj>(ledger, nodeId, UbseMemShmImportObjStateChangeHandler);
    ProcessNodeMapWithHandler<UbseMemAddrBorrowExportObj>(ledger, nodeId, UbseMemAddrExportObjStateChangeHandler);
    ProcessNodeMapWithHandler<UbseMemAddrBorrowImportObj>(ledger, nodeId, UbseMemAddrImportObjStateChangeHandler);
}

void ClearNodeMap()
{
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "elc module not load.";
        return;
    }
    if (module->IsLeader()) {
        Node node{};
        auto ret = module->GetCurrentNode(node);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Get current node failed";
            return;
        }
        UBSE_LOG_INFO << "current node is master.";
        UpdateSchedulerCache(node.id);
        return;
    }
    auto curNodeId = UbseNodeController::GetInstance().GetCurrentNodeId();
    UbseMemDebtLedger::GetInstance().ClearOtherNodeMaps(curNodeId);
    ubse::mem::scheduler::ClearCacheValue();
}
} // namespace ubse::mem::controller
