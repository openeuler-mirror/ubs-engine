/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/
#include "ubse_mem_controller_api.h"

#include "api/ubse_mem_controller_api_common.h"
#include "message/node_mem_debtInfo_query_req_simpo.h"
#include "message/node_mem_debt_info_simpo.h"
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
    RegUbseMemControllerHandler();
    UbseMemControllerMasterOnlineHandler::Initial();
    ret = usbe::mem::api::UbseMemApi::Register();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register UbseMem IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
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

void LoadObjState(NodeMemDebtInfo &nodeMemDebtInfo)
{
    for (const auto &item : nodeMemDebtInfo.fdImportObjMap) {
        nodeMemDebtInfo.fdImportObjMap[item.first].status.state = UBSE_MEM_IMPORT_SUCCESS;
    }
    for (const auto &item : nodeMemDebtInfo.fdExportObjMap) {
        nodeMemDebtInfo.fdExportObjMap[item.first].status.state = UBSE_MEM_EXPORT_SUCCESS;
    }
    for (const auto &item : nodeMemDebtInfo.numaExportObjMap) {
        nodeMemDebtInfo.numaExportObjMap[item.first].status.state = UBSE_MEM_EXPORT_SUCCESS;
    }
    for (const auto &item : nodeMemDebtInfo.numaImportObjMap) {
        nodeMemDebtInfo.numaImportObjMap[item.first].status.state = UBSE_MEM_IMPORT_SUCCESS;
    }
    for (const auto &item : nodeMemDebtInfo.shareImportObjMap) {
        nodeMemDebtInfo.shareImportObjMap[item.first].status.state = UBSE_MEM_IMPORT_SUCCESS;
    }
    for (const auto &item : nodeMemDebtInfo.shareExportObjMap) {
        nodeMemDebtInfo.shareExportObjMap[item.first].status.state = UBSE_MEM_EXPORT_SUCCESS;
    }
    for (const auto &item : nodeMemDebtInfo.addrExportObjMap) {
        nodeMemDebtInfo.addrExportObjMap[item.first].status.state = UBSE_MEM_EXPORT_SUCCESS;
    }
    for (const auto &item : nodeMemDebtInfo.addrImportObjMap) {
        nodeMemDebtInfo.addrImportObjMap[item.first].status.state = UBSE_MEM_IMPORT_SUCCESS;
    }
}

uint32_t LoadLocalAllObjs(const ubse::nodeController::UbseNodeInfo &node)
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
    for (auto &exportObj : nodeMemDebtInfo.shareExportObjMap) {
        for (auto &nodeInfo : exportObj.second.req.shmRegion.nodelist) {
            nodeInfo.nodeId = std::to_string(nodeInfo.index + 1);
        }
    }
    for (auto &importObj : nodeMemDebtInfo.shareImportObjMap) {
        for (auto &nodeInfo : importObj.second.req.shmRegion.nodelist) {
            nodeInfo.nodeId = std::to_string(nodeInfo.index + 1);
        }
    }
    LoadObjState(nodeMemDebtInfo);
    UbseMemDebtLedger::GetInstance().LoadFromNodeMemDebtInfo(node.nodeId, nodeMemDebtInfo);

    return UBSE_OK;
}

uint16_t GetMarId(const std::string &portGroupIdStr)
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

void FillAttachSocketId(UbseMemBorrowImportBaseObj &importObj, const bool isFdOrAddr,
                        const UbseNodeMemCnaInfoOutput &cnaOutput)
{
    try {
        importObj.algoResult.attachSocketId = std::stol(cnaOutput.borrowSocketId);
    } catch (const std::invalid_argument &e) {
        importObj.algoResult.attachSocketId = 0;
        UBSE_LOG_ERROR << "Invalid argument: " << e.what();
    } catch (const std::out_of_range &e) {
        importObj.algoResult.attachSocketId = 0;
        UBSE_LOG_ERROR << "Out of range: " << e.what();
    }
}

void GetDcnaWhenSpecifylink(const UbseNodeMemCnaInfoInput &cnaInput, const UbseNodeMemCnaInfoOutput &cnaOutput,
                            uint32_t &dcna, const UbseMemNumaBorrowImportObj &importObj, std::string &borrowPortId)
{
    if (importObj.req.linkInfo.lenderPort == -1) {
        return;
    }
    if (!IsSameSocketMultiPortTopo()) {
        return;
    }
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(cnaInput.exportNodeId);
    uint32_t socketId{};
    try {
        socketId = std::stoi(cnaOutput.exportSocketId);
    } catch (...) {
        UBSE_LOG_INFO << "exportSocketId is " << cnaOutput.exportSocketId;
    }
    UbseCpuLocation location{};
    std::string portCna{};
    for (const auto &cpuInfo : nodeInfo.cpuInfos) {
        if (cpuInfo.second.socketId == socketId) {
            location = cpuInfo.first;
            break;
        }
    }
    auto cpu = nodeInfo.cpuInfos.find(location);
    UBSE_LOG_INFO << "location nodeId is " << location.nodeId << "chipId is " << location.chipId;
    if (cpu == nodeInfo.cpuInfos.end()) {
        UBSE_LOG_WARN << "location " << location.nodeId << " - " << location.chipId << " is not found in cpuInfo";
        return;
    }
    for (const auto &portInfo : cpu->second.portInfos) {
        if (portInfo.second.portId == std::to_string(importObj.req.linkInfo.lenderPort)) {
            UBSE_LOG_INFO << "portCna is " << portInfo.second.portCna;
            dcna = portInfo.second.portCna;
            borrowPortId = portInfo.second.remotePortId;
            UBSE_LOG_INFO << "location nodeId is " << location.nodeId << ", chipId is " << location.chipId
                          << ", portId is " << portInfo.second.portId << ", dcna is " << dcna << ", borrowPort is "
                          << borrowPortId;
        }
    }
}

uint32_t GetCnaInfoForNumaBorrow(const std::string &exportNodeId, const std::string &importNodeId,
                                 UbseMemNumaBorrowImportObj &importObj)
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
    std::string borrowPortId{};
    GetDcnaWhenSpecifylink(cnaInput, cnaOutput, dcna, importObj, borrowPortId);
    for (auto &newObmmDesc : importObj.exportObmmInfo) {
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

uint32_t GetCnaInfoWhenImport(const std::string &exportNodeId, const std::string &importNodeId,
                              UbseMemBorrowImportBaseObj &importObj, const bool isFdOrAddr)
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
    for (auto &newObmmDesc : importObj.exportObmmInfo) {
        // mar_id为port_id除4。port 0-3对应mar_id 0，port 4-7对应mar_id 1, port 8对应mar_id 2
        newObmmDesc.desc.scna = cnaOutput.borrowNodeCna;
        newObmmDesc.desc.dcna = cnaOutput.exportNodeCna;
        newObmmDesc.desc.marId = GetMarId(cnaOutput.portGroupId);
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
    UBSE_LOG_INFO << "Obmm portGroupId=" << cnaOutput.portGroupId << ", scna=" << cnaOutput.borrowNodeCna
                  << ", dcna=" << cnaOutput.exportNodeCna;
    return UBSE_OK;
}

template <typename T>
void ProcessNodeMapWithHandler(UbseMemDebtLedger &ledger, const std::string &nodeId, std::function<void(T &)> handler)
{
    auto nodeMap = ledger.GetDebtMap<T>().FindNodeMap(nodeId);
    if (!nodeMap) {
        return;
    }
    auto allResources = nodeMap->GetAll();
    for (const auto &[name, _] : allResources) {
        nodeMap->Modify(name, handler);
    }
}

void UpdateSchedulerCache(const std::string &nodeId)
{
    UbseMemNodeObjChangeHandler(nodeController::UbseNodeController::GetInstance().GetCurNode());
    auto &ledger = UbseMemDebtLedger::GetInstance();

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