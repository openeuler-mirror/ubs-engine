/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "smap_query_process_send.h"
#include "ubse_com.h"
#include "ubse_logger.h"

#include "mempooling_message.h"
#include "mp_configuration.h"
#include "response_info_simpo.h"
#include "smap_remote_process_query_trans_msg.h"

namespace mempooling::over_commit {
using namespace ubse::log;
using namespace ubse::com;
using namespace mempooling::message;

MpResult SmapQueryProcessSend::SendMsg()
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][SmapQuery] Send request start.";

    const UbseComEndpoint endpoint = {MP_MODULE_CODE, OPCODE_SMAP_PROCESS_QUERY, srcMemoryBorrowParam_.srcNid};
    UbseByteBuffer reqData{};
    auto ret = CreateRequestData(reqData);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][SmapQuery] Create request data failed";
        return ret;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][SmapQuery] Send request start.";
    ret = UbseRpcSend(endpoint, reqData, this, RespHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][SmapQuery] Send request failed";
        return ret;
    }
    if (sendResult_ != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][SmapQuery] "
                                                          "Handle response failed, result="
                                                       << sendResult_ << ".";
        return sendResult_;
    }

    return MEM_POOLING_OK;
}

MpResult SmapQueryProcessSend::CreateRequestData(UbseByteBuffer &reqData) const
{
    auto smapRemoteProcessQuryMsg = SmapRemoteProcessQueryTransMsg(SmapRemoteProcessQueryTrans({.numaIds = numaIds_}));
    RmrsOutStream builder;
    builder << smapRemoteProcessQuryMsg;
    // 生成rpc消息
    reqData.data = builder.GetBufferPointer();
    if (reqData.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][SmapQuery] Get buffer pointer failed";
        return MEM_POOLING_ERROR;
    }
    reqData.len = builder.GetSize();
    reqData.freeFunc = DefaultFreeFunc;
    return MEM_POOLING_OK;
}

void SmapQueryProcessSend::RespHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][SmapQuery]"
                                                             " Process response, invalid params.";
        return;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][SmapQuery] Process response data.";
    const auto smapQueryProcessSend = static_cast<SmapQueryProcessSend *>(ctx);
    if (resCode != MEM_POOLING_OK) {
        smapQueryProcessSend->sendResult_ = resCode;
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemReturn][SmapQuery] Send msg failed, ret=" << resCode << ".";
        return;
    }

    ResponseInfoSimpo response{};
    RmrsInStream builder(respData.data, respData.len);
    builder >> response;

    auto [code, message] = response.GetResponseInfo();
    smapQueryProcessSend->sendResult_ = code;
    smapQueryProcessSend->pidJson_ = message;
    if (code != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][OSTurboSmap][SmapQuery]"
                                                          " Smap query failed, retCode="
                                                       << code << ".";
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][OSTurboSmap][SmapQuery]"
                                                         " Handle result="
                                                      << response.ToString() << ".";
    }
}

} // namespace mempooling::over_commit
