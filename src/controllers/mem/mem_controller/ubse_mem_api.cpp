/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * UbseEngine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "ubse_mem_api.h"

#include <arpa/inet.h>
#include <securec.h>
#include <regex>

#include "src/sdk/c/include/ubs_engine.h"
#include "ubse_api_server_module.h"
#include "ubse_com_base.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_logger.h"
#include "ubse_mem_account.h"
#include "ubse_mem_advice.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_controller_api_agent.h"
#include "ubse_mem_controller_def_serial.h"
#include "ubse_mem_controller_module.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_mem_debt_info_partial_fetch_req.h"
#include "ubse_mem_debt_info_partial_fetch_res.h"
#include "ubse_mem_rpc_processor.h"
#include "ubse_mem_task_executor.h"
#include "ubse_mem_util.h"
#include "ubse_node_controller.h"
#include "ubse_os_util.h"
#include "ubse_serial_util.h"
#include "ubse_str_util.h"
#include "ubse_mem_util.h"

namespace usbe::mem::api {
using namespace ubse::context;
using namespace ubse::log;
using namespace ::api::server;
using namespace ubse::serial;
using namespace ubse::mem::controller;
using namespace ubse::utils;
using namespace ubse::mem::controller::message;
using namespace ubse::com;
using namespace ubse::mem::util;
using UbseBorrowDetailsRequestPair = std::pair<UbseMemDebtInfoPartialFetchReqPtr, UbseMemDebtInfoPartialFetchResPtr>;
UBSE_DEFINE_THIS_MODULE("ubse");

const double BYTES_PER_MB = 1024 * 1024; // 1MB = 1,048,576字节
const int BASE_10 = 10;

UbseResult UbseMemApi::UbseRegisterShmCliInterface(const std::shared_ptr<UbseApiServerModule> &apiServerModule)
{
    auto ret = apiServerModule->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                                   static_cast<uint16_t>(UBSE_MEM_CLI_SHM_ATTACH),
                                                   UbseCliShmAttachDispatch);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of CliShmAttach IPC-API failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    ret = apiServerModule->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                              static_cast<uint16_t>(UBSE_MEM_CLI_SHM_DETACH), UbseCliShmDetachDispatch);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of CliShmDetach IPC-API failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    ret = apiServerModule->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                              static_cast<uint16_t>(UBSE_MEM_CLI_SHM_INFO_GET_BY_NAME),
                                              UbseCliShmGetDispatch);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of CliShmGet IPC-API failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    ret = apiServerModule->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                              static_cast<uint16_t>(UBSE_MEM_CLI_SHM_CREATE), UbseCliShmCreateDispatch);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of CliShmCreate IPC-API failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemApi::Register()
{
    auto ubse_api_server_module = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (ubse_api_server_module == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = ubse_api_server_module->RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_BORROW_DETAIL_DEBT_PARTIAL_FETCH,
                                                          UbseBorrowDetailsFetchDebtHandle);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_CHECK_STATUS, UbseCheckMemoryStatus);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_CONFIG, UbseNodeMemConfigHandle);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_NUMA_STATUS, UbseNumaStatusHandler);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_NUMA_STATE_QUERY, QueryNumaStateHandler);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_NUMA_CREATE, UbseMemCliNumaCreate);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_NUMA_INFO_GET_BY_NAME,
        UbseMemCliNumaInfoGetByName);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_FD_CREATE, UbseMemCliFdCreate);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_FD_INFO_GET_BY_NAME,
        UbseMemCliFdInfoGetByName);
    ret |= UbseRegisterShmCliInterface(ubse_api_server_module);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of mem IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

struct MemoryInfo {
    std::string ubseName;
    std::string ubseType;
    std::string ubseBorrowNode;
    std::string ubseBorrowSize;
    std::string ubseLendNode;
    std::string ubseLendNuma;
    std::string ubseLendSize;
    std::string ubseStatus;
};

static UbseBorrowDetailsRequestPair UbseBorrowDetailsPrepareRequest(const UbseIpcMessage &req)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "debt fetch IPC request info is null.";
        return { nullptr, nullptr };
    }
    UbseMemDebtInfoPartialFetchReqPtr ubseRequestPtr = new (std::nothrow) UbseMemDebtInfoPartialFetchReq();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "new request ptr failed";
        return { nullptr, nullptr };
    }

    UbseMemDebtInfoPartialFetchResPtr ubseResponsePtr = new (std::nothrow) UbseMemDebtInfoPartialFetchRes();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "new response ptr failed";
        return { nullptr, nullptr };
    }

    ubseRequestPtr->SetInputRawData(req.buffer, req.length);
    if (ubseRequestPtr->Deserialize() != UBSE_OK) {
        UBSE_LOG_ERROR << "Deserialize failed.";
        return { nullptr, nullptr };
    }
    return { ubseRequestPtr, ubseResponsePtr };
}
uint32_t UbseBorrowDetailsSendRpcAndFetchResponse(const ubse::election::UbseRoleInfo &masterInfo,
    UbseMemDebtInfoPartialFetchReqPtr ubseRequestPtr, UbseMemDebtInfoPartialFetchResPtr ubseResponsePtr)
{
    const SendParam sendParam{ masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
        static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_PARTIAL_FETCH) };

    UbseContext &ubseContext = UbseContext::GetInstance();
    auto ubseComModule = ubseContext.GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "Communication module not init, " << FormatRetCode(UBSE_ERROR_MODULE_LOAD_FAILED);
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto retCode = ubseComModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr, false);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "rpc sync send metric failed, " << FormatRetCode(retCode);
        return retCode;
    }

    return UBSE_OK;
}

uint32_t UbseBorrowDetailsSendResponseToClient(UbseMemDebtInfoPartialFetchResPtr ubseResponsePtr,
    const UbseRequestContext &context)
{
    UbseIpcMessage partial_fetch{};
    if (ubseResponsePtr->InputRawData() == nullptr) {
        UBSE_LOG_ERROR << "fetch response data is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }

    partial_fetch.buffer = ubseResponsePtr->InputRawData();
    partial_fetch.length = ubseResponsePtr->InputRawDataSize();

    auto ubse_api_server_module = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (ubse_api_server_module == nullptr) {
        partial_fetch.buffer = nullptr;
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }

    auto ret = ubse_api_server_module->SendResponse(UBSE_OK, context.requestId, partial_fetch);
    if (ret != UBSE_OK) {
        partial_fetch.buffer = nullptr;
        UBSE_LOG_ERROR << " BorrowDetailsHandle response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

uint32_t UbseMemApi::UbseBorrowDetailsFetchDebtHandle(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    auto [ubseRequestPtr, ubseResponsePtr] = UbseBorrowDetailsPrepareRequest(req);
    if (ubseRequestPtr == nullptr || ubseResponsePtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }

    ubse::election::UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        return res;
    }

    res = UbseBorrowDetailsSendRpcAndFetchResponse(masterInfo, ubseRequestPtr, ubseResponsePtr);
    if (res != UBSE_OK) {
        return res;
    }

    res = UbseBorrowDetailsSendResponseToClient(ubseResponsePtr, context);
    if (res != UBSE_OK) {
        return res;
    }

    return UBSE_OK;
}

void UbseClusterList(std::vector<ubse::nodeController::UbseNodeInfo> &nodeList)
{
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos =
        ubse::nodeController::UbseNodeController::GetInstance().GetAllNodes();
    if (nodeInfos.empty()) {
        return;
    }
    std::vector<ubse::nodeController::UbseNodeInfo> staticNodeInfos =
        ubse::nodeController::UbseNodeController::GetInstance().GetStaticNodeInfo();
    nodeList.reserve(std::max(nodeInfos.size(), staticNodeInfos.size()));
    std::transform(nodeInfos.begin(), nodeInfos.end(), std::back_inserter(nodeList),
                   [](auto &kv) { return kv.second; });
    // 加入静态节点信息
    for (auto &nodeInfo : staticNodeInfos) {
        if (nodeInfos.find(nodeInfo.nodeId) == nodeInfos.end()) {
            ConvertStrToUint32(nodeInfo.nodeId, nodeInfo.slotId);
            nodeList.emplace_back(nodeInfo);
        }
    }
    std::sort(nodeList.begin(), nodeList.end(),
              [](ubse::nodeController::UbseNodeInfo &l, ubse::nodeController::UbseNodeInfo &r) {
                  return l.slotId < r.slotId;
              });
}

bool CheckAllNodeMemoryConfigValid(const std::vector<ubse::nodeController::UbseNodeInfo> &nodeList)
{
    bool foundFirst = false;
    ubse::nodeController::UbseNodeInfo tempNode;
    for (const auto &node : nodeList) {
        if (!node.nodeId.empty() && (node.clusterState != UbseNodeClusterState::UBSE_NODE_WORKING &&
                                     node.clusterState != UbseNodeClusterState::UBSE_NODE_SMOOTHING)) {
            continue;
        }
        if (node.isLender) {
            if (!foundFirst) {
                tempNode = node;
                foundFirst = true;
                continue;
            }
            if (node.blockSize != tempNode.blockSize) {
                UBSE_LOG_ERROR << "Invalid mem configuration. Lender node=" << node.nodeId
                               << " has block size=" << node.blockSize << ", expect size=" << tempNode.blockSize;
                return false;
            }
            if (node.allocator != tempNode.allocator) {
                UBSE_LOG_ERROR << "Invalid mem configuration. Lender node=" << node.nodeId
                               << " has allocator=" << static_cast<int>(node.allocator)
                               << ", allocator=" << static_cast<int>(tempNode.allocator);
                return false;
            }
        }
    }
    if (!foundFirst) {
        return false;
    }
    return true;
}

void SerializeCheckMemoryStatus(const std::vector<ubse::nodeController::UbseNodeInfo> &nodeList, UbseSerialization &ubseSerial)
{
    bool memConfigValid = CheckAllNodeMemoryConfigValid(nodeList);

    for (const auto &node : nodeList) {
        UBSE_LOG_INFO << "hostname=" << node.hostName << ", slotId=" << node.slotId
                      << ", clusterState=" << static_cast<uint32_t>(node.clusterState);
        std::string detail;
        bool isOnline = node.clusterState == ubse::nodeController::UbseNodeClusterState::UBSE_NODE_SMOOTHING ||
                        node.clusterState == ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING;
        bool isSysSentryReady = node.sysSentryState == UbseNodeSysSentryState::UBSE_NODE_SYSSENTRY_OK;
        bool isObmmKernelInserted = node.obmmState == UbseNodeObmmState::UBSE_NODE_OBMM_INSERTED;
        ubseSerial << ((!isOnline || node.hostName.empty() ? "-" : node.hostName) + "(" + std::to_string(node.slotId) +
                       ")");
        // 平滑对账和正常工作的时候ok
        ubseSerial << (memConfigValid && isOnline && isSysSentryReady && isObmmKernelInserted ? "ok" : "nok");
        std::string clusterDetail = isOnline ? "ok" : "nok";
        std::string sysSentryDetail = node.sysSentryState == UbseNodeSysSentryState::UBSE_NODE_SYSSENTRY_OK  ? "ok" :
                                      node.sysSentryState == UbseNodeSysSentryState::UBSE_NODE_SYSSENTRY_NOK ? "nok" :
                                                                                                               "unknown";
        std::string obmmDetail = node.obmmState == UbseNodeObmmState::UBSE_NODE_OBMM_INSERTED     ? "ok" :
                                 node.obmmState == UbseNodeObmmState::UBSE_NODE_OBMM_NOT_INSERTED ? "nok" :
                                                                                                    "unknown";
        if (node.clusterState == ubse::nodeController::UbseNodeClusterState::UBSE_NODE_UNKNOWN ||
            node.clusterState == ubse::nodeController::UbseNodeClusterState::UBSE_NODE_FAULT) {
            sysSentryDetail = "unknown";
            obmmDetail = "unknown";
        }
        detail.append("cluster state: ").append(clusterDetail).append("; obmm: ").append(obmmDetail).append("; sysSentry: ").append(sysSentryDetail);
        // detail = "cluster state: " + clusterDetail + "; obmm: " + obmmDetail + "; sysSentry: " + sysSentryDetail;
        ubseSerial << detail;
    }
}

uint32_t UbseMemApi::UbseCheckMemoryStatus(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "Cluster IPC request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    std::vector<ubse::nodeController::UbseNodeInfo> nodeList;
    UbseClusterList(nodeList);
    UbseSerialization ubseSerial;
    ubseSerial << (right_v<std::size_t>(nodeList.size()));
    // hostName(slotId), status;
    SerializeCheckMemoryStatus(nodeList, ubseSerial);
    if (!ubseSerial.Check()) {
        UBSE_LOG_ERROR << "Serialization of cluster response info failed";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbseIpcMessage res{ubseSerial.GetBuffer(), static_cast<uint32_t>(ubseSerial.GetLength())};
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, res);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "CheckMemoryStatus response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseMemApi::UbseNodeMemConfigHandle(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "Node mem config IPC request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    auto nodeMap = ubse::nodeController::UbseNodeController::GetInstance().GetAllNodes();
    UbseSerialization ubse_serial;
    ubse_serial << array_len_insert(nodeMap.size());
    for (const auto &[_, nodeInfo] : nodeMap) {
        ubse_serial << std::string(nodeInfo.hostName + "(" + std::to_string(nodeInfo.slotId) + ")");
        ubse_serial << nodeInfo.isLender;
    }
    if (!ubse_serial.Check()) {
        UBSE_LOG_ERROR << "Serialization of topo response info failed";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbseIpcMessage res{};
    res.buffer = ubse_serial.GetBuffer();
    if (!res.buffer) {
        UBSE_LOG_ERROR << "Serialization response failed.";
        return UBSE_ERROR_NULLPTR;
    }
    res.length = static_cast<uint32_t>(ubse_serial.GetLength());
    auto ubseApiModule = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (ubseApiModule == nullptr) {
        UBSE_LOG_ERROR << "Get ubse api server module failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    uint32_t ret = ubseApiModule->SendResponse(UBSE_OK, context.requestId, res);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseNodeMemConfigHandle response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

inline uint32_t UbseConvertBytesToMegabytes(uint64_t bytes)
{
    auto mb = static_cast<uint64_t>(std::round(static_cast<double>(bytes) / BYTES_PER_MB));
    if (mb > std::numeric_limits<uint32_t>::max()) {
        UBSE_LOG_ERROR << "Value exceeds uint32_t range.";
        return std::numeric_limits<uint32_t>::max();
    }
    return static_cast<uint32_t>(mb);
}

uint32_t UbseMemApi::UbseNumaStatusHandler(const UbseIpcMessage &req, const UbseRequestContext &context)

{
    std::vector<ubse::mem::account::UbseNumaNodeInfo> numaInfoList{};
    auto ret = UbseAllNumaInfo(numaInfoList);
    if (ret != UBSE_OK) {
        return ret;
    }
    UbseSerialization ubse_serial;
    ubse_serial << array_len_insert(numaInfoList.size());
    for (const auto &numaInfo : numaInfoList) {
        auto memUsed = numaInfo.mMemTotal - numaInfo.mMemFree;
        std::string usedPercent = "0";
        if (numaInfo.mMemTotal != 0) {
            constexpr double PERCENTAGE_FACTOR = 100.0; // 将小数转换为百分比的乘数
            constexpr int DECIMAL_PLACES = 1;           // 要保留的小数位数
            double percent =
                (static_cast<double>(memUsed) / static_cast<double>(numaInfo.mMemTotal)) * PERCENTAGE_FACTOR;
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(DECIMAL_PLACES) << percent;
            usedPercent = oss.str();
        }
        ubse_serial << numaInfo.hostName + "(" + numaInfo.nodeId + ")" << std::to_string(numaInfo.numaId)
                    << std::to_string(UbseConvertBytesToMegabytes(numaInfo.mMemTotal))
                    << std::to_string(UbseConvertBytesToMegabytes(memUsed))
                    << std::to_string(UbseConvertBytesToMegabytes(numaInfo.mMemFree)) << usedPercent;
    }
    if (!ubse_serial.Check()) {
        UBSE_LOG_ERROR << "Serialization of topo response info failed";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbseIpcMessage res{};
    res.buffer = ubse_serial.GetBuffer();
    if (!res.buffer) {
        UBSE_LOG_ERROR << "Serialization response failed.";
        return UBSE_ERROR_NULLPTR;
    }
    res.length = static_cast<uint32_t>(ubse_serial.GetLength());
    auto apiServerModule = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get ubse api server module failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, res);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseNumaStatusHandler response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t SerializeNumaState(UbseSerialization &ubse_serial, const ubse::mem::def::UbseMemNumaDesc &memNumaDesc,
                            UbseIpcMessage &res)
{
    ubse_serial << memNumaDesc.name << std::to_string(static_cast<uint32_t>(memNumaDesc.state));
    if (memNumaDesc.state == UbseMemStage::UBSE_EXIST || memNumaDesc.state == UbseMemStage::UBSE_ERR_ONLY_IMPORT) {
        ubse_serial << memNumaDesc.numaId << std::to_string(memNumaDesc.exportNode.slotId)
                    << std::to_string(memNumaDesc.importNode.slotId);
    }
    if (!ubse_serial.Check()) {
        UBSE_LOG_ERROR << "Serialization of state query response info failed";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    res.buffer = ubse_serial.GetBuffer();
    if (!res.buffer) {
        UBSE_LOG_ERROR << "Serialization response failed.";
        return UBSE_ERROR_NULLPTR;
    }
    res.length = static_cast<uint32_t>(ubse_serial.GetLength());
    return UBSE_OK;
}

uint32_t UbseMemApi::QueryNumaStateHandler(const UbseIpcMessage &request, const UbseRequestContext &context)
{
    std::string errorMsg{};
    UbseDeSerialization deserial{request.buffer, request.length};
    if (!deserial.Check()) {
        UBSE_LOG_ERROR << "Failed to deserial buffer.";
        return UBSE_ERROR;
    }
    std::string name;
    deserial >> name;
    if (!deserial.Check()) {
        UBSE_LOG_ERROR << "DeSerialization name failed.";
        return UBSE_ERROR;
    }
    if (!ubse::mem::util::CheckName(name)) {
        UBSE_LOG_ERROR << "Invalid name";
        return UBSE_ERROR;
    }
    ubse::mem::def::UbseMemNumaDesc memNumaDesc{};
    auto ret = ubse::mem::controller::UbseMemNumaGet(name, memNumaDesc, nullptr);
    if (ret == UBSE_ERR_NOT_EXIST) {
        UBSE_LOG_ERROR << "Numa info not exist, ret=" << FormatRetCode(ret);
        return UBSE_IPC_ERROR_QUERY_NUMA_NOT_EXIST;
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get numa info failed, ret=" << FormatRetCode(ret);
        return UBSE_IPC_ERROR_QUERY_STATE_FAILED;
    }
    UbseIpcMessage res{};
    UbseSerialization ubse_serial{};
    ret = SerializeNumaState(ubse_serial, memNumaDesc, res);
    if (ret != UBSE_OK) {
        return ret;
    }
    auto apiServerModule = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get ubse api server module failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, res);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "QueryNumaStateHandler response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t DeserializeAndValidateName(const UbseIpcMessage &buffer, std::string &name)
{
    UbseDeSerialization deSerialization(buffer.buffer, buffer.length);
    deSerialization >> name;

    if (!deSerialization.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize";
        return UBSE_ERR_INTERNAL;
    }

    if (!CheckName(name)) {
        UBSE_LOG_ERROR << "Invalid name";
        return UBSE_ERR_INTERNAL;
    }

    return UBSE_OK;
}

UbseResult BuildMemShareCreateReq(const UbseIpcMessage &buffer, const UbseRequestContext &context,
                                  UbseMemShareBorrowReq &req)
{
    // 解析请求参数
    UbseDeSerialization deserialization(buffer.buffer, buffer.length);
    deserialization >> req.size >> req.name >> array_len_capture(req.shmRegion.nodeNum);
    if (!deserialization.Check()) {
        UBSE_LOG_ERROR << "Failed to parse CLI create request, requestId: " << context.requestId;
        return UBSE_ERR_INTERNAL;
    }
    if (!CheckName(req.name)) {
        UBSE_LOG_ERROR << "Invalid name";
        return UBSE_ERROR_INVAL;
    }
    if (req.shmRegion.nodeNum > UBS_MEM_MAX_SLOT_NUM) {
        UBSE_LOG_ERROR << "Invalid node number: " << req.shmRegion.nodeNum;
        return UBSE_ERROR_INVAL;
    }
    if (req.shmRegion.nodeNum == 0) {
        auto nodeInfos = ubse::nodeController::UbseNodeController::GetInstance().GetAllNodes();
        req.shmRegion.nodeNum = nodeInfos.size();
        for (const auto &[_, nodeInfo] : nodeInfos) {
            ubse::adapter_plugins::mmi::UbseNodeInfo ubseNodeInfo{nodeInfo.slotId, nodeInfo.nodeId, nodeInfo.hostName};
            req.shmRegion.nodelist.push_back(ubseNodeInfo);
        }
        return UBSE_OK;
    }
    for (uint32_t i = 0; i < req.shmRegion.nodeNum; i++) {
        uint32_t slotId;
        deserialization >> slotId;
        if (!deserialization.Check()) {
            UBSE_LOG_ERROR << "Failed to deserialize slotId.";
            return UBSE_ERR_INTERNAL;
        }
        ubse::adapter_plugins::mmi::UbseNodeInfo nodeInfo;
        nodeInfo.index = slotId;
        nodeInfo.nodeId = std::to_string(slotId);
        req.shmRegion.nodelist.push_back(nodeInfo);
    }

    if (!deserialization.Check()) {
        UBSE_LOG_ERROR << "Failed to parse CLI create request, requestId: " << context.requestId;
        return UBSE_ERR_INTERNAL;
    }
    return UBSE_OK;
}

uint32_t UbseMemApi::UbseCliShmGetDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "UbseCliShmGetDispatch, request_id=" << context.requestId;
    // 参数验证和解析
    std::string name;
    auto ret = DeserializeAndValidateName(buffer, name);
    if (ret != UBSE_OK) {
        return ret;
    }

    // 查询共享内存描述符
    ubse::mem::def::UbseMemShmDesc shmDesc{};
    ret = UbseMemShmGet(name, shmDesc, nullptr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get shm desc, " << FormatRetCode(ret);
        return ret;
    }

    // 序列化数据
    ubse::election::UbseRoleInfo currentRoleInfo{};
    ret = UbseGetCurrentNodeInfo(currentRoleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
        return ret;
    }
    UbseSerialization serialization;
    if (!UbseCliShmDescSerialize(shmDesc, currentRoleInfo.nodeId, serialization)) {
        UBSE_LOG_ERROR << "Failed to serialize shm desc";
        return UBSE_ERROR_INVAL;
    }

    UbseIpcMessage responseMessage{serialization.GetBuffer(), static_cast<uint32_t>(serialization.GetLength())};
    // 发送响应
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServer == nullptr) {
        UBSE_LOG_ERROR << "Failed to get api server";
        return UBSE_ERROR_INVAL;
    }
    return apiServer->SendResponse(UBSE_OK, context.requestId, responseMessage);
}

uint32_t UbseMemApi::UbseCliShmAttachDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "UbseCliShmAttachDispatch, request_id=" << context.requestId;
    // 参数验证和解析
    std::string name;
    auto ret = DeserializeAndValidateName(buffer, name);
    if (ret != UBSE_OK) {
        ubse::election::UbseRoleInfo currentNodeInfo;
        auto res = UbseGetCurrentNodeInfo(currentNodeInfo);
        BorrowFailedAdvice("Import failed", name, "SHARE_BORROW", 0, "", currentNodeInfo.nodeId, ret,
                           MemAdvice::CHECK_FAILED);
        return ret;
    }

    // 构造请求
    UbseMemShareAttachReq req;
    req.name = std::move(name);

    // 异步执行
    return ExecuteOperationAsync<UbseMemShmAttachOperation>(context, std::move(req));
}

uint32_t UbseMemApi::UbseCliShmDetachDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "UbseCliShmDetachDispatch, request_id=" << context.requestId;
    // 参数验证和解析
    std::string name;
    auto ret = DeserializeAndValidateName(buffer, name);
    if (ret != UBSE_OK) {
        return ret;
    }

    // 构造请求
    UbseMemShareDetachReq req;
    req.name = std::move(name);

    // 异步执行
    return ExecuteOperationAsync<UbseMemShmDetachOperation>(context, std::move(req));
}

uint32_t UbseMemApi::UbseCliShmCreateDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "CLI shm create dispatch, requestId: " << context.requestId;

    // 解序列化和构造请求体
    UbseMemShareBorrowReq req{};
    auto ret = BuildMemShareCreateReq(buffer, context, req);
    if (ret != UBSE_OK) {
        ubse::election::UbseRoleInfo currentNodeInfo;
        auto res = UbseGetCurrentNodeInfo(currentNodeInfo);
        BorrowFailedAdvice("Export failed", req.name, "SHARE_BORROW", req.size, "", "", ret,
                           MemAdvice::CHECK_FAILED);
        return ret;
    }

    // 异步执行
    return ExecuteOperationAsync<UbseMemShmCreateOperation>(context, std::move(req));
}
uint32_t SendResponseCommon(UbseSerialization &serial, const UbseRequestContext &context)
{
    auto apiServerModule = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage response{ serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength()) };
    if (!response.buffer) {
        UBSE_LOG_ERROR << "Serialization response failed.";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " send response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseMemApi::UbseMemCliNumaInfoGetByName(const UbseIpcMessage &buffer, const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "UbseMemCliNumaGetInfo, request_id=" << context.requestId;
    if (buffer.buffer == nullptr) {
        UBSE_LOG_ERROR << "CliNumaGet IPC request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseDeSerialization deserialize(buffer.buffer, buffer.length);
    std::string name;
    deserialize >> name;
    if (!deserialize.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize request information.";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    UbseUdsInfo udsInfo = GenUdsInfo(context);
    ubse::mem::def::UbseMemNumaDesc memNumaDesc{};
    auto ret = ubse::mem::controller::UbseMemNumaGet(name, memNumaDesc, &udsInfo);
    UBSE_LOG_INFO << memNumaDesc.name << memNumaDesc.numaId << memNumaDesc.importNode.slotId <<
        memNumaDesc.exportNode.slotId << memNumaDesc.size << static_cast<uint32_t>(memNumaDesc.state);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemNumaGet failed," << FormatRetCode(ret);
        return ret;
    }
    UbseSerialization serial{};
    serial << memNumaDesc.name << memNumaDesc.numaId << memNumaDesc.importNode.slotId <<
        memNumaDesc.exportNode.slotId << memNumaDesc.size << enum_v(memNumaDesc.state);

    UBSE_LOG_INFO << memNumaDesc.name << memNumaDesc.numaId << memNumaDesc.importNode.slotId <<
        memNumaDesc.exportNode.slotId << memNumaDesc.size << static_cast<uint32_t>(memNumaDesc.state);

    if (!serial.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize response information.";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return SendResponseCommon(serial, context);
}

uint32_t UbseMemApi::UbseMemCliFdInfoGetByName(const UbseIpcMessage &buffer, const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "UbseMemCliFdGetInfo, request_id=" << context.requestId;
    if (buffer.buffer == nullptr) {
        UBSE_LOG_ERROR << "CliFdGet IPC request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseDeSerialization deserial{ buffer.buffer, buffer.length };
    std::string name{};
    deserial >> name;
    if (!deserial.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize request information.";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }

    UbseUdsInfo udsInfo = GenUdsInfo(context);
    ubse::mem::def::UbseMemFdDesc fdDesc;
    auto ret = ubse::mem::controller::UbseMemFdGet(name, fdDesc, &udsInfo);
    UBSE_LOG_INFO << fdDesc.name << " " <<
        (fdDesc.importNode.hostName.empty() ? std::string("-") : fdDesc.importNode.hostName) << " " <<
        std::to_string(fdDesc.importNode.slotId) << " " <<
        (fdDesc.exportNode.hostName.empty() ? std::string("-") : fdDesc.exportNode.hostName) << " " <<
        std::to_string(fdDesc.exportNode.slotId) << " " << fdDesc.totalMemSize << " " <<
        static_cast<uint32_t>(fdDesc.state);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemFdGet failed," << FormatRetCode(ret);
        return ret;
    }
    UbseSerialization serial{};
    serial << fdDesc.name << fdDesc.memIds << fdDesc.importNode.slotId << fdDesc.exportNode.slotId <<
        fdDesc.totalMemSize << enum_v(fdDesc.state);

    UBSE_LOG_INFO << fdDesc.name << " " <<
        (fdDesc.importNode.hostName.empty() ? std::string("-") : fdDesc.importNode.hostName) << " " <<
        std::to_string(fdDesc.importNode.slotId) << " " <<
        (fdDesc.exportNode.hostName.empty() ? std::string("-") : fdDesc.exportNode.hostName) << " " <<
        std::to_string(fdDesc.exportNode.slotId) << " " << fdDesc.totalMemSize << " " <<
        static_cast<uint32_t>(fdDesc.state);

    if (!serial.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize response information.";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return SendResponseCommon(serial, context);
}


bool CheckLinkInfo(const std::string &str)
{
    if (str.empty()) {
        UBSE_LOG_INFO << "link info is empty.";
        return true;
    }
    UBSE_LOG_INFO << "link info is " << str;
    const std::regex pattern(R"(^\d+/\d+/\d+-\d+/\d+/\d+$)");
    if (!std::regex_match(str, pattern)) {
        UBSE_LOG_ERROR << "Invalid link info. link info is " << str;
        return false;
    }
    return true;
}

UbseResult FillNumaInfoToCreateReq(const UbseIpcMessage &buffer, const UbseRequestContext &context,
    UbseMemNumaBorrowReq &req)
{
    // 解析请求参数
    UbseDeSerialization deserialization{ buffer.buffer, buffer.length };
    std::string name{};
    size_t size{};
    std::string linkInfo{};
    deserialization >> name >> size >> linkInfo;
    if (!deserialization.Check()) {
        UBSE_LOG_ERROR << "Failed to parse CLI create request, requestId: " << context.requestId;
        return UBSE_ERR_INTERNAL;
    }
    req.name = name;
    req.size = size;
    if (!CheckLinkInfo(linkInfo)) {
        return UBSE_ERROR_INVAL;
    }
    if (!CheckName(name)) {
        UBSE_LOG_ERROR << "Invalid name";
        return UBSE_ERROR_INVAL;
    }
    if (!linkInfo.empty()) {
        req.lowWatermark = 0;
        req.highWatermark = 100; // 指定端口借用走水线，高水线填值为100
        std::string errorMsg{};
        ubse::election::UbseRoleInfo currentNodeInfo;
        if (auto ret = UbseGetCurrentNodeInfo(currentNodeInfo); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to get current node info.";
            return ret;
        }
        if (auto ret = DealLinkInfo(linkInfo, req, currentNodeInfo, errorMsg); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal linkInfo.";
            return ret;
        }
        if (auto ret = FillSrcNuma(req, currentNodeInfo); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to fill src numa.";
            return ret;
        }
    }
    return UBSE_OK;
}

uint32_t UbseMemApi::UbseMemCliNumaCreate(const UbseIpcMessage &buffer, const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "CLI numa create dispatch, requestId: " << context.requestId;

    // 解序列化和构造请求体
    UbseMemNumaBorrowReq req{};
    auto ret = FillNumaInfoToCreateReq(buffer, context, req);
    if (ret != UBSE_OK) {
        ubse::election::UbseRoleInfo currentNodeInfo;
        auto res = UbseGetCurrentNodeInfo(currentNodeInfo);
        BorrowFailedAdvice("Import failed", req.name, "APP_NUMA_BORROW", req.size, "", currentNodeInfo.nodeId, ret,
                           MemAdvice::CHECK_FAILED);
        return ret;
    }

    // 异步执行
    return ExecuteOperationAsync<UbseMemNumaCreateOperation>(context, std::move(req));
}

UbseResult FillFdInfoToCreateReq(const UbseIpcMessage &buffer, const UbseRequestContext &context,
    UbseMemFdBorrowReq &req)
{
    // 解析请求参数
    UbseDeSerialization deserialization{ buffer.buffer, buffer.length };
    std::string name{};
    size_t size{};
    deserialization >> name >> size;
    if (!deserialization.Check()) {
        UBSE_LOG_ERROR << "Failed to parse CLI create request, requestId: " << context.requestId;
        return UBSE_ERR_INTERNAL;
    }
    req.name = name;
    req.size = size;
    if (!CheckName(req.name)) {
        UBSE_LOG_ERROR << "Invalid name";
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

uint32_t UbseMemApi::UbseMemCliFdCreate(const UbseIpcMessage &buffer, const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "CLI fd create dispatch, requestId: " << context.requestId;

    // 解序列化和构造请求体
    UbseMemFdBorrowReq req{};
    auto ret = FillFdInfoToCreateReq(buffer, context, req);
    if (ret != UBSE_OK) {
        ubse::election::UbseRoleInfo currentNodeInfo;
        auto res = UbseGetCurrentNodeInfo(currentNodeInfo);
        BorrowFailedAdvice("Import failed", req.name, "WATER_BORROW", req.size, "", currentNodeInfo.nodeId, ret,
                           MemAdvice::CHECK_FAILED);
        return ret;
    }

    // 异步执行
    return ExecuteOperationAsync<UbseMemCliFdCreateDispatch>(context, std::move(req));
}
} // namespace usbe::mem::api
