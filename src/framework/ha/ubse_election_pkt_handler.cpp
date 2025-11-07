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

#include "ubse_election_pkt_handler.h"
#include "ubse_base_message.h"
#include "ubse_com_module.h"
#include "role/ubse_election_role_mgr.h"
#include "ubse_context.h"
#include "ubse_election_node_mgr.h"
#include "ubse_election_pkt_simpo.h"
#include "ubse_election_reply_pkt_simpo.h"

namespace ubse::election {
using namespace ubse::context;
using namespace ubse::message;
using namespace message;
using namespace ubse::log;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_ELECTION_MID)

UbseResult UbseElectionPktHandler::RegElectionPktHandler()
{
    UbseContext &ubseContext = UbseContext::GetInstance();
    auto ubseComModule = ubseContext.GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] get ubseComModule failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseComBaseMessageHandlerPtr ubseElectionPktHandler = new (std::nothrow) UbseElectionPktHandler();
    if (ubseElectionPktHandler == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] register ubseElectionPktHandler new failed";
        return UBSE_ERROR;
    }
    // 注册
    UbseResult ret =
        ubseComModule->RegRpcService<UbseElectionPktSimpo, UbseElectionReplyPktSimpo>(ubseElectionPktHandler);
    if (ret == UBSE_ERROR) {
        UBSE_LOG_ERROR << "[ELECTION] ubseElectionPktHandler RegRpc failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseElectionPktHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
    UbseComBaseMessageHandlerCtxPtr ctx)
{
    UbseElectionPktSimpoPtr request = UbseBaseMessage::DeConvert<UbseElectionPktSimpo>(req);
    if (request == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] new UbseElectionPktSimpo failed," << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    UbseElectionReplyPktSimpoPtr response = UbseBaseMessage::DeConvert<UbseElectionReplyPktSimpo>(rsp);
    if (response == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] new UbseElectionReplyPktSimpo failed," << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    ElectionPkt ubseRequest = request->GetElectionPkt();
    ElectionReplyPkt ubseResponse = response->GetElectionReplyPkt();
    UbseResult ret = RoleMgr::GetInstance().RecvPkt(ubseRequest.masterId, ubseRequest, ubseResponse);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] ElectionRecvPkt deal failed.";
    }
    response->SetResponse(ubseResponse);
    return UBSE_OK;
}
}