/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#include "ubse_mem_controller_msg.h"

#include <ubse_conf_module.h>

#include "ubse_com_base.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_mem_agent_task_manager.h"
#include "ubse_mem_controller.h"
#include "ubse_mem_controller_ledger.h"
#include "ubse_mem_controller_ledger_filter.h"
#include "ubse_mem_debt_info.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_mem_opt_req_simpo.h"
#include "ubse_mem_opt_result_simpo.h"
#include "ubse_mem_util.h"
#include "ubse_os_util.h"
#include "message/node_mem_debt_info_simpo.h"
#include "message/ubse_mem_Ledger_resp_serial.h"
#include "message/ubse_mem_addr_borrow_exportobj_simpo.h"
#include "message/ubse_mem_addr_borrow_importobj_simpo.h"
#include "message/ubse_mem_controller_def_simpo.h"
#include "message/ubse_mem_fd_borrow_exportobj_simpo.h"
#include "message/ubse_mem_fd_borrow_importobj_simpo.h"
#include "message/ubse_mem_numa_borrow_exportobj_simpo.h"
#include "message/ubse_mem_numa_borrow_importobj_simpo.h"
#include "message/ubse_mem_remote_numa_status.h"
#include "message/ubse_mem_share_borrow_exportobj_simpo.h"
#include "message/ubse_mem_share_borrow_importobj_simpo.h"
namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::mem::util;
using namespace ubse::mem::controller::message;
using namespace ubse::election;
using namespace ubse::serial;
using namespace ubse::context;
using namespace ubse::mem::controller::debt;

void RegRespCtrlHandlers()
{
    const ubse::com::UbseComEndpoint collectEndpoint = {static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
                                                        static_cast<uint32_t>(UbseMemRespCtrlOpCode::UBSE_MEM_LEDGER)};
    const ubse::com::UbseComEndpoint queryFdImportEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
        static_cast<uint32_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_IMPORT)};
    const ubse::com::UbseComEndpoint queryNumaImportEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
        static_cast<uint32_t>(UbseMemRespCtrlOpCode::UBSE_MEM_NUMA_IMPORT)};
    const ubse::com::UbseComEndpoint queryAddrImportEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
        static_cast<uint32_t>(UbseMemRespCtrlOpCode::UBSE_MEM_ADDR_IMPORT)};
    const ubse::com::UbseComEndpoint preOnLineEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
        static_cast<uint32_t>(UbseMemRespCtrlOpCode::UBSE_MEM_PRE_ONLINE_REQ)};
    const ubse::com::UbseComEndpoint preOnLineReplyEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
        static_cast<uint32_t>(UbseMemRespCtrlOpCode::UBSE_MEM_PRE_ONLINE_RESP)};
    const ubse::com::UbseComEndpoint invalidateImportDebtEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
        static_cast<uint32_t>(UbseMemRespCtrlOpCode::UBSE_MEM_INVALIDATE_SINGLE_IMPORT_DEBT)};

    UbseRegRpcService(collectEndpoint, CollectLedgeHandler);
    UbseRegRpcService(queryFdImportEndpoint, QueryFdImportObjHandler);
    UbseRegRpcService(queryNumaImportEndpoint, QueryNumaImportObjHandler);
    UbseRegRpcService(queryAddrImportEndpoint, QueryAddrImportObjHandler);
    UbseRegRpcService(preOnLineEndpoint, PreOnLineHandler);
    UbseRegRpcService(preOnLineReplyEndpoint, PreOnLineReplyHandler);
    UbseRegRpcService(invalidateImportDebtEndpoint, SendInvalidateSingleImportDebtRpcHandler);
}

void RegQueryHandlers()
{
    const ubse::com::UbseComEndpoint getNumaInfoByPidEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
        static_cast<uint32_t>(UbseMemQueryOpCode::UBSE_MEM_GET_NUMAINFO_BY_PID)};
    const ubse::com::UbseComEndpoint getFdExportEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
        static_cast<uint32_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_FD_EXPORT)};
    const ubse::com::UbseComEndpoint getFdImportEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
        static_cast<uint32_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_FD_IMPORT)};
    const ubse::com::UbseComEndpoint getNumaExportEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
        static_cast<uint32_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_NUMA_EXPORT)};
    const ubse::com::UbseComEndpoint getNumaImportEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
        static_cast<uint32_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_NUMA_IMPORT)};
    const ubse::com::UbseComEndpoint getAddrExportEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
        static_cast<uint32_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_ADDR_EXPORT)};
    const ubse::com::UbseComEndpoint getAddrImportEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
        static_cast<uint32_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_ADDR_IMPORT)};
    const ubse::com::UbseComEndpoint getShareExportEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
        static_cast<uint32_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_SHARE_EXPORT)};
    const ubse::com::UbseComEndpoint getShareImportEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
        static_cast<uint32_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_SHARE_IMPORT)};
    const ubse::com::UbseComEndpoint getNumaStatusEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
        static_cast<uint32_t>(UbseMemQueryOpCode::UBSE_MEM_REMOTE_NUMA_STATUS)};

    UbseRegRpcService(getNumaInfoByPidEndpoint, GetNumaInfoByPidHandler);
    UbseRegRpcService(getFdExportEndpoint, QueryFdExportHandler);
    UbseRegRpcService(getFdImportEndpoint, QueryFdImportHandler);
    UbseRegRpcService(getNumaExportEndpoint, QueryNumaExportHandler);
    UbseRegRpcService(getNumaImportEndpoint, QueryNumaImportHandler);
    UbseRegRpcService(getAddrExportEndpoint, QueryAddrExportHandler);
    UbseRegRpcService(getAddrImportEndpoint, QueryAddrImportHandler);
    UbseRegRpcService(getShareExportEndpoint, QueryShareExportHandler);
    UbseRegRpcService(getShareImportEndpoint, QueryShareImportHandler);
    UbseRegRpcService(getNumaStatusEndpoint, NotifyRemoteNumaStatusHandler);
}

void RegUbseMemControllerHandler()
{
    RegRespCtrlHandlers();
    RegQueryHandlers();
}

bool IsUrma()
{
    auto ubseConfModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::config::UbseConfModule>();
    if (ubseConfModule == nullptr) {
        UBSE_LOG_ERROR << "Get config info failed";
        return true;
    }
    std::string ipList;
    auto ret = ubseConfModule->GetConf<std::string>("ubse.rpc", "cluster.ipList", ipList);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "Unable to get ub config, use default urma, " << FormatRetCode(ret);
        return true;
    }
    return false;
}

UbseResult CollectLedge(const std::string& nodeId, NodeMemDebtInfo& info)
{
    NodeMemDebtInfoSimpo simpo{};
    simpo.Serialize();

    SendParam sendParam{nodeId, static_cast<uint16_t>(ubse::com::UbseModuleCode::UBSE_MEM_RESP),
                        static_cast<uint32_t>(UbseMemRespCtrlOpCode::UBSE_MEM_LEDGER)};
    auto ubseComModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Get UbseComModule failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseResult collectRet = UBSE_OK;
    size_t size = simpo.SerializedDataSize();
    UbseComBaseBufferMessagePtr request = new (std::nothrow) UbseComBaseBufferMessage(simpo.SerializedData(), size);
    UbseComBaseBufferMessagePtr response = new (std::nothrow) UbseComBaseBufferMessage();
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Fail to new response buffer";
        return UBSE_ERROR_NULLPTR;
    }
    response->SetIsNeedFreeData(true);
    UbseResult ret = UBSE_OK;
    ret = ubseComModule->RpcSend(sendParam, request, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send collect nodeId=" << nodeId << " msg failed, " << FormatRetCode(ret);
        return ret;
    }
    if (response->GetData() == nullptr || response->GetDataLen() == 0) {
        UBSE_LOG_ERROR << "collect nodeId=" << nodeId << " resp null";
        return UBSE_ERROR_NULLPTR;
    }
    UbseMemLedgerRespSerial resultSimpo{response->GetData(), static_cast<uint32_t>(response->GetDataLen())};
    collectRet = resultSimpo.Deserialize();
    if (collectRet != UBSE_OK) {
        UBSE_LOG_ERROR << "collect nodeId=" << nodeId << " deserialize failed, " << FormatRetCode(collectRet);
    } else {
        LedgerResp resp = resultSimpo.GetLedgerResp();
        if (resp.ret != UBSE_OK) {
            UBSE_LOG_ERROR << "collect nodeId=" << nodeId << " ledger failed, " << FormatRetCode(resp.ret);
            return resp.ret;
        }
        if (resp.debtInfoMap.find(nodeId) != resp.debtInfoMap.end()) {
            info = resp.debtInfoMap[nodeId];
        }
    }
    return collectRet;
}

UbseResult CreateRespBuffer(const UbseBaseMessage& simpo, UbseByteBuffer& resp)
{
    size_t size = simpo.SerializedDataSize();
    auto ptr = new (std::nothrow) uint8_t[size];
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "fail to alloc resp buffer";
        return UBSE_ERROR_NULLPTR;
    }
    resp = {ptr, size, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    auto ret = memcpy_s(resp.data, resp.len, simpo.SerializedData(), size);
    if (ret != EOK) {
        UBSE_LOG_ERROR << "memcpy_s failed, ErrorCode=" << ret;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult CollectLedgeHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UbseResult ret = UBSE_OK;
    std::string nodeId = GetCurNodeId();
    UbseMemLedgerRespSerial simpo{};
    LedgerResp ledgerResp{UBSE_OK, {}};
    size_t size = 0;
    auto node = UbseNodeController::GetInstance().GetCurNode();
    if (node.nodeId.empty()) {
        UBSE_LOG_ERROR << "current node not collect.";
        ledgerResp.ret = UBSE_ERROR;
        simpo.SetLedgerResp(ledgerResp);
        ret = simpo.Serialize();
        if (ret != UBSE_OK) {
            UBSE_LOG_INFO << "mem debt info deserialize failed, " << FormatRetCode(ret);
            resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                        SafeDeleteArray(p, size);
                    }};
            return ret;
        }
        return CreateRespBuffer(simpo, resp);
    }
    if (node.localState != UbseNodeLocalState::UBSE_NODE_READY) {
        UBSE_LOG_ERROR << "current node not ready.";
        ledgerResp.ret = UBSE_ERROR;
        simpo.SetLedgerResp(ledgerResp);
        ret = simpo.Serialize();
        if (ret != UBSE_OK) {
            UBSE_LOG_INFO << "mem debt info deserialize failed, " << FormatRetCode(ret);
            resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                        SafeDeleteArray(p, size);
                    }};
            return ret;
        }
        return CreateRespBuffer(simpo, resp);
    }
    NodeMemDebtInfo info{};
    info = GetNoDeletedNodeMemDebtInfoById(nodeId);
    ledgerResp.debtInfoMap[nodeId] = std::move(info);
    simpo.SetLedgerResp(ledgerResp);
    ret = simpo.Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "mem debt info deserialize failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return ret;
    }
    return CreateRespBuffer(simpo, resp);
}

UbseResult QueryFdImportObj(const std::string& nodeId, const std::string& name, UbseMemFdBorrowImportObj& fdInfo,
                            const std::unordered_map<std::string, NodeMemDebtInfo>& allDebtInfoMap)
{
    if (allDebtInfoMap.find(nodeId) != allDebtInfoMap.end()) {
        if (allDebtInfoMap.at(nodeId).fdImportObjMap.find(name) != allDebtInfoMap.at(nodeId).fdImportObjMap.end()) {
            fdInfo = allDebtInfoMap.at(nodeId).fdImportObjMap.at(name);
            UBSE_LOG_INFO << "query fd import, import nodeId=" << nodeId << " import name=" << name
                          << " success, result=" << fdInfo.req.name << ", status=" << TransState(fdInfo.status.state);
            return UBSE_OK;
        }
        fdInfo = UbseMemFdBorrowImportObj{};
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

void FindFdImportObj(const std::string& nodeId, const std::string& fdImportName, UbseMemFdBorrowImportObj& obj)
{
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    if (nodeMemDebtInfoMap.find(nodeId) != nodeMemDebtInfoMap.end()) {
        if (nodeMemDebtInfoMap[nodeId].fdImportObjMap.find(fdImportName) !=
            nodeMemDebtInfoMap[nodeId].fdImportObjMap.end()) {
            obj = nodeMemDebtInfoMap[nodeId].fdImportObjMap[fdImportName];
            UBSE_LOG_INFO << "query fd import, import nodeId=" << nodeId << " import name=" << fdImportName
                          << " success, result=" << obj.req.name << ", status=" << TransState(obj.status.state);
        }
    }
}

UbseResult QueryFdImportObjHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOG_INFO << "query fd import start";
    std::string nodeId = GetCurNodeId();
    auto node = UbseNodeController::GetInstance().GetCurNode();
    if (node.nodeId.empty()) {
        UBSE_LOG_ERROR << "current node not collect.";
        return UBSE_ERROR;
    }
    if (node.localState != UbseNodeLocalState::UBSE_NODE_READY) {
        UBSE_LOG_ERROR << "current node not ready.";
        return UBSE_ERROR;
    }
    UbseMemFdBorrowImportobjSimpo simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    size_t size = 0;
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "mem query fd import serialize failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return ret;
    }
    std::string fdImportName = simpo.GetImportObj().req.name;
    UBSE_LOG_INFO << "query fd import, import nodeId=" << nodeId << " import name=" << fdImportName;
    UbseMemFdBorrowImportObj obj{};
    FindFdImportObj(nodeId, fdImportName, obj);
    UbseMemFdBorrowImportobjSimpo resultSimpo{};
    resultSimpo.SetUbseMemFdBorrowImportobj(obj);
    ret = resultSimpo.Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "mem fd import deserialize failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return ret;
    }
    return CreateRespBuffer(resultSimpo, resp);
}

UbseResult QueryNumaImportObj(const std::string& nodeId, const std::string& name, UbseMemNumaBorrowImportObj& numaInfo,
                              const std::unordered_map<std::string, NodeMemDebtInfo>& allDebtInfoMap)
{
    if (allDebtInfoMap.find(nodeId) != allDebtInfoMap.end()) {
        if (allDebtInfoMap.at(nodeId).numaImportObjMap.find(name) != allDebtInfoMap.at(nodeId).numaImportObjMap.end()) {
            numaInfo = allDebtInfoMap.at(nodeId).numaImportObjMap.at(name);
            UBSE_LOG_INFO << "query fd import, import nodeId=" << nodeId << " import name=" << name
                          << " success, result=" << numaInfo.req.name
                          << ", status=" << TransState(numaInfo.status.state);
            return UBSE_OK;
        }
        numaInfo = UbseMemNumaBorrowImportObj{};
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

void FindNumaImportObj(const std::string& nodeId, const std::string& numaImportName, UbseMemNumaBorrowImportObj& obj)
{
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    if (nodeMemDebtInfoMap.find(nodeId) != nodeMemDebtInfoMap.end()) {
        if (nodeMemDebtInfoMap[nodeId].numaImportObjMap.find(numaImportName) !=
            nodeMemDebtInfoMap[nodeId].numaImportObjMap.end()) {
            obj = nodeMemDebtInfoMap[nodeId].numaImportObjMap[numaImportName];
            UBSE_LOG_INFO << "query numa import, import nodeId=" << nodeId << " import name=" << numaImportName
                          << " success, result=" << obj.req.name << ", status=" << TransState(obj.status.state);
        }
    }
}
UbseResult QueryNumaImportObjHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOG_INFO << "query numa import start";
    std::string nodeId = GetCurNodeId();
    auto node = UbseNodeController::GetInstance().GetCurNode();
    if (node.nodeId.empty()) {
        UBSE_LOG_ERROR << "current node not collect.";
        return UBSE_ERROR;
    }
    if (node.localState != UbseNodeLocalState::UBSE_NODE_READY) {
        UBSE_LOG_ERROR << "current node not ready.";
        return UBSE_ERROR;
    }
    UbseMemNumaBorrowImportobjSimpo simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    size_t size = 0;
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "mem query numa import serialize failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return ret;
    }
    std::string numaImportName = simpo.GetImportObj().req.name;
    UbseMemNumaBorrowImportObj obj{};
    UBSE_LOG_INFO << "query numa import, import nodeId=" << nodeId << " import name=" << numaImportName;
    FindNumaImportObj(nodeId, numaImportName, obj);
    UbseMemNumaBorrowImportobjSimpoPtr resultSimpo = new (std::nothrow) UbseMemNumaBorrowImportobjSimpo();
    if (resultSimpo == nullptr) {
        UBSE_LOG_ERROR << "new simpo failed.";
        return UBSE_ERROR_NULLPTR;
    }
    resultSimpo->SetUbseMemNumaBorrowImportobj(obj);
    ret = resultSimpo->Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "mem numa import deserialize failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return ret;
    }
    return CreateRespBuffer(*resultSimpo.Get(), resp);
}

UbseResult QueryAddrImportObj(const std::string& nodeId, const std::string& name, UbseMemAddrBorrowImportObj& addrInfo,
                              const std::unordered_map<std::string, NodeMemDebtInfo>& allDebtInfoMap)
{
    if (allDebtInfoMap.find(nodeId) != allDebtInfoMap.end()) {
        if (allDebtInfoMap.at(nodeId).addrImportObjMap.find(name) != allDebtInfoMap.at(nodeId).addrImportObjMap.end()) {
            addrInfo = allDebtInfoMap.at(nodeId).addrImportObjMap.at(name);
            UBSE_LOG_INFO << "query fd import, import nodeId=" << nodeId << " import name=" << name
                          << " success, result=" << addrInfo.req.name
                          << ", status=" << TransState(addrInfo.status.state);
            return UBSE_OK;
        }
        addrInfo = UbseMemAddrBorrowImportObj{};
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

void FindAddrImportObj(const std::string& nodeId, const std::string& addrImportName, UbseMemAddrBorrowImportObj& obj)
{
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    if (nodeMemDebtInfoMap.find(nodeId) != nodeMemDebtInfoMap.end()) {
        if (nodeMemDebtInfoMap[nodeId].addrImportObjMap.find(addrImportName) !=
            nodeMemDebtInfoMap[nodeId].addrImportObjMap.end()) {
            obj = nodeMemDebtInfoMap[nodeId].addrImportObjMap[addrImportName];
            UBSE_LOG_INFO << "addr import nodeId=" << nodeId
                          << " import name=" + addrImportName + " success, result=" << obj.req.name
                          << ", status=" << TransState(obj.status.state);
        }
    }
}

UbseResult QueryAddrImportObjHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOG_INFO << "query addr import start";
    std::string nodeId = GetCurNodeId();
    auto node = UbseNodeController::GetInstance().GetCurNode();
    if (node.nodeId.empty()) {
        UBSE_LOG_ERROR << "current node not collect.";
        return UBSE_ERROR;
    }
    if (node.localState != UbseNodeLocalState::UBSE_NODE_READY) {
        UBSE_LOG_ERROR << "current node not ready.";
        return UBSE_ERROR;
    }
    UbseMemAddrBorrowImportobjSimpo simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    size_t size = 0;
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "mem query addr import serialize failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return ret;
    }
    std::string addrImportName = simpo.GetImportObj().req.name;
    UbseMemAddrBorrowImportObj obj{};
    FindAddrImportObj(nodeId, addrImportName, obj);
    UbseMemAddrBorrowImportobjSimpoPtr resultSimpo = new (std::nothrow) UbseMemAddrBorrowImportobjSimpo;
    if (resultSimpo == nullptr) {
        UBSE_LOG_ERROR << "new simpo failed.";
        return UBSE_ERROR_NULLPTR;
    }
    resultSimpo->SetUbseMemAddrBorrowImportobj(obj);
    ret = resultSimpo->Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "mem addr import deserialize failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return ret;
    }
    return CreateRespBuffer(*resultSimpo.Get(), resp);
}

UbseResult PreOnLineRequest(const std::string& nodeId, PreOnLineReq req)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::UBSE_MEM_RESP),
        .serviceId = static_cast<uint32_t>(UbseMemRespCtrlOpCode::UBSE_MEM_PRE_ONLINE_REQ),
        .address = nodeId,
    };
    uint8_t* buffer;
    size_t size;
    auto ret = SerializePreOnLine(req, buffer, size);
    if (ret != UBSE_OK) {
        return ret;
    }
    UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t* p) noexcept {
                                 SafeDeleteArray(p, size);
                             }};
    return UbseRpcSend(endpoint, reqBuffer, nullptr,
                       [](void* ctx, const UbseByteBuffer& respData, uint32_t resCode) -> void {});
}

UbseResult PreOnLineHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    PreOnLineReq preOnLineReq{};
    PreOnLineResp preOnLineResp{};
    preOnLineResp.ret = UBSE_OK;
    uint8_t* buffer;
    size_t size = 0;
    UbseResult ret = DeSerializePreOnLine(preOnLineReq, req.data, req.len);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "deSerialize pre online cna failed, " << FormatRetCode(ret);
        preOnLineResp.ret = ret;
        SerializePreOnlineResp(preOnLineResp, buffer, size);
        resp = {buffer, size, [size](uint8_t* p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return UBSE_OK;
    }
    OperatePreOnLine(preOnLineReq);
    SerializePreOnlineResp(preOnLineResp, buffer, size);
    resp = {buffer, size, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    return UBSE_OK;
}

UbseResult PreOnLineReply(PreOnLineResp resp)
{
    UbseRoleInfo masterInfo{};
    auto ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get master nodeId failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_ERROR << "reply to master=" << masterInfo.nodeId;
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::UBSE_MEM_RESP),
        .serviceId = static_cast<uint32_t>(UbseMemRespCtrlOpCode::UBSE_MEM_PRE_ONLINE_RESP),
        .address = masterInfo.nodeId,
    };
    uint8_t* buffer;
    size_t size;
    ret = SerializePreOnlineResp(resp, buffer, size);
    if (ret != UBSE_OK) {
        return ret;
    }
    UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t* p) noexcept {
                                 SafeDeleteArray(p, size);
                             }};
    return UbseRpcSend(endpoint, reqBuffer, nullptr,
                       [](void* ctx, const UbseByteBuffer& respData, uint32_t resCode) -> void {});
}

UbseResult PreOnLineReplyHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    PreOnLineResp preOnLineReply{};
    PreOnLineResp preOnLineResp{};
    preOnLineResp.ret = UBSE_OK;
    uint8_t* buffer;
    size_t size = 0;
    UbseResult ret = DeSerializePreOnLineResp(preOnLineReply, req.data, req.len);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "deSerialize pre online reply failed, " << FormatRetCode(ret);
        preOnLineResp.ret = ret;
        SerializePreOnlineResp(preOnLineResp, buffer, size);
        resp = {buffer, size, [size](uint8_t* p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return UBSE_OK;
    }
    handlePreOnLineTask(preOnLineReply);
    SerializePreOnlineResp(preOnLineResp, buffer, size);
    resp = {buffer, size, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    return UBSE_OK;
}

UbseResult GetLocalNumaInfoByPid(const uint64_t& pid, uint32_t& numaId, uint32_t& socketId)
{
    auto ret = UbseOsUtil::GetNumaIdByPid(pid, numaId);
    if (ret != UBSE_OK) {
        return ret;
    }
    ubse::nodeController::UbseNodeInfo node = UbseNodeController::GetInstance().GetCurNode();
    ubse::nodeController::UbseNumaLocation location{node.nodeId, numaId};
    auto it = node.numaInfos.find(location);
    if (it == node.numaInfos.end()) {
        UBSE_LOG_ERROR << "can not find socketId by nodeId=" << node.nodeId << " , numaId=" << numaId;
        return UBSE_ERROR;
    }
    socketId = it->second.socketId;
    UBSE_LOG_INFO << "find numaId=" << numaId << ", socketId=" << socketId << " by nodeId=" << node.nodeId
                  << ", pid=" << pid;
    return UBSE_OK;
}

UbseResult GetNumaInfoByPidHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    uint64_t pid = -1;
    uint32_t numaId{};
    uint32_t socketId{};
    UbseDeSerialization input(req.data, req.len);
    input >> pid;
    if (!input.Check()) {
        UBSE_LOG_ERROR << "deserialize get numa info req failed.";
        return UBSE_ERROR;
    }
    auto ret = GetLocalNumaInfoByPid(pid, numaId, socketId);
    if (ret != UBSE_OK) {
        return ret;
    }
    UbseSerialization output;
    output << numaId << socketId;
    if (!output.Check()) {
        UBSE_LOG_ERROR << "serialize get numa info resp failed.";
        return UBSE_ERROR;
    }
    resp = UbseByteBuffer{
        .data = output.GetBuffer(true), .len = output.GetLength(), .freeFunc = [](uint8_t* data) -> void {
            SafeDeleteArray(data);
        }};
    return UBSE_OK;
}

void GetNumaInfoFromRemoteRespHandler(const UbseByteBuffer& respData, const uint32_t& resCode, uint32_t& numaId,
                                      uint32_t& socketId, UbseResult& getRet)
{
    if (resCode != UBSE_OK) {
        UBSE_LOG_ERROR << "get numa info failed, " << FormatRetCode(resCode);
        getRet = resCode;
        return;
    }
    if (respData.data == nullptr || respData.len == 0) {
        UBSE_LOG_ERROR << "get numa info resp null";
        getRet = UBSE_ERROR_NULLPTR;
        return;
    }
    serial::UbseDeSerialization input(respData.data, respData.len);
    input >> numaId >> socketId;
    if (!input.Check()) {
        UBSE_LOG_ERROR << "deserialize numa info failed";
        getRet = UBSE_ERROR;
    }
}

UbseResult GetNumaInfoFromAgent(const std::string& nodeId, const uint64_t& pid, uint32_t& numaId, uint32_t& socketId)
{
    ubse::nodeController::UbseNodeInfo node = UbseNodeController::GetInstance().GetCurNode();
    if (nodeId == node.nodeId) {
        return GetLocalNumaInfoByPid(pid, numaId, socketId);
    }
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::UBSE_MEM_QUERY),
        .serviceId = static_cast<uint32_t>(ubse::com::UbseMemQueryOpCode::UBSE_MEM_GET_NUMAINFO_BY_PID),
        .address = nodeId,
    };
    UbseResult getRet = UBSE_OK;
    serial::UbseSerialization output;
    output << pid;
    if (!output.Check()) {
        UBSE_LOG_ERROR << "seserialize numa info failed";
        getRet = UBSE_ERROR;
    }

    UbseByteBuffer reqBuffer{output.GetBuffer(true), output.GetLength(), [](uint8_t* p) noexcept {
                                 SafeDeleteArray(p);
                             }};
    auto ret =
        UbseRpcSend(endpoint, reqBuffer, nullptr,
                    [&numaId, &socketId, &getRet](void* ctx, const UbseByteBuffer& respData, uint32_t resCode) -> void {
                        GetNumaInfoFromRemoteRespHandler(respData, resCode, numaId, socketId, getRet);
                    });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send get numa info msg failed, " << FormatRetCode(ret);
        return ret;
    }
    return getRet;
}

UbseResult QueryFdExport(def::UbseMemDebtQueryRequest request, UbseMemFdBorrowExportObj& obj)
{
    const SendParam sendParam{request.exportNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
                              static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_FD_EXPORT)};

    UbseMemDebtQueryRequestSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemDebtQueryRequestSimpo();
    if (ubseRequestPtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemDebtQueryRequest(request);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemFdBorrowExportobjSimpo();
    if (ubseResponsePtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ubseComModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto retCode = ubseComModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "rpc sync send query fd export obj failed, " << FormatRetCode(retCode);
        return retCode;
    }
    auto exportPtr = UbseBaseMessage::DeConvert<UbseMemFdBorrowExportobjSimpo>(ubseResponsePtr);
    obj = exportPtr->GetUbseMemFdBorrowExportObj();
    return UBSE_OK;
}

UbseResult QueryFdExportHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UbseMemDebtQueryRequestSimpo simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    size_t size = 0;
    resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem query fd export deserialize failed, " << FormatRetCode(ret);
        return ret;
    }
    def::UbseMemDebtQueryRequest memReq = simpo.GetUbseMemDebtQueryRequest();
    UbseMemFdBorrowExportObj obj = UbseFdExportObjGet(memReq.exportNodeId, memReq.name, memReq.importNodeId, true);
    UbseMemFdBorrowExportobjSimpoPtr resultSimpo = new (std::nothrow) UbseMemFdBorrowExportobjSimpo;
    if (resultSimpo == nullptr) {
        UBSE_LOG_ERROR << "new simpo failed.";
        return UBSE_ERROR_NULLPTR;
    }
    resultSimpo->SetUbseMemFdBorrowExportobj(obj);
    ret = resultSimpo->Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem fd export deserialize failed, " << FormatRetCode(ret);
        return ret;
    }
    return CreateRespBuffer(*resultSimpo.Get(), resp);
}

UbseResult QueryNumaExport(def::UbseMemDebtQueryRequest request, UbseMemNumaBorrowExportObj& obj)
{
    const SendParam sendParam{request.exportNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
                              static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_NUMA_EXPORT)};

    UbseMemDebtQueryRequestSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemDebtQueryRequestSimpo();
    if (ubseRequestPtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemDebtQueryRequest(request);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemNumaBorrowExportobjSimpo();
    if (ubseResponsePtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ubseComModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto retCode = ubseComModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "rpc sync send query numa export obj failed, " << FormatRetCode(retCode);
        return retCode;
    }
    auto exportPtr = UbseBaseMessage::DeConvert<UbseMemNumaBorrowExportobjSimpo>(ubseResponsePtr);
    if (exportPtr == nullptr) {
        UBSE_LOG_ERROR << "exportPtr is null, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    obj = exportPtr->GetUbseMemNumaBorrowExportObj();
    return UBSE_OK;
}

UbseResult QueryNumaExportHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    std::string curNodeId = UbseNodeController::GetInstance().GetCurrentNodeId();
    UbseMemDebtQueryRequestSimpo simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    size_t size = 0;
    resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem query numa export deserialize failed, " << FormatRetCode(ret);
        return ret;
    }
    def::UbseMemDebtQueryRequest memReq = simpo.GetUbseMemDebtQueryRequest();
    UbseMemNumaBorrowExportObj obj = UbseNumaExportObjGet(memReq.exportNodeId, memReq.name, memReq.importNodeId, true);
    UbseMemNumaBorrowExportobjSimpoPtr resultSimpo = new (std::nothrow) UbseMemNumaBorrowExportobjSimpo;
    if (resultSimpo == nullptr) {
        UBSE_LOG_ERROR << "new simpo failed.";
        return UBSE_ERROR_NULLPTR;
    }
    resultSimpo->SetUbseMemNumaBorrowExportobj(obj);
    ret = resultSimpo->Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem numa export serialize failed, " << FormatRetCode(ret);
        return ret;
    }
    return CreateRespBuffer(*resultSimpo.Get(), resp);
}

UbseResult QueryAddrExport(def::UbseMemDebtQueryRequest request, UbseMemAddrBorrowExportObj& obj)
{
    const SendParam sendParam{request.exportNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
                              static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_ADDR_EXPORT)};

    UbseMemDebtQueryRequestSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemDebtQueryRequestSimpo();
    if (ubseRequestPtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemDebtQueryRequest(request);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemAddrBorrowExportobjSimpo();
    if (ubseResponsePtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ubseComModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto retCode = ubseComModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "rpc sync send query addr export obj failed, " << FormatRetCode(retCode);
        return retCode;
    }
    auto exportPtr = UbseBaseMessage::DeConvert<UbseMemAddrBorrowExportobjSimpo>(ubseResponsePtr);
    obj = exportPtr->GetUbseMemAddrBorrowExportObj();
    return UBSE_OK;
}

UbseResult QueryAddrExportHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    std::string curNodeId = UbseNodeController::GetInstance().GetCurrentNodeId();
    UbseMemDebtQueryRequestSimpo simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    size_t size = 0;
    resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem query addr export deserialize failed, " << FormatRetCode(ret);
        return ret;
    }
    def::UbseMemDebtQueryRequest memReq = simpo.GetUbseMemDebtQueryRequest();
    UbseMemAddrBorrowExportObj obj = UbseAddrExportObjGet(memReq.exportNodeId, memReq.name, memReq.importNodeId, true);
    UbseMemAddrBorrowExportobjSimpoPtr resultSimpo = new (std::nothrow) UbseMemAddrBorrowExportobjSimpo;
    if (resultSimpo == nullptr) {
        UBSE_LOG_ERROR << "new simpo failed.";
        return UBSE_ERROR_NULLPTR;
    }
    resultSimpo->SetUbseMemAddrBorrowExportobj(obj);
    ret = resultSimpo->Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem addr export serialize failed, " << FormatRetCode(ret);
        return ret;
    }
    return CreateRespBuffer(*resultSimpo.Get(), resp);
}

UbseResult QueryShareExport(def::UbseMemDebtQueryRequest request, UbseMemShareBorrowExportObj& obj)
{
    const SendParam sendParam{request.exportNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
                              static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_SHARE_EXPORT)};

    UbseMemDebtQueryRequestSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemDebtQueryRequestSimpo();
    if (ubseRequestPtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemDebtQueryRequest(request);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemShareBorrowExportobjSimpo();
    if (ubseResponsePtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ubseComModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto retCode = ubseComModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "rpc sync send query share export obj failed, " << FormatRetCode(retCode);
        return retCode;
    }
    auto exportPtr = UbseBaseMessage::DeConvert<UbseMemShareBorrowExportobjSimpo>(ubseResponsePtr);
    obj = exportPtr->GetUbseMemShareBorrowExportObj();
    return UBSE_OK;
}

UbseResult QueryShareExportHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    std::string curNodeId = UbseNodeController::GetInstance().GetCurrentNodeId();
    UbseMemDebtQueryRequestSimpo simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    size_t size = 0;
    resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem query share export deserialize failed, " << FormatRetCode(ret);
        return ret;
    }
    def::UbseMemDebtQueryRequest memReq = simpo.GetUbseMemDebtQueryRequest();
    UbseMemShareBorrowExportObj obj = UbseShareExportObjGet(memReq.exportNodeId, memReq.name, true);
    UbseMemShareBorrowExportobjSimpoPtr resultSimpo = new (std::nothrow) UbseMemShareBorrowExportobjSimpo;
    if (resultSimpo == nullptr) {
        UBSE_LOG_ERROR << "new simpo failed.";
        return UBSE_ERROR_NULLPTR;
    }
    resultSimpo->SetUbseMemShareBorrowExportobj(obj);
    ret = resultSimpo->Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem share export deserialize failed, " << FormatRetCode(ret);
        return ret;
    }
    return CreateRespBuffer(*resultSimpo.Get(), resp);
}

UbseResult QueryFdImport(def::UbseMemDebtQueryRequest request, UbseMemFdBorrowImportObj& obj)
{
    const SendParam sendParam{request.importNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
                              static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_FD_IMPORT)};

    UbseMemDebtQueryRequestSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemDebtQueryRequestSimpo();
    if (ubseRequestPtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemDebtQueryRequest(request);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemFdBorrowImportobjSimpo();
    if (ubseResponsePtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ubseComModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto retCode = ubseComModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "rpc sync send query fd import obj failed, " << FormatRetCode(retCode);
        return retCode;
    }
    auto importPtr = UbseBaseMessage::DeConvert<UbseMemFdBorrowImportobjSimpo>(ubseResponsePtr);
    obj = importPtr->GetUbseMemFdBorrowImportObj();
    return UBSE_OK;
}

UbseResult QueryFdImportHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    std::string curNodeId = UbseNodeController::GetInstance().GetCurrentNodeId();
    UbseMemDebtQueryRequestSimpo simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    size_t size = 0;
    resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem query fd import deserialize failed, " << FormatRetCode(ret);
        return ret;
    }
    def::UbseMemDebtQueryRequest memReq = simpo.GetUbseMemDebtQueryRequest();
    UbseMemFdBorrowImportObj obj = UbseFdImportObjGet(memReq.importNodeId, memReq.name, true);
    UbseMemFdBorrowImportobjSimpoPtr resultSimpo = new (std::nothrow) UbseMemFdBorrowImportobjSimpo;
    if (resultSimpo == nullptr) {
        UBSE_LOG_ERROR << "new simpo failed.";
        return UBSE_ERROR_NULLPTR;
    }
    resultSimpo->SetUbseMemFdBorrowImportobj(obj);
    ret = resultSimpo->Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem fd import deserialize failed, " << FormatRetCode(ret);
        return ret;
    }
    return CreateRespBuffer(*resultSimpo.Get(), resp);
}

UbseResult QueryNumaImport(def::UbseMemDebtQueryRequest request, UbseMemNumaBorrowImportObj& obj)
{
    const SendParam sendParam{request.importNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
                              static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_NUMA_IMPORT)};

    UbseMemDebtQueryRequestSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemDebtQueryRequestSimpo();
    if (ubseRequestPtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemDebtQueryRequest(request);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemNumaBorrowImportobjSimpo();
    if (ubseResponsePtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ubseComModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto retCode = ubseComModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "rpc sync send query numa import obj failed, " << FormatRetCode(retCode);
        return retCode;
    }
    auto importPtr = UbseBaseMessage::DeConvert<UbseMemNumaBorrowImportobjSimpo>(ubseResponsePtr);
    obj = importPtr->GetUbseMemNumaBorrowImportObj();
    return UBSE_OK;
}

UbseResult QueryNumaImportHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    std::string curNodeId = UbseNodeController::GetInstance().GetCurrentNodeId();
    UbseMemDebtQueryRequestSimpo simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    size_t size = 0;
    resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem query numa import deserialize failed, " << FormatRetCode(ret);
        return ret;
    }
    def::UbseMemDebtQueryRequest memReq = simpo.GetUbseMemDebtQueryRequest();
    UbseMemNumaBorrowImportObj obj = UbseNumaImportObjGet(memReq.importNodeId, memReq.name, true);
    UbseMemNumaBorrowImportobjSimpoPtr resultSimpo = new (std::nothrow) UbseMemNumaBorrowImportobjSimpo;
    if (resultSimpo == nullptr) {
        UBSE_LOG_ERROR << "new simpo failed.";
        return UBSE_ERROR_NULLPTR;
    }
    resultSimpo->SetUbseMemNumaBorrowImportobj(obj);
    ret = resultSimpo->Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem numa import deserialize failed, " << FormatRetCode(ret);
        return ret;
    }
    return CreateRespBuffer(*resultSimpo.Get(), resp);
}

UbseResult QueryAddrImport(def::UbseMemDebtQueryRequest request, UbseMemAddrBorrowImportObj& obj)
{
    const SendParam sendParam{request.importNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
                              static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_ADDR_IMPORT)};

    UbseMemDebtQueryRequestSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemDebtQueryRequestSimpo();
    if (ubseRequestPtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemDebtQueryRequest(request);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemAddrBorrowImportobjSimpo();
    if (ubseResponsePtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ubseComModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto retCode = ubseComModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "rpc sync send query addr import obj failed, " << FormatRetCode(retCode);
        return retCode;
    }
    auto importPtr = UbseBaseMessage::DeConvert<UbseMemAddrBorrowImportobjSimpo>(ubseResponsePtr);
    obj = importPtr->GetUbseMemAddrBorrowImportobj();
    return UBSE_OK;
}

UbseResult QueryAddrImportHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    std::string curNodeId = UbseNodeController::GetInstance().GetCurrentNodeId();
    UbseMemDebtQueryRequestSimpo simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    size_t size = 0;
    resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem query addr import deserialize failed, " << FormatRetCode(ret);
        return ret;
    }
    def::UbseMemDebtQueryRequest memReq = simpo.GetUbseMemDebtQueryRequest();
    UbseMemAddrBorrowImportObj obj = UbseAddrImportObjGet(memReq.importNodeId, memReq.name, true);
    UbseMemAddrBorrowImportobjSimpoPtr resultSimpo = new (std::nothrow) UbseMemAddrBorrowImportobjSimpo;
    if (resultSimpo == nullptr) {
        UBSE_LOG_ERROR << "new simpo failed.";
        return UBSE_ERROR_NULLPTR;
    }
    resultSimpo->SetUbseMemAddrBorrowImportobj(obj);
    ret = resultSimpo->Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem addr import deserialize failed, " << FormatRetCode(ret);
        return ret;
    }
    return CreateRespBuffer(*resultSimpo.Get(), resp);
}

UbseResult QueryShareImport(def::UbseMemDebtQueryRequest request, UbseMemShareBorrowImportObj& obj)
{
    const SendParam sendParam{request.importNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
                              static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_SHARE_IMPORT)};

    UbseMemDebtQueryRequestSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemDebtQueryRequestSimpo();
    if (ubseRequestPtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemDebtQueryRequest(request);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemShareBorrowImportobjSimpo();
    if (ubseResponsePtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ubseComModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto retCode = ubseComModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "rpc sync send query share import obj failed, " << FormatRetCode(retCode);
        return retCode;
    }
    auto importPtr = UbseBaseMessage::DeConvert<UbseMemShareBorrowImportobjSimpo>(ubseResponsePtr);
    obj = importPtr->GetUbseMemShareBorrowImportObj();
    return UBSE_OK;
}

UbseResult QueryShareImportHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    std::string curNodeId = UbseNodeController::GetInstance().GetCurrentNodeId();
    UbseMemDebtQueryRequestSimpo simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    size_t size = 0;
    resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem query share import deserialize failed, " << FormatRetCode(ret);
        return ret;
    }
    def::UbseMemDebtQueryRequest memReq = simpo.GetUbseMemDebtQueryRequest();
    UbseMemShareBorrowImportObj obj = UbseShareImportObjGet(memReq.importNodeId, memReq.name, true);
    UbseMemShareBorrowImportobjSimpoPtr resultSimpo = new (std::nothrow) UbseMemShareBorrowImportobjSimpo;
    if (resultSimpo == nullptr) {
        UBSE_LOG_ERROR << "new simpo failed.";
        return UBSE_ERROR_NULLPTR;
    }
    resultSimpo->SetUbseMemShareBorrowImportobj(obj);
    ret = resultSimpo->Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem share import deserialize failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return ret;
    }
    return CreateRespBuffer(*resultSimpo.Get(), resp);
}

UbseResult SendInvalidateSingleImportDebtRpc(const std::string& nodeId, const std::string& debtName,
                                             UbseMemBorrowType type)
{
    const SendParam sendParam{nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
                              static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_INVALIDATE_SINGLE_IMPORT_DEBT)};
    UbseMemOptReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemOptReqSimpo();
    if (ubseRequestPtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetOptRequest(debtName, nodeId, type);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemOptResultSimpo();
    if (ubseResponsePtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ubseComModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto retCode = ubseComModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "rpc sync send invalidate single import debt failed, " << FormatRetCode(retCode);
        return retCode;
    }
    auto resultPtr = UbseBaseMessage::DeConvert<UbseMemOptResultSimpo>(ubseResponsePtr);
    return resultPtr->GetResult();
}

UbseResult SendInvalidateSingleImportDebtRpcHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UbseMemOptReqSimpo simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    size_t size = 0;
    resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Mem invalidate single import debt deserialize failed, " << FormatRetCode(ret);
        return ret;
    }
    std::string name = simpo.GetName();
    UbseMemBorrowType type = simpo.GetType();
    UBSE_LOG_INFO << "Agent invalidate import debt, name=" << name << ", type=" << int(type);
    auto result = AgentInvalidateImportDebt(name, type);
    UbseMemOptResultSimpoPtr resultSimpo = new (std::nothrow) UbseMemOptResultSimpo();
    if (resultSimpo == nullptr) {
        UBSE_LOG_ERROR << "new simpo failed.";
        return UBSE_ERROR_NULLPTR;
    }
    resultSimpo->SetResult(result);
    ret = resultSimpo->Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem invalidate single import debt serialize failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return ret;
    }
    return CreateRespBuffer(*resultSimpo.Get(), resp);
}

UbseResult NotifyRemoteNumaStatus(const std::string& nodeId, const std::vector<std::pair<int64_t, int>>& numaStatus)
{
    const SendParam sendParam{nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
                              static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_REMOTE_NUMA_STATUS)};
    UbseMemRemoteNumaStatusPtr ubseRequestPtr = new (std::nothrow) UbseMemRemoteNumaStatus();
    if (ubseRequestPtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemRemoteNumaStatus(numaStatus);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemOptResultSimpo();
    if (ubseResponsePtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ubseComModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto retCode = ubseComModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "rpc sync send query remote numa status failed, " << FormatRetCode(retCode);
        return retCode;
    }
    auto resultPtr = UbseBaseMessage::DeConvert<UbseMemOptResultSimpo>(ubseResponsePtr);
    return resultPtr->GetResult();
}

UbseResult NotifyRemoteNumaStatusHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UbseMemRemoteNumaStatus simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    size_t size = 0;
    resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Mem query remote numa status deserialize failed, " << FormatRetCode(ret);
        return ret;
    }
    auto numaStatus = simpo.GetUbseRemoteNumaStatus();
    UBSE_LOG_INFO << "Agent query remote numa status";
    auto result = AgentModifyRemoteNumaStatus(numaStatus);
    UbseMemOptResultSimpoPtr resultSimpo = new (std::nothrow) UbseMemOptResultSimpo();
    if (resultSimpo == nullptr) {
        UBSE_LOG_ERROR << "new simpo failed.";
        return UBSE_ERROR_NULLPTR;
    }
    resultSimpo->SetResult(result);
    ret = resultSimpo->Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem query remote numa status serialize failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t* p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return ret;
    }
    return CreateRespBuffer(*resultSimpo.Get(), resp);
}

} // namespace ubse::mem::controller