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

#include "ubse_urma_controller_rpc.h"
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <thread>
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_serial_util.h"
#include "ubse_str_util.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_uvs.h"
#include "ubse_urma_uvs_module.h"

namespace ubse::urmaController {
using namespace ubse::com;
using namespace ubse::utils;
using namespace ubse::serial;
using namespace ubse::election;
using namespace ubse::context;
using namespace ubse::nodeController;
using namespace ubse::urma;

const int URMA_NO2 = 2;

UrmaDevState ConvertUint32ToBondingState(uint32_t val)
{
    if (val == 1) {
        return UrmaDevState::ACTIVED;
    }
    if (val == URMA_NO2) {
        return UrmaDevState::INACTIVED;
    }
    return UrmaDevState::UNKNOWN;
}

UbseResult UbseUrmaQosReqSimpo::Serialize()
{
    UbseSerialization out;
    out << req.nodeId << req.urmaName;
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseUrmaQosReqSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> req.nodeId >> req.urmaName;
    return UBSE_OK;
}

UbseResult UbseUrmaQosRspSimpo::Serialize()
{
    UbseSerialization out;
    out << rsp.minBandWidth << rsp.maxBandWidth;
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseUrmaQosRspSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> rsp.minBandWidth >> rsp.maxBandWidth;
    return UBSE_OK;
}

UbseResult UbseUrmaQosMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                             UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseUrmaQosReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseUrmaQosRspSimpo>(rsp);
    UrmaQosRpcReq urmaQosReq = request->GetUbseUrmaQosReq();
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    UbseRoleInfo masterInfo{};
    UbseGetMasterInfo(masterInfo);

    /* 如果是本节点的消息就查询，如果是主节点就转发，其他情况丢弃 */
    if (std::to_string(urmaQosReq.nodeId) == currentNodeInfo.nodeId) {
        UrmaQosRpcRsp urmaQosRpcRsp;
        uint32_t ret = UrmaController::GetInstance().UbseUrmaBandWidthGet(
            urmaQosReq.urmaName, urmaQosRpcRsp.minBandWidth, urmaQosRpcRsp.maxBandWidth);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthGet failed," << FormatRetCode(ret);
            return UBSE_ERROR_SRCH;
        }
        response->SetUbseUrmaQosRsp(urmaQosRpcRsp);
        return UBSE_OK;
    } else if (masterInfo.nodeId == currentNodeInfo.nodeId) {
        SendParam sendParam{std::to_string(urmaQosReq.nodeId), static_cast<uint16_t>(UbseModuleCode::UBSE_URMA),
                            static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_QOS_QUERY)};
        auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
        if (comModule == nullptr) {
            UBSE_LOG_ERROR << "Getting ComModule failed.";
            return UBSE_ERROR_NULLPTR;
        }
        return comModule->RpcSend(sendParam, request, response);
    }
    return UBSE_OK;
}

uint16_t UbseUrmaQosMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_URMA);
}

uint16_t UbseUrmaQosMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_QOS_QUERY);
}

UbseResult UrmaDevQueryReqSimpo::Serialize()
{
    UbseSerialization out;
    out << req.nodeId << req.type;
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
    in >> req.nodeId >> req.type;
    return UBSE_OK;
}

UbseResult UrmaDevQueryRspSimpo::Serialize()
{
    UbseSerialization out;
    const uint32_t tmpSize = rsp.urmaInfos.size();
    out << tmpSize;
    for (size_t i = 0; i < tmpSize; i++) {
        const uint32_t tmpState = static_cast<uint32_t>(rsp.urmaInfos[i].state);
        out << rsp.urmaInfos[i].bondingName << rsp.urmaInfos[i].fe1Name << rsp.urmaInfos[i].fe2Name << tmpState;
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
    uint32_t tmpSize;
    in >> tmpSize;
    for (size_t i = 0; i < tmpSize; i++) {
        UbseUrmaInfoForQuery tmpInfo;
        in >> tmpInfo.bondingName >> tmpInfo.fe1Name >> tmpInfo.fe2Name;
        uint32_t tmpState;
        in >> tmpState;
        tmpInfo.state = ConvertUint32ToBondingState(tmpState);
        rsp.urmaInfos.push_back(tmpInfo);
    }
    return UBSE_OK;
}

UbseResult UbseUrmaDevQueryMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                  UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UrmaDevQueryReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UrmaDevQueryRspSimpo>(rsp);
    auto urmaReq = request->GetUbseUrmaDevReq();
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    UbseRoleInfo masterInfo{};
    UbseGetMasterInfo(masterInfo);

    /* 如果是本节点的消息就查询，如果是主节点就转发，其他情况丢弃 */
    if (std::to_string(urmaReq.nodeId) == currentNodeInfo.nodeId) {
        UrmaDevQueryRpcRsp rpcRsp;
        UbseUrmaControllerManager::GetInstance().GetUrmaNameForQueryByType(static_cast<UrmaDevType>(urmaReq.type),
                                                                           rpcRsp.urmaInfos);

        response->SetUbseUrmaDevQueryRsp(rpcRsp);
        return UBSE_OK;
    } else if (masterInfo.nodeId == currentNodeInfo.nodeId) {
        SendParam sendParam{std::to_string(urmaReq.nodeId), static_cast<uint16_t>(UbseModuleCode::UBSE_URMA),
                            static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_DEV_QUERY)};
        auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
        if (comModule == nullptr) {
            UBSE_LOG_ERROR << "Getting ComModule failed.";
            return UBSE_ERROR_NULLPTR;
        }
        return comModule->RpcSend(sendParam, request, response);
    }
    return UBSE_OK;
}

uint16_t UbseUrmaDevQueryMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_URMA);
}

uint16_t UbseUrmaDevQueryMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_DEV_QUERY);
}

UbseResult UbseUrmaNotifyReqSimpo::Serialize()
{
    UbseSerialization out;
    out << req.nodeId;
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseUrmaNotifyReqSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> req.nodeId;
    return UBSE_OK;
}

UbseResult UbseUrmaNotifyRspSimpo::Serialize()
{
    return UBSE_OK;
}

UbseResult UbseUrmaNotifyRspSimpo::Deserialize()
{
    return UBSE_OK;
}

UbseResult DoRpcQuery(const UbseRoleInfo &roleInfo, const std::string &dstId)
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
    UrmaQueryReq req{dstId};
    request->SetUrmaQueryReq(req);
    auto ret = comModule->RpcSend(sendParam, request, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Do rpc query failed.";
        return ret;
    }
    // check
    return UBSE_OK;
}

UbseResult DoQueryInfo(const std::string &dstId)
{
    UbseRoleInfo roleInfo;
    auto retCode = UbseGetMasterInfo(roleInfo);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseGetMasterInfo failed.";
        return retCode;
    }
    if (DoRpcQuery(roleInfo, dstId) != UBSE_OK) {
        return UBSE_ERROR;
    }
    auto urmaModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::urma::UbseUrmaUvsModule>();
    if (urmaModule == nullptr) {
        UBSE_LOG_ERROR << "Getting UrmaModule failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseGetCurrentNodeInfo(roleInfo);
    std::vector<UbseUrmaUvsNodeInfo> uvsInfos;
    UbseUrmaControllerManager::GetInstance().GetAllUvsInfo(uvsInfos);
    auto allLinkInfo = UbseNodeController::GetInstance().UbseGetDirConnectInfo(); // check
    auto curNode = UbseNodeController::GetInstance().GetCurNode();
    bool isAllPortDown = std::all_of(allLinkInfo.begin(), allLinkInfo.end(), [&curNode](const auto &linkInfo) {
        return linkInfo.second.slotId != curNode.slotId && linkInfo.second.peerSlotId != curNode.slotId;
    });
    if (isAllPortDown) {
        // 将该节点的所有urmaInfo状态改成Inactive
        UbseUrmaControllerManager::GetInstance().SetAllUrmaInfoToInactiveForNode(curNode.nodeId);
    }
    std::vector<PhysicalLink> reqVec;
    for (auto &linkInfo : allLinkInfo) {
        reqVec.push_back(linkInfo.second);
    }
    auto ret = urmaModule->SetUvsInfo(roleInfo.nodeId, reqVec, uvsInfos);
    for (auto devInfo : uvsInfos) {
        for (auto dev : devInfo.devList) {
            auto res = urmaModule->ActivateBondingDevice(dev.urmaDevEid);
            if (res == UBSE_OK) {
                UbseUrmaControllerManager::GetInstance().SetActiveState(dev.urmaDevEid, roleInfo.nodeId);
            }
            for (auto feInfo : dev.feList) {
                std::string urmaEidName;
                urmaModule->GetNameByUrmaEid(feInfo.primaryEid, urmaEidName);
                UbseUrmaControllerManager::GetInstance().SetFeName(feInfo.primaryEid, urmaEidName);
            }
        }
    }
    // check
    return UBSE_OK;
}

void DoUpdateUrmaInfo(const std::string &nodeId)
{
    if (!UbseUrmaControllerManager::GetInstance().IsUrmaInfoExists(nodeId)) {
        std::thread th([nodeId]() {
            if (DoQueryInfo(nodeId) == UBSE_OK) {
                return;
            }
        });
        th.detach();
    }
}

UbseResult UbseUrmaNotifyMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseUrmaNotifyReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseUrmaNotifyRspSimpo>(rsp);
    UrmaNotifyReq newReq = request->GetUrmaNotifyReq();
    response->SetErrCode(UBSE_OK);
    auto nodeList = UbseUrmaControllerManager::GetInstance().GetEmptyNodeInfo();
    for (auto nodeId : nodeList) {
        DoUpdateUrmaInfo(nodeId);
    }
    return UBSE_OK;
}

uint16_t UbseUrmaNotifyMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_URMA);
}

uint16_t UbseUrmaNotifyMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_URMA_INFO_NOTIFY);
}
// query

UbseResult UbseUrmaQueryReqSimpo::Serialize()
{
    UbseSerialization out;
    out << req.nodeId;
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
    in >> req.nodeId;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserilize urma query req";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaQueryRspSimpo::Serialize()
{
    UbseSerialization out;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize urma fe infos";
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
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserilize urma fe infos";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaQueryMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                               UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseUrmaQueryReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseUrmaQueryRspSimpo>(rsp);
    std::vector<UbseFeInfo> feInfos{};
    UbseUrmaControllerManager::GetInstance().GetFeInfoByNodeId(request->GetUrmaQueryReq().nodeId, feInfos);
    UrmaQueryRsp newRsp{.devInfo = std::move(feInfos)};
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
        UBSE_LOG_ERROR << "Failed to deserilize urma infos";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaReportUrmaNodeInfoRspSimpo::Serialize()
{
    return UBSE_OK;
}

UbseResult UbseUrmaReportUrmaNodeInfoRspSimpo::Deserialize()
{
    return UBSE_OK;
}

std::queue<std::string> GetWaitingNotifyNodes()
{
    std::queue<std::string> waitingNotifyNodes;
    auto nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    for (auto kv : nodeInfos) {
        auto nodeInfo = kv.second;
        if (!nodeInfo.nodeId.empty()) {
            waitingNotifyNodes.push(nodeInfo.nodeId);
        }
    }
    return waitingNotifyNodes;
}

UbseResult UbseUrmaAsyncNotifyUrmaInfoChange(const std::string &nodeId)
{
    UBSE_LOG_INFO << "Notify urma info changes for nodeId=" << nodeId;
    try {
        std::thread([nodeId]() {
            UbseBaseMessagePtr req = new (std::nothrow) UbseUrmaNotifyReqSimpo;
            if (req == nullptr) {
                UBSE_LOG_ERROR << "Failed to create rpc request";
                return;
            }
            auto ret = UbseUrmaAsyncNotifyAllNodes<UbseUrmaNotifyReqSimpo, UbseUrmaNotifyRspSimpo>(
                req, static_cast<uint16_t>(UbseModuleCode::UBSE_URMA),
                static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_URMA_INFO_NOTIFY));
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to notify all nodes, ret=" << ret;
                return;
            }
        }).detach();
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Failed to create thread, " << e.what();
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

UbseResult UbseUrmaReportUrmaNodeInfoMessageHandler::Handle(const UbseBaseMessagePtr &req,
                                                            const UbseBaseMessagePtr &rsp,
                                                            UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseUrmaReportUrmaNodeInfoReqSimpo>(req);
    auto nodeInfoReq = request->GetUbseUrmaNodeInfo();
    auto nodeId = nodeInfoReq.nodeId;
    auto &nodeInfo = nodeInfoReq.urmaNodeInfo;
    if (nodeId.empty()) {
        UBSE_LOG_ERROR << "node id is empty";
        rsp->SetErrCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    // 保存到全量列表中，待从节点获取
    UbseUrmaControllerManager::GetInstance().InsertNewNodeInfo(nodeId, nodeInfo);
    // 异步通知各节点nodeInfo变化
    if (auto ret = UbseUrmaAsyncNotifyUrmaInfoChange(nodeId); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to notify all nodes when urma info changes for nodeId=" << nodeId;
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

UbseResult ReportUrmaNodeInfoToMaster(const std::string &nodeId, UbseUrmaNodeInfo &&nodeInfo)
{
    // 向master节点上报本节点urma信息
    UbseUrmaReportUrmaNodeInfoReqSimpoPtr req = new (std::nothrow) UbseUrmaReportUrmaNodeInfoReqSimpo();
    UbseUrmaReportUrmaNodeInfoRspSimpoPtr rsp = new (std::nothrow) UbseUrmaReportUrmaNodeInfoRspSimpo();
    if (req == nullptr || rsp == nullptr) {
        UBSE_LOG_ERROR << "Failed to allocate memory for rpc req or rsp";
        return UBSE_ERROR;
    }
    ReportUrmaNodeInfoReq nodeInfoReq{.nodeId = nodeId, .urmaNodeInfo = std::move(nodeInfo)};
    req->SetUbseUrmaNodeInfo(nodeInfoReq);
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
} // namespace ubse::urmaController