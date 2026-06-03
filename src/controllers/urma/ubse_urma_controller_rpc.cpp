/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_urma_controller_rpc.h"
#include <cstdint>
#include "ubse_com_module.h"
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_logger.h"
#include "ubse_serial_util.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_controller_util.h"
#include "adapter_plugins/urma/ubse_urma_uvs.h"

namespace ubse::urmaController {
using namespace ubse::com;
using namespace ubse::utils;
using namespace ubse::serial;
using namespace ubse::election;
using namespace ubse::context;
using namespace ubse::nodeController;
using namespace ubse::urma;
using namespace ubse::urmaController;
using namespace ubse::common::def;

UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UrmaDevQueryReqSimpo::Serialize()
{
    UbseSerialization out;
    out << req.nodeId;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize req";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UrmaDevQueryReqSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> req.nodeId;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize req";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UrmaDevQueryRspSimpo::Serialize()
{
    UbseSerialization out;
    const uint32_t tmpSize = rsp.urmaInfos.size();
    out << rsp;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize UrmaDevQueryRspSimpo";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UrmaDevQueryRspSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> rsp;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize UrmaDevQueryRspSimpo";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaDevQueryMessageHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                                  UbseComBaseMessageHandlerCtxPtr ctx)
{
    AsyncHandlerGuard cntGuard;
    if (g_globalStop) {
        UBSE_LOG_INFO << "Stop urma controller, ignore message";
        return UBSE_OK;
    }
    auto request = UbseBaseMessage::DeConvert<UrmaDevQueryReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UrmaDevQueryRspSimpo>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert message";
        return UBSE_ERROR;
    }
    auto urmaReq = request->GetUbseUrmaDevReq();
    UbseRoleInfo currentNodeInfo{};
    UbseRoleInfo masterInfo{};
    if (UbseGetMasterInfo(masterInfo) != UBSE_OK || UbseGetCurrentNodeInfo(currentNodeInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get node info";
        return UBSE_ERROR;
    }
    UrmaDevQueryRpcRsp rpcRsp;
    /* 如果是本节点的消息就查询，如果是主节点就转发，其他情况丢弃 */
    if (std::to_string(urmaReq.nodeId) == currentNodeInfo.nodeId) {
        UbseUrmaController::GetInstance().GetLocalUrmaDevs(rpcRsp.urmaInfos);
        rpcRsp.result = UBSE_OK;
        response->SetUbseUrmaDevQueryRsp(rpcRsp);
        return UBSE_OK;
    } else if (masterInfo.nodeId == currentNodeInfo.nodeId) {
        SendParam sendParam{std::to_string(urmaReq.nodeId), static_cast<uint16_t>(UbseModuleCode::UBSE_URMA),
                            static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_DEV_QUERY)};
        auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
        if (comModule == nullptr) {
            UBSE_LOG_ERROR << "Getting ComModule failed.";
            rpcRsp.result = UBSE_ERROR_NULLPTR;
            response->SetUbseUrmaDevQueryRsp(rpcRsp);
            return UBSE_ERROR_NULLPTR;
        }
        auto ret = comModule->RpcSend(sendParam, request, response);
        if (ret != UBSE_OK) {
            rpcRsp.result = ret;
            response->SetUbseUrmaDevQueryRsp(rpcRsp);
            return ret;
        }
    }
    return UBSE_ERROR_INVAL;
}

uint16_t UbseUrmaDevQueryMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_URMA);
}

uint16_t UbseUrmaDevQueryMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_DEV_QUERY);
}

UbseResult UbseUrmaBrocastReqSimpo::Serialize()
{
    UbseSerialization out;
    out << req;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize brocast req";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseUrmaBrocastReqSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> req;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize brocast req";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaBrocastRspSimpo::Serialize()
{
    UbseSerialization out;
    out << mErrCode;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize broadcast rsp";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseUrmaBrocastRspSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> mErrCode;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize broadcast rsp";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult QueryUrmaInfoFromMaster(const UbseRoleInfo& roleInfo, std::vector<std::string>& updateNodeIds)
{
    SendParam sendParam{roleInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_URMA),
                        static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_URMA_INFO_QUERY)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseUrmaQueryReqSimpoPtr request = new (std::nothrow) UbseUrmaQueryReqSimpo();
    UbseUrmaQueryRspSimpoPtr response = new (std::nothrow) UbseUrmaQueryRspSimpo();
    if (request == nullptr || response == nullptr) {
        return UBSE_ERROR;
    }
    QueryUrmaInfoReq req{updateNodeIds};
    request->SetUrmaQueryReq(req);
    auto ret = comModule->RpcSend(sendParam, request, response);
    if (ret != UBSE_OK || response->GetErrCode() != UBSE_OK) {
        UBSE_LOG_ERROR << "Do rpc query failed, ret=" << ret << ", response code=" << response->GetErrCode();
        return ret;
    }
    auto rsp = response->GetUbseUrmaQueryRsp();
    for (auto& nodeInfo : rsp.queryNodeInfos) {
        UbseUrmaControllerManager::GetInstance().InsertNewNodeInfo(nodeInfo.nodeId, nodeInfo);
    }
    return UBSE_OK;
}

UbseResult DoUpdateUrmaInfos(std::vector<std::string> updateNodeIds)
{
    AsyncHandlerGuard cntGuard;
    if (g_globalStop) {
        UBSE_LOG_INFO << "Urma controller is stopped, ignore msg";
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "Start to query urma info";
    UbseRoleInfo masterRoleInfo;
    if (auto ret = UbseGetMasterInfo(masterRoleInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseGetMasterInfo failed, ret=" << ret;
        return ret;
    }
    // 如果节点状态不是最新，更新节点信息
    if (auto ret = QueryUrmaInfoFromMaster(masterRoleInfo, updateNodeIds); ret != UBSE_OK) {
        UBSE_LOG_WARN << "QueryUrmaInfoFromMaster failed.";
        return ret;
    }
    // 下发拓扑
    auto curNode = UbseNodeController::GetInstance().GetCurNode();
    std::vector<UbseUrmaUvsNodeInfo> uvsInfos;
    UbseUrmaControllerManager::GetInstance().GetAllUvsTopoInfo(uvsInfos);
    if (auto ret = UbseUrmaControllerSetUvsInfo(curNode.nodeId, GetDirConnectInfo(), uvsInfos); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to set uvs info, ret=" << ret;
        return ret;
    }
    // 尝试从urma恢复bonding设备
    UbseUrmaController::GetInstance().FillUrmaDevsByUvsInfo(curNode.nodeId, uvsInfos);
    bool isAllPortDown = false;
    if (auto ret = QueryAllPortsDown(isAllPortDown); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to query all ports status, ret=" << ret;
        return ret;
    }
    if (isAllPortDown) {
        // 将该节点的所有urmaInfo状态改成Inactive
        UBSE_LOG_INFO << "All ports are down for nodeId=" << curNode.nodeId << ", set all URMA info to PORT_DOWN";
        UbseUrmaControllerManager::GetInstance().SetAllUrmaDevStateForNode(UrmaDevState::PORT_DOWN);
    }
    UBSE_LOG_INFO << "End to update urma info";
    return UBSE_OK;
}

UbseResult PostUpdateUrmaInfosTask(const std::map<std::string, uint64_t>& urmaInfoTimestamps)
{
    UBSE_LOG_INFO << "Start to update urma info";
    std::vector<std::string> updateNodeIds; // 存储urma info有更新的节点
    static std::atomic<uint64_t> globalTimeStampUpdateId{1};
    uint64_t timeStampUpdateId{0};
    for (auto& kv : urmaInfoTimestamps) {
        auto nodeId = kv.first;
        auto brocastTimeStamp = kv.second;
        if (g_globalStop) {
            UBSE_LOG_INFO << "Urma controller is stopped, ignore msg";
            return UBSE_OK;
        }
        if (brocastTimeStamp > UbseUrmaControllerManager::GetInstance().GetUrmaUpdateTimeStamp(nodeId)) {
            updateNodeIds.emplace_back(nodeId);
        }
        timeStampUpdateId = globalTimeStampUpdateId.fetch_add(1);
    }

    static std::mutex postUpdateUrmaInfosTaskMtx;
    std::lock_guard<std::mutex> lock(postUpdateUrmaInfosTaskMtx);
    if (timeStampUpdateId < globalTimeStampUpdateId - 1) {
        UBSE_LOG_INFO << "Urma info has been updated, ignore this task";
        return UBSE_OK;
    }
    auto taskExecutor = ubse::context::UbseContext::GetInstance().GetModule<task_executor::UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get task executor failed";
        return UBSE_ERROR_NULLPTR;
    }
    std::string executorName = "UrmaExecutor";
    auto urmaExecutor = taskExecutor->Get(executorName);
    if (urmaExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get task executor for urma failed";
        return UBSE_ERROR_NULLPTR;
    }
    urmaExecutor->Execute([updateNodeIds, executorName]() {
        auto task = [updateNodeIds]() {
            return DoUpdateUrmaInfos(updateNodeIds);
        };
        std::string taskName = "UrmaUpdateUrmaInfoRetryTimer";
        HandleTaskWithRetry(executorName, taskName, NO_10, task);
    });
    return UBSE_OK;
}

UbseResult UbseUrmaNotifyMessageHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                                UbseComBaseMessageHandlerCtxPtr ctx)
{
    UBSE_LOG_INFO << "Receive urma notification, start to handle";
    AsyncHandlerGuard cntGuard;
    if (g_globalStop) {
        UBSE_LOG_INFO << "Urma controller is stopped, ignore msg";
        return UBSE_OK;
    }
    auto request = UbseBaseMessage::DeConvert<UbseUrmaBrocastReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseUrmaBrocastRspSimpo>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert rpc message";
        return UBSE_ERROR;
    }
    UrmaBrocastReq newReq = request->GetUrmaNotifyReq();
    response->SetErrCode(UBSE_OK);
    if (auto ret = PostUpdateUrmaInfosTask(newReq.urmaInfoTimestamps); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to update urma info, ret=" << ret;
        response->SetErrCode(ret);
        return ret;
    }
    return UBSE_OK;
}

uint16_t UbseUrmaNotifyMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_URMA);
}

uint16_t UbseUrmaNotifyMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_URMA_INFO_BROADCAST);
}
// query

UbseResult UbseUrmaQueryReqSimpo::Serialize()
{
    UbseSerialization out;
    out << req.updateNodeIds;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize urma query req";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseUrmaQueryReqSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> req.updateNodeIds;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize urma query req";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaQueryRspSimpo::Serialize()
{
    UbseSerialization out;
    out << rsp.queryNodeInfos;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize urma node infos";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseUrmaQueryRspSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> rsp.queryNodeInfos;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize urma fe infos";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaQueryMessageHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                               UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseUrmaQueryReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseUrmaQueryRspSimpo>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert rpc message";
        return UBSE_ERROR;
    }
    auto queryNodeIds = request->GetUrmaQueryReq().updateNodeIds;
    std::vector<UbseUrmaNodeInfo> updateUrmaInfos;
    response->SetErrCode(UBSE_OK);
    for (const auto& nodeId : queryNodeIds) {
        if (g_globalStop) {
            UBSE_LOG_INFO << "Urma controller is stopped, ignore msg";
            return UBSE_OK;
        }
        auto nodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo(nodeId);
        updateUrmaInfos.push_back(nodeInfo);
    }
    QueryUrmaInfoRsp newRsp{.queryNodeInfos = std::move(updateUrmaInfos)};
    response->SetUbseQueryRsp(newRsp);
    return UBSE_OK;
}

uint16_t UbseUrmaQueryMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_URMA);
}

uint16_t UbseUrmaQueryMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_URMA_INFO_QUERY);
}

UbseResult UbseUrmaReportUrmaNodeInfoReqSimpo::Serialize()
{
    UbseSerialization out;
    out << urmaNodeInfo;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize urma infos";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseUrmaReportUrmaNodeInfoReqSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> urmaNodeInfo;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize urma infos";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaReportUrmaNodeInfoRspSimpo::Serialize()
{
    UbseSerialization out;
    out << mErrCode;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize urma infos";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseUrmaReportUrmaNodeInfoRspSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> mErrCode;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize urma infos";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaAsyncNotifyOneNodeUrmaInfoChange(const std::string& notifyNodeId)
{
    AsyncHandlerGuard cntGuard;
    if (g_globalStop) {
        UBSE_LOG_INFO << "Urma controller is stopped, ignore msg";
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "Brocast urma info timestamp to nodeId=" << notifyNodeId;
    UbseUrmaBrocastReqPtr req = new (std::nothrow) UbseUrmaBrocastReqSimpo;
    UbseUrmaBrocastRspPtr rsp = new (std::nothrow) UbseUrmaBrocastRspSimpo;
    if (req == nullptr || rsp == nullptr) {
        UBSE_LOG_ERROR << "Failed to create rpc message";
        return UBSE_ERROR;
    }
    auto nodes = UbseNodeController::GetInstance().GetAllNodes();
    // 确认待通知的节点是否在集群中，避免定时给不存在的节点发信息
    if (nodes.find(notifyNodeId) == nodes.end()) {
        UBSE_LOG_WARN << "nodeId=" << notifyNodeId << " is not in cluster, will stop brocast urma info";
        return UBSE_OK; // 返回UBSE_OK，结束定时任务
    }
    std::map<std::string, uint64_t> urmaInfoTimestamps;
    for (auto& node : nodes) {
        if (g_globalStop) {
            UBSE_LOG_INFO << "Urma controller is stopped, ignore msg";
            return UBSE_OK;
        }
        auto timeStamp = UbseUrmaControllerManager::GetInstance().GetUrmaUpdateTimeStamp(node.second.nodeId);
        urmaInfoTimestamps[node.second.nodeId] = timeStamp;
    }

    UrmaBrocastReq brocastReq{.urmaInfoTimestamps = std::move(urmaInfoTimestamps)};
    req->SetUrmaNotifyReq(brocastReq);
    SendParam sendParam{notifyNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_URMA),
                        static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_URMA_INFO_BROADCAST)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get com module";
        return UBSE_ERROR;
    }
    if (auto ret = comModule->RpcSend(sendParam, req, rsp); ret != UBSE_OK || rsp->GetErrCode() != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send rpc message, ret=" << ret << ", " << FormatRetCode(rsp->GetErrCode());
        if (ret == UBSE_OK) {
            return rsp->GetErrCode();
        }
        return ret;
    }
    return UBSE_OK;
}

UbseResult BrocastUrmaInfoTask(const std::string& nodeId)
{
    UBSE_LOG_INFO << "Brocast urma info timestamp to nodeId=" << nodeId;
    if (auto ret = UbseUrmaAsyncNotifyOneNodeUrmaInfoChange(nodeId); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to brocast urma info timestamp to nodeId=" << nodeId;
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaAsyncBrocastUrmaInfo()
{
    std::string executorName = "UrmaExecutor";
    auto taskExecutor = ubse::context::UbseContext::GetInstance().GetModule<task_executor::UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get task executor failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto urmaExecutor = taskExecutor->Get(executorName);
    if (urmaExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get task executor for urma failed";
        return UBSE_ERROR_NULLPTR;
    }

    auto nodes = UbseNodeController::GetInstance().GetAllNodes();
    for (auto& node : nodes) {
        if (g_globalStop) {
            return UBSE_OK;
        }
        urmaExecutor->Execute([executorName, nodeId = node.second.nodeId]() {
            UBSE_LOG_INFO << "Brocast urma info timestamp to nodeId=" << nodeId;
            std::string taskName = "UrmaMasterBrocastRetryTimer_" + nodeId;
            auto task = [nodeId]() {
                return BrocastUrmaInfoTask(nodeId);
            };
            // 广播事件设置的定时器时间为10s
            HandleTaskWithRetry(executorName, taskName, NO_10, task);
        });
    }

    return UBSE_OK;
}

UbseResult UbseUrmaReportUrmaNodeInfoMessageHandler::Handle(const UbseBaseMessagePtr& req,
                                                            const UbseBaseMessagePtr& rsp,
                                                            UbseComBaseMessageHandlerCtxPtr ctx)
{
    AsyncHandlerGuard cntGuard;
    if (g_globalStop) {
        UBSE_LOG_INFO << "Urma controller is stopped, ignore msg";
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "Handling URMA report node info message";
    auto request = UbseBaseMessage::DeConvert<UbseUrmaReportUrmaNodeInfoReqSimpo>(req);
    if (request == nullptr || rsp == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert rpc message";
        return UBSE_ERROR;
    }
    auto nodeInfoReq = request->GetUbseUrmaNodeInfo();
    auto changeNodeId = nodeInfoReq.nodeId;
    auto& nodeInfo = nodeInfoReq.urmaNodeInfo;
    if (changeNodeId.empty() || nodeInfo.nodeId.empty()) {
        UBSE_LOG_ERROR << "node id is empty, changeNodeId=" << changeNodeId << ", nodeId=" << nodeInfo.nodeId;
        rsp->SetErrCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    // 保存到全量列表中，待其它节点获取
    UbseUrmaControllerManager::GetInstance().InsertNewNodeInfo(changeNodeId, nodeInfo);
    // 异步通知各节点nodeInfo变化
    if (auto ret = UbseUrmaAsyncBrocastUrmaInfo(); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to brocast urma info timestamps";
        rsp->SetErrCode(UBSE_ERROR);
        return ret;
    }
    rsp->SetErrCode(UBSE_OK);
    return UBSE_OK;
}

uint16_t UbseUrmaReportUrmaNodeInfoMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_URMA_INFO_REPORT);
}

uint16_t UbseUrmaReportUrmaNodeInfoMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_URMA);
}

UbseResult ReportUrmaNodeInfoToMaster(const std::string& nodeId)
{
    UBSE_LOG_INFO << "Report urma node info to master, nodeId=" << nodeId;
    // 向master节点上报本节点urma信息
    UbseUrmaReportUrmaNodeInfoReqSimpoPtr req = new (std::nothrow) UbseUrmaReportUrmaNodeInfoReqSimpo();
    UbseUrmaReportUrmaNodeInfoRspSimpoPtr rsp = new (std::nothrow) UbseUrmaReportUrmaNodeInfoRspSimpo();
    if (req == nullptr || rsp == nullptr) {
        UBSE_LOG_ERROR << "Failed to allocate memory for rpc req or rsp";
        return UBSE_ERROR;
    }
    auto urmaNodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo(nodeId);
    ReportUrmaNodeInfoReq nodeInfoReq{.nodeId = nodeId, .urmaNodeInfo = std::move(urmaNodeInfo)};
    req->SetUbseUrmaNodeInfo(std::move(nodeInfoReq));
    UbseRoleInfo masterInfo{};
    if (auto ret = UbseGetMasterInfo(masterInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get master info, " << FormatRetCode(ret);
        return ret;
    }
    SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_URMA),
                        static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_URMA_INFO_REPORT)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed";
        return UBSE_ERROR_NULLPTR;
    }
    if (auto ret = comModule->RpcSend(sendParam, req, rsp); ret != UBSE_OK || rsp->GetErrCode() != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to do rpc send, ret=" << ret << ", " << FormatRetCode(rsp->GetErrCode());
        return ret;
    }
    return UBSE_OK;
}

UbseResult GetCurNodeIdAndMasterNodeId(std::string& curNodeId, std::string& masterNodeId)
{
    UbseRoleInfo currentNodeInfo{};
    if (UbseGetCurrentNodeInfo(currentNodeInfo) != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get current node info";
        return UBSE_ERROR_AGAIN;
    }
    curNodeId = currentNodeInfo.nodeId;
    UbseRoleInfo masterInfo{};
    if (UbseGetMasterInfo(masterInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get master info";
        return UBSE_ERROR_AGAIN;
    }
    masterNodeId = masterInfo.nodeId;
    return UBSE_OK;
}
} // namespace ubse::urmaController