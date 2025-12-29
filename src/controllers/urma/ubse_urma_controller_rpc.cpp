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
#include <thread>
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_serial_util.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_manager.h"

namespace ubse::urmaController {
using namespace ubse::com;
using namespace ubse::utils;
using namespace ubse::serial;
using namespace ubse::election;
using namespace ubse::context;

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

} // namespace ubse::urmaController