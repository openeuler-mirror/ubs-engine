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

#include "ubse_mem_controller_api_agent.h"
#include <regex>
#include "message/ubse_mem_addr_borrow_req_simpo.h"
#include "message/ubse_mem_fd_borrow_req_simpo.h"
#include "message/ubse_mem_numa_borrow_req_simpo.h"
#include "message/ubse_mem_return_req_simpo.h"
#include "message/ubse_mem_share_attach_req_simpo.h"
#include "message/ubse_mem_share_borrow_req_simpo.h"
#include "message/ubse_mem_share_detach_req_simpo.h"
#include "request_helper.h"
#include "request_id.h"
#include "ubse_api_server_module.h"
#include "ubse_com_module.h"
#include "ubse_conf.h"
#include "ubse_context.h"
#include "ubse_conf_module.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_logger_audit.h"
#include "ubse_logger.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_handler.h"
#include "ubse_mem_rpc_processor.h"
#include "ubse_mem_constants.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_sign_verifier.h"
#include "ubse_mem_util.h"
#include "ubse_serial_util.h"
#include "ubse_str_util.h"
#include "ubse_thread_pool_module.h"

namespace ubse::mem::controller::agent {
using namespace ubse::election;
using namespace ubse::mem::controller;
using namespace ubse::log;
using namespace ubse::com;
using namespace ubse::mem_controller;
using namespace ubse::config;
using namespace ubse::mem::controller::message;
using namespace ubse::task_executor;
using namespace ubse::mem::util;
using namespace ubse::context;
using namespace ubse::serial;
using namespace api::server;
using namespace ubse::mem::strategy;
static std::chrono::seconds WAIT_TIMEOUT(API_TIME_OUT); // seconds

UBSE_DEFINE_THIS_MODULE("ubse");

const int8_t CREATE_REQUEST_CHECK_FAILED = 100;
const int8_t DELETE_REQUEST_CHECK_FAILED = 101;
const int8_t CREATE_SUCCESS = UBSE_OK;
const int8_t MAX_NAME_LENGTH = 47;

uint64_t GenRequestId()
{
    uint8_t slotId = 0;
    ubse::election::UbseRoleInfo roleInfo{};
    auto ret = ubse::election::UbseGetCurrentNodeInfo(roleInfo);
    if (ret == UBSE_OK) {
        uint64_t nodeId = std::stoul(roleInfo.nodeId);
        slotId = static_cast<uint8_t>(nodeId);
    }
    static auto requestIdUtil = UbseRequestIdUtil(ubse::utils::UbseRequestType::CLI_REQUEST);
    return requestIdUtil.GenerateRequestId(slotId);
}

UbseResult FillLinkInfo(const std::vector<std::string> &link, UbseMemNumaBorrowReq &numaBorrowReq)
{
    numaBorrowReq.linkInfo.lenderNode = link[0];
    auto ret = ubse::utils::ConvertStrToInt(link[1], numaBorrowReq.linkInfo.lenderSocketId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to conver str to int.";
        return ret;
    }
    ret = ubse::utils::ConvertStrToInt(link[2], numaBorrowReq.linkInfo.lenderPort); // link[2]为借出端port
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to conver str to int.";
        return ret;
    }
    return UBSE_OK;
}

bool IsSocketExist(const uint32_t &socketId, const ubse::nodeController::UbseNodeInfo &nodeInfo,
                   UbseCpuLocation &location)
{
    for (const auto cpuInfo : nodeInfo.cpuInfos) {
        if (cpuInfo.second.socketId == socketId) {
            location = cpuInfo.first;
            return true;
        }
    }
    return false;
}

UbseResult CheckRemoteExist(const UbsePortInfo &portInfo, const std::vector<std::string> secondLink,
                            const std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos)
{
    uint32_t remoteChipId{};
    auto ret = ConvertStrToUint32(portInfo.remoteChipId, remoteChipId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to convert remoteChipId from str to int."
                          "remoteChipId is "
                       << portInfo.remoteChipId;
        return UBSE_ERROR;
    }
    UbseCpuLocation remoteLocation{.nodeId = portInfo.remoteSlotId, .chipId = remoteChipId};
    ubse::nodeController::UbseNodeInfo remoteNode{};
    for (const auto node : nodeInfos) {
        if (std::to_string(node.second.slotId) == remoteLocation.nodeId) {
            remoteNode = node.second;
            break;
        }
    }
    auto remoteCpuIter = remoteNode.cpuInfos.find(remoteLocation);
    if (remoteCpuIter == remoteNode.cpuInfos.end()) {
        UBSE_LOG_ERROR << "Failed to find remote cpu info, remoteSlotId is " << remoteLocation.nodeId << ", chipId is "
                       << remoteLocation.chipId;
        return UBSE_ERROR;
    }
    if (secondLink.size() != 3) {  // 长度为3
        UBSE_LOG_ERROR << "second link size is false. size is " << secondLink.size();
        return UBSE_ERROR;
    }
    // secondLink[1]表示 socketId
    if (std::to_string(remoteCpuIter->second.socketId) != secondLink[1]) {
        UBSE_LOG_ERROR << "the remote socket mismatch. "
                          "the remote socket is "
                       << remoteCpuIter->second.socketId;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult CheckLinkExist(std::vector<std::string> firstLink, std::vector<std::string> secondLink)
{
    auto nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    ubse::nodeController::UbseNodeInfo nodeInfo{};
    for (const auto node : nodeInfos) {
        if (std::to_string(node.second.slotId) == firstLink[0]) {
            nodeInfo = node.second;
            break;
        }
    }
    if (nodeInfo.nodeId == "") {
        UBSE_LOG_ERROR << "Invalid slot id.";
        return UBSE_ERROR;
    }
    uint32_t firstSocketId{0};
    if (auto ret = ubse::utils::ConvertStrToUint32(firstLink[1], firstSocketId); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to conver str to int.";
        return ret;
    }
    UBSE_LOG_INFO << "first link socket is " << firstLink[1] << "nodeId is " << firstLink[0];
    uint32_t secondSocket{0};
    if (auto ret = ubse::utils::ConvertStrToUint32(secondLink[1], secondSocket); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to conver str to int.";
        return ret;
    }
    UbseCpuLocation location{};
    if (!IsSocketExist(firstSocketId, nodeInfo, location)) {
        UBSE_LOG_ERROR << "Invalid link. The sockets are not direct link.";
        return UBSE_ERROR;
    }
    for (const auto cpuInfo : nodeInfo.cpuInfos) {
        UBSE_LOG_INFO << "cpuInfo node id is " << cpuInfo.first.nodeId;
        UBSE_LOG_INFO << "cpuInfo chipId id is " << cpuInfo.first.chipId;
    }
    auto it = nodeInfo.cpuInfos.find(location);
    if (it == nodeInfo.cpuInfos.end()) {
        UBSE_LOG_ERROR << "Invalid link. The sockets are not direct link.";
        return UBSE_ERROR;
    }
    auto portIter = it->second.portInfos.find(firstLink[2]); // firstLink[2]为portId
    if (portIter == it->second.portInfos.end() || portIter->second.portStatus == PortStatus::DOWN) {
        UBSE_LOG_ERROR << "Invalid port. port is " << firstLink[2]; // firstLink[2]为portId
        return UBSE_ERROR;
    }
    if (portIter->second.remotePortId != secondLink[2]                                    // secondLink[2]为对端端口号
        || portIter->second.remoteSlotId != secondLink[0]) {                              // secondLink[0]为对端nodeId
        UBSE_LOG_ERROR << "Invalid link. The ports are not direct link " << firstLink[2]; // firstLink[2]为端口号
        return UBSE_ERROR;
    }
    return CheckRemoteExist(portIter->second, secondLink, nodeInfos);
}

UbseResult DealLinkInfo(const std::string &linkInfo, UbseMemNumaBorrowReq &numaBorrowReq,
                        const UbseRoleInfo &currentNodeInfo, std::string &errorMsg)
{
    std::vector<std::string> linkInfos{};
    std::vector<std::string> firstLink{};
    std::vector<std::string> secondLink{};
    ubse::utils::StrSplit(linkInfo, "-", linkInfos);
    if (linkInfos.size() != 2) { // linkinfo格式为0/0/1-1/0/1，-分割后size为2
        errorMsg = "Invalid linkInfo. Please check the form of linkInfo.";
        UBSE_LOG_ERROR << "Invalid link infos, size is " << linkInfos.size();
        return UBSE_ERROR;
    }
    ubse::utils::StrSplit(linkInfos[0], "/", firstLink);
    ubse::utils::StrSplit(linkInfos[1], "/", secondLink);
    if (firstLink.size() != 3 || secondLink.size() != 3) { // 用/分割后size为3
        errorMsg = "Invalid linkInfo. Please check the form of linkInfo.";
        UBSE_LOG_ERROR << "Invalid link infos.";
        return UBSE_ERROR;
    }
    if (auto ret = CheckLinkExist(firstLink, secondLink); ret != UBSE_OK) {
        errorMsg = "Link not exist.";
        UBSE_LOG_ERROR << "Link not exist.";
        return UBSE_ERROR;
    }
    if (firstLink[0] == currentNodeInfo.nodeId) {
        if (auto ret = ubse::utils::ConvertStrToInt(firstLink[1], numaBorrowReq.srcSocket); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to conver str to int.";
            return ret;
        }
        if (auto ret = FillLinkInfo(secondLink, numaBorrowReq) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to fill link info.";
            return ret;
        }
        return UBSE_OK;
    }
    if (secondLink[0] == currentNodeInfo.nodeId) {
        if (auto ret = ubse::utils::ConvertStrToInt(secondLink[1], numaBorrowReq.srcSocket); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to conver str to int.";
            return ret;
        }
        if (auto ret = FillLinkInfo(firstLink, numaBorrowReq) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to fill link info.";
            return ret;
        }
        return UBSE_OK;
    }
    errorMsg = "The node and the linkInfo is not matched.";
    UBSE_LOG_ERROR << errorMsg;
    return UBSE_ERROR;
}

UbseResult FillSrcNuma(UbseMemNumaBorrowReq &numaBorrowReq, const UbseRoleInfo &currentNodeInfo)
{
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(currentNodeInfo.nodeId);
    if (nodeInfo.nodeId == "") {
        UBSE_LOG_ERROR << "Failed to find node info by nodeId, nodeId is " << currentNodeInfo.nodeId;
        return UBSE_ERROR;
    }
    auto srcSocket = numaBorrowReq.srcSocket;
    UBSE_LOG_INFO << "Src socket is " << srcSocket << ", import node id is " << currentNodeInfo.nodeId;
    for (const auto &numaInfo : nodeInfo.numaInfos) {
        if (numaInfo.second.socketId == srcSocket) {
            numaBorrowReq.srcNuma = numaInfo.first.numaId;
            UBSE_LOG_INFO << "Src numa is " << numaBorrowReq.srcNuma;
            return UBSE_OK;
        }
    }
    UBSE_LOG_ERROR << "Failed to find correct numa.";
    return UBSE_ERROR;
}

uint32_t ReplyDeleteErrorMsg(const UbseRequestContext &context, const std::string &errorMsg, const uint32_t &errorCode)
{
    auto ubseApiModule = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (ubseApiModule == nullptr) {
        UBSE_LOG_ERROR << "Get ubse api server module failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage response;
    UbseSerialization serial;
    serial << errorCode << errorMsg;
    response.buffer = serial.GetBuffer();
    response.length = serial.GetLength();
    if (!response.buffer) {
        UBSE_LOG_ERROR << "Serialization response failed.";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = ubseApiModule->SendResponse(UBSE_OK, context.requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Send response failed, " << FormatRetCode(ret);
    }
    return ret;
}

void SwitchType(MemOperationType &operationType, const std::string &type)
{
    if (type == "fd") {
        operationType = MemOperationType::FD_RETURN;
    } else if (type == "addr") {
        operationType = MemOperationType::ADDR_RETURN;
    } else if (type == "share") {
        operationType = MemOperationType::SHARED_RETURN;
    } else {
        operationType = MemOperationType::NUMA_RETURN;
    }
}

uint32_t DeleteMemoryHandler(const UbseIpcMessage &request, const UbseRequestContext &context)
{
    std::string errorMsg{};
    UbseDeSerialization deserial{request.buffer, request.length};
    UbseMemReturnReq req;
    std::string type{};
    deserial >> req.name >> type;
    if (!deserial.Check()) {
        UBSE_LOG_ERROR << "Failed to deserial buffer.";
        return UBSE_ERROR;
    }
    if (!CheckName(req.name)) {
        errorMsg = "Invalid name. Please check the form of name.";
        UBSE_LOG_ERROR << "Invalid name. length is " << req.name.length() <<" or includes invalid characters.";
        return ReplyDeleteErrorMsg(context, errorMsg, DELETE_REQUEST_CHECK_FAILED);
    }
    UBSE_AUDIT_OPERATE("DeleteMemoryHandler") << "Start to delete memory, name is " << req.name << "type is " << type;
    UbseRoleInfo currentNodeInfo;
    if (auto ret = UbseGetCurrentNodeInfo(currentNodeInfo); ret != UBSE_OK) {
        errorMsg = "Failed to get server node info.";
        UBSE_LOG_ERROR << "Failed to get current node info.";
        return ReplyDeleteErrorMsg(context, errorMsg, DELETE_REQUEST_CHECK_FAILED);
    }
    req.requestNodeId = currentNodeInfo.nodeId;
    req.importNodeId = currentNodeInfo.nodeId;
    req.requestId = GenRequestId();
    req.udsInfo = GenUdsInfo(context);
    UbseMemOperationResp resp;
    MemOperationType operationType{};
    SwitchType(operationType, type);
    auto ret = UbseMemReturn(req, operationType, resp);
    if (ret != UBSE_OK) {
        return ret;
    }
    return ReplyDeleteErrorMsg(context, resp.errMsg, resp.errorCode);
}

void RegSpecifyLinkInfoCreateHandler()
{
    auto ubseApiModule = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (ubseApiModule == nullptr) {
        UBSE_LOG_ERROR << "Get ubse api server module failed. ";
        return;
    }
    auto ret = ubseApiModule->RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_DELETE_MEMORY, DeleteMemoryHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register ipc handler failed, " << FormatRetCode(ret);
        return;
    }
}

uint32_t Init()
{
    auto configModule = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (configModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get config module.";
        return UBSE_ERROR_NULLPTR;
    }
    uint32_t timeout = API_TIME_OUT;
    auto ret = configModule->GetConf("ubse.memory", "api.timeout", timeout);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get api timeout from config. use default value=" << API_TIME_OUT;
        timeout = API_TIME_OUT;
    }
    // api接口超时时间配置范围【1,3600】
    if (timeout == NO_0 || timeout > MAX_TIME_OUT) {
        UBSE_LOG_WARN << "Get api timeout out of range, use default value=" << API_TIME_OUT;
        timeout = API_TIME_OUT;
    }
    WAIT_TIMEOUT = std::chrono::seconds(timeout);
    InitAgentMaxWaitTime(timeout);
    auto executorModule = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (executorModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get executor module.";
        return UBSE_ERROR_NULLPTR;
    }
    ret = executorModule->Create("MemBorrowWaitTimeOutExecutor", THREAD_NUM, Queue_Capacity);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to create executor:MemBorrowWaitTimeOutExecutor," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    RegSpecifyLinkInfoCreateHandler();
    return UbseMemOperationRespHandler::RegUbseMemOperationRespHandlerToServer();
}

std::chrono::seconds GetWaitTimeout()
{
    return WAIT_TIMEOUT;
}

static UbseResult SendRpcRequestForFdBorrow(const UbseMemFdBorrowReq &req)
{
    UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master info failed, " << FormatRetCode(res);
        return res;
    }
    SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_FD_BORROW)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseMemFdBorrowReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemFdBorrowReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse request ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemFdBorrowReq(req);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse response ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    return comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
}

void DealBorrowWaitTimeOut(const std::string &name, const std::string &requestNodeId, const std::string &importNodeId,
                           const MemOperationType &type)
{
    auto memBorrowWaitTimeOutExecutor = GetExecutor("MemBorrowWaitTimeOutExecutor");
    UbseMemReturnReq returnReq;
    returnReq.name = name;
    returnReq.requestNodeId = requestNodeId;
    returnReq.importNodeId = importNodeId;
    memBorrowWaitTimeOutExecutor->Execute([returnReq, type] {
        UbseMemOperationResp returnResp;
        ubse::mem::controller::agent::UbseMemReturn(returnReq, type, returnResp);
    });
}

uint32_t UbseMemFdBorrow(UbseMemFdBorrowReq &req, UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "begin fd borrow, name is " << req.name << ", requestNodeId is " << req.requestNodeId
                  << ", request_id=" << req.requestId;
    if (IsHighSafety()) {
        if (const auto res =
                UbseMemSignVerifier::Sign("numa", req.trustRingData.reqSignedData, req.trustRingData.trustRingId);
            res != UBSE_OK) {
            UBSE_LOG_ERROR << "Sign for request failed, " << FormatRetCode(res);
            return res;
            }
    }
    // 创建请求
    auto requestId = GetRequestIdNew(req.name, req.requestNodeId);
    auto respMgr = FutureMgr::CreateInstance(requestId);
    if (respMgr == nullptr) {
        UBSE_LOG_ERROR << "respMgr is null for request ID: " << requestId;
        return UBSE_ERROR;
    }
    auto respFuture = respMgr->GetFuture<UbseMemOperationResp>();
    UbseResult ret = SendRpcRequestForFdBorrow(req);
    if (ret != UBSE_OK) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = UBSE_ERR_TIMEOUT;
        UBSE_LOG_ERROR << "requestId=" << requestId << "RpcSend dispatch failed";
        return ret;
    }
    UBSE_LOG_INFO << "begin wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId
                  << ", request_id=" << req.requestId;
    if (respFuture.wait_for(GetWaitTimeout()) != std::future_status::ready) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = UBSE_ERR_TIMEOUT;
        UBSE_LOG_ERROR << "requestId=" << requestId << " borrow timeout.";
        DealBorrowWaitTimeOut(req.name, req.requestNodeId, req.importNodeId, MemOperationType::FD_RETURN);
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "End to wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId
                  << ", request_id=" << req.requestId;
    resp = respFuture.get();
    return ret;
}

static UbseResult SendRpcRequestForNumaBorrow(const UbseMemNumaBorrowReq &req)
{
    UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master info failed, " << FormatRetCode(res);
        return res;
    }
    SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_NUMA_BORROW)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseMemNumaBorrowReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemNumaBorrowReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse request ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemNumaBorrowReq(req);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse response ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    return comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
}

uint32_t UbseMemNumaBorrow(UbseMemNumaBorrowReq &req, UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "begin numa borrow, name is " << req.name << ", requestNodeId is " << req.requestNodeId
                  << ", request_id=" << req.requestId;
    if (IsHighSafety()) {
        if (const auto res =
                UbseMemSignVerifier::Sign("numa", req.trustRingData.reqSignedData, req.trustRingData.trustRingId);
            res != UBSE_OK) {
            UBSE_LOG_ERROR << "Sign for request failed, " << FormatRetCode(res);
            return res;
        }
    }
    // 创建请求
    auto requestId = GetRequestId(req.name, req.requestNodeId);
    auto respMgr = FutureMgr::CreateInstance(requestId);
    if (respMgr == nullptr) {
        UBSE_LOG_ERROR << "requestId=" << requestId << "Create future instance failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto respFuture = respMgr->GetFuture<UbseMemOperationResp>();
    auto ret = SendRpcRequestForNumaBorrow(req);
    if (ret != UBSE_OK) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = UBSE_ERR_TIMEOUT;
        UBSE_LOG_ERROR << "requestId=" << requestId << "RpcSend dispatch failed";
        return ret;
    }
    UBSE_LOG_INFO << "begin wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId
                  << ", request_id=" << req.requestId;
    if (respFuture.wait_for(GetWaitTimeout()) != std::future_status::ready) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = UBSE_ERR_TIMEOUT;
        UBSE_LOG_ERROR << "requestId=" << requestId << " borrow timeout.";
        DealBorrowWaitTimeOut(req.name, req.requestNodeId, req.importNodeId, MemOperationType::NUMA_RETURN);
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "End to wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId
                  << ", request_id=" << req.requestId;
    resp = respFuture.get();
    return ret;
}

static UbseResult SendRpcRequestForAddrBorrow(const UbseMemAddrBorrowReq &req)
{
    UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master info failed, " << FormatRetCode(res);
        return res;
    }
    SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_ADDR_BORROW)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseMemAddrBorrowReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemAddrBorrowReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse request ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemAddrBorrowReq(req);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    return comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
}

uint32_t UbseMemAddrBorrow(UbseMemAddrBorrowReq &req, UbseMemOperationResp &resp)
{
    // 创建请求
    UBSE_LOG_INFO << "begin addr borrow, name is " << req.name << ", requestNodeId is " << req.requestNodeId
                  << ", request_id=" << req.requestId;
    if (IsHighSafety()) {
        if (const auto res =
                UbseMemSignVerifier::Sign("addr", req.trustRingData.reqSignedData, req.trustRingData.trustRingId);
            res != UBSE_OK) {
            UBSE_LOG_ERROR << "Sign for request failed, " << FormatRetCode(res);
            return res;
            }
    }
    if (req.wrDelayComp != 0 && req.wrDelayComp != 1) { // 0为接力模式，1为非接力模式
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = UBSE_MEMCONTROLLER_ERROR_COMP_ERROR;
        UBSE_LOG_ERROR << "Only relay mode and non-relay mode are supported currently,"
                          " the value of wrDelayComp is "
                       << req.wrDelayComp;
        return UBSE_ERROR;
    }
    auto requestId = GetRequestIdNew(req.name, req.requestNodeId);
    auto respMgr = FutureMgr::CreateInstance(requestId);
    if (respMgr == nullptr) {
        UBSE_LOG_ERROR << "respMgr is null for request ID: " << requestId;
        return UBSE_ERROR;
    }
    auto respFuture = respMgr->GetFuture<UbseMemOperationResp>();
    UbseResult ret = SendRpcRequestForAddrBorrow(req);
    if (ret != UBSE_OK) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = UBSE_ERR_TIMEOUT;
        UBSE_LOG_ERROR << "requestId=" << requestId << "RpcSend dispatch failed";
        return ret;
    }
    UBSE_LOG_INFO << "begin wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId;
    if (respFuture.wait_for(GetWaitTimeout()) != std::future_status::ready) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = UBSE_ERR_TIMEOUT;
        UBSE_LOG_ERROR << "requestId=" << requestId << " borrow timeout.";
        DealBorrowWaitTimeOut(req.name, req.requestNodeId, req.importNodeId, MemOperationType::ADDR_RETURN);
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "End to wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId;
    resp = respFuture.get();
    return ret;
}

static UbseResult SendRpcRequestForShareBorrow(const UbseMemShareBorrowReq &req)
{
    UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master info failed, " << FormatRetCode(res);
        return res;
    }
    SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseMemShareBorrowReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemShareBorrowReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse request ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemShareBorrowReq(req);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse response ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    return comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
}

uint32_t UbseMemShareBorrow(UbseMemShareBorrowReq &req, UbseMemOperationResp &resp)
{
    // 创建请求
    if (IsHighSafety()) {
        if (const auto res =
                UbseMemSignVerifier::Sign("share", req.trustRingData.reqSignedData, req.trustRingData.trustRingId);
            res != UBSE_OK) {
            UBSE_LOG_ERROR << "Sign for request failed, " << FormatRetCode(res);
            return res;
        }
    }

    auto requestId = GetRequestIdNew(req.name, req.requestNodeId);
    auto respMgr = FutureMgr::CreateInstance(requestId);
    if (respMgr == nullptr) {
        UBSE_LOG_ERROR << "respMgr is null for request ID: " << requestId;
        return UBSE_ERROR;
    }
    auto respFuture = respMgr->GetFuture<UbseMemOperationResp>();
    UbseResult ret = SendRpcRequestForShareBorrow(req);
    if (ret != UBSE_OK) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = UBSE_ERR_TIMEOUT;
        UBSE_LOG_ERROR << "requestId=" << requestId << "RpcSend dispatch failed";
        return ret;
    }
    UBSE_LOG_INFO << "begin wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId;
    if (respFuture.wait_for(GetWaitTimeout()) != std::future_status::ready) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = UBSE_ERR_TIMEOUT;
        UBSE_LOG_ERROR << "requestId=" << requestId << " borrow timeout.";
        DealBorrowWaitTimeOut(req.name, req.requestNodeId, "", MemOperationType::SHARED_RETURN);
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "End to wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId;
    resp = respFuture.get();
    return ret;
}

static UbseResult SendRpcRequestShareAttach(const UbseMemShareAttachReq &req)
{
    UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master info failed, " << FormatRetCode(res);
        return res;
    }
    SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_ATTACH)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseMemShareAttachReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemShareAttachReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse request ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemShareAttachReq(req);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse response ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    return comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
}

uint32_t UbseMemShareAttach(const UbseMemShareAttachReq &req, UbseMemOperationResp &resp)
{
    // 创建请求
    auto requestId = GetRequestIdNew(req.name, req.requestNodeId);
    auto respMgr = FutureMgr::CreateInstance(requestId);
    if (respMgr == nullptr) {
        UBSE_LOG_ERROR << "respMgr is null for request ID: " << requestId;
        return UBSE_ERROR;
    }
    auto respFuture = respMgr->GetFuture<UbseMemOperationResp>();
    UbseResult ret = SendRpcRequestShareAttach(req);
    if (ret != UBSE_OK) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = UBSE_ERR_TIMEOUT;
        UBSE_LOG_ERROR << "requestId=" << requestId << "RpcSend dispatch failed";
        return ret;
    }
    UBSE_LOG_INFO << "begin wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId;
    if (respFuture.wait_for(GetWaitTimeout()) != std::future_status::ready) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = UBSE_ERR_TIMEOUT;
        UBSE_LOG_ERROR << "requestId=" << requestId << " borrow timeout.";
        DealBorrowWaitTimeOut(req.name, req.requestNodeId, req.importNodeId, MemOperationType::SHARED_RETURN);
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "End to wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId;
    resp = respFuture.get();
    return ret;
}

static UbseResult SendRpcRequestForShareDetach(const UbseMemShareDetachReq &req)
{
    UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master info failed, " << FormatRetCode(res);
        return res;
    }
    SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_DETACH)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseMemShareDetachReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemShareDetachReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse request ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemShareDetachReq(req);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    return comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
}

uint32_t UbseMemShareDetach(const UbseMemShareDetachReq &req, UbseMemOperationResp &resp)
{
    // 创建请求
    auto requestId = GetRequestIdNew(req.name, req.requestNodeId);
    auto respMgr = FutureMgr::CreateInstance(requestId);
    if (respMgr == nullptr) {
        UBSE_LOG_ERROR << "respMgr is null for request ID: " << requestId;
        return UBSE_ERROR;
    }
    auto respFuture = respMgr->GetFuture<UbseMemOperationResp>();
    UbseResult ret = SendRpcRequestForShareDetach(req);
    if (ret != UBSE_OK) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = UBSE_ERR_TIMEOUT;
        UBSE_LOG_ERROR << "requestId=" << requestId << "RpcSend dispatch failed";
        return ret;
    }
    UBSE_LOG_INFO << "begin wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId;
    if (respFuture.wait_for(GetWaitTimeout()) != std::future_status::ready) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = UBSE_ERR_TIMEOUT;
        UBSE_LOG_ERROR << "requestId=" << requestId << " borrow timeout.";
        auto memBorrowWaitTimeOutExecutor = GetExecutor("MemBorrowWaitTimeOutExecutor");
        memBorrowWaitTimeOutExecutor->Execute([req, resp] { SendRpcRequestForShareDetach(req); });
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "End to wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId;
    resp = respFuture.get();
    return ret;
}

void SwitchReturnType(SendParam &sendParam, const MemOperationType &type)
{
    switch (type) {
        case MemOperationType::FD_RETURN:
            sendParam.SetOpCode(static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_RETURN));
            break;
        case MemOperationType::NUMA_RETURN:
            sendParam.SetOpCode(static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_NUMA_RETURN));
            break;
        case MemOperationType::SHARED_RETURN:
            sendParam.SetOpCode(static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_SHARE_RETURN));
            break;
        case MemOperationType::ADDR_RETURN:
            sendParam.SetOpCode(static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_ADDR_RETURN));
            break;
        default:
            sendParam.SetOpCode(static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_NUMA_RETURN));
    }
}

static UbseResult SendRpcRequestForReturn(const UbseMemReturnReq &req, const MemOperationType &type)
{
    UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master info failed, " << FormatRetCode(res);
        return res;
    }
    SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
                        static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_NUMA_RETURN)};
    SwitchReturnType(sendParam, type);
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseMemReturnReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemReturnReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse request ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemReturnReq(req);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "UbseBaseMessagePtr is null";
        return UBSE_ERROR;
    }
    return comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
}

uint32_t UbseMemReturn(const UbseMemReturnReq &req, const MemOperationType &type, UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "begin mem return, name is " << req.name << ", requestNodeId is " << req.requestNodeId
                  << ", request_id=" << req.requestId;
    // 创建请求
    auto requestId = GetRequestIdNew(req.name, req.requestNodeId);
    auto respMgr = FutureMgr::CreateInstance(requestId);
    if (respMgr == nullptr) {
        UBSE_LOG_ERROR << "respMgr is null for request ID: " << requestId;
        return UBSE_ERROR;
    }
    auto respFuture = respMgr->GetFuture<UbseMemOperationResp>();
    UbseResult ret = SendRpcRequestForReturn(req, type);
    if (ret != UBSE_OK) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = UBSE_ERR_TIMEOUT;
        UBSE_LOG_ERROR << "requestId=" << requestId << "RpcSend dispatch failed";
        return ret;
    }
    UBSE_LOG_INFO << "begin wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId
                  << ", request_id=" << req.requestId;
    if (respFuture.wait_for(GetWaitTimeout()) != std::future_status::ready) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = UBSE_ERR_TIMEOUT;
        UBSE_LOG_ERROR << "requestId=" << requestId << " borrow timeout.";
        auto memBorrowWaitTimeOutExecutor = GetExecutor("MemBorrowWaitTimeOutExecutor");
        if (memBorrowWaitTimeOutExecutor == nullptr) {
            UBSE_LOG_ERROR << "Get memBorrowWaitTimeOutExecutor is nullptr";
            return UBSE_ERROR_NULLPTR;
        }
        memBorrowWaitTimeOutExecutor->Execute([req, type] { SendRpcRequestForReturn(req, type); });
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "End to wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId
                  << ", request_id=" << req.requestId;
    resp = respFuture.get();
    return ret;
}
} // namespace ubse::mem::controller::agent