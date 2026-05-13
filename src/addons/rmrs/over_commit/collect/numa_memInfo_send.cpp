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

#include "numa_memInfo_send.h"
#include "ubse_com.h"
#include "ubse_logger.h"
#include "smap_query_process_send.h"

#include "mempooling_message.h"
#include "mp_configuration.h"
#include "response_info_simpo.h"

namespace mempooling::over_commit {
using namespace ubse::log;
using namespace ubse::com;
using namespace mempooling::message;

MpResult NumaMemInfoSend::SendMsg()
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[Overcommit][Collect] Send request start.";

    const UbseComEndpoint endpoint = {MP_MODULE_CODE, OPCODE_NUMA_MEM_INFO_COLLECT, nid_};
    UbseByteBuffer reqData{};
    auto ret = CreateRequestData(reqData);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Overcommit][Collect] Create request data failed.";
        return ret;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[Overcommit][Collect] Send request start.";
    ret = UbseRpcSend(endpoint, reqData, this, RespHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Overcommit][Collect] Send request failed.";
        return ret;
    }
    if (sendResult_ != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Overcommit][Collect] "
                                                             "Handle response failed."
                                                          << sendResult_ << ".";
        return sendResult_;
    }

    return MEM_POOLING_OK;
}

MpResult NumaMemInfoSend::CreateRequestData(UbseByteBuffer& reqData) const
{
    over_commit::NumaMemInfoCollectParam param = NumaMemInfoCollectParam{numaId_};
    RmrsOutStream builder;
    builder << param;
    auto mOutputRawData = builder.GetBufferPointer();
    if (mOutputRawData == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Overcommit][Collect] Get buffer pointer failed.";
        return MEM_POOLING_ERROR;
    }
    reqData = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = DefaultFreeFunc};
    return MEM_POOLING_OK;
}

void NumaMemInfoSend::RespHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Overcommit][Collect]"
                                                          << " Process response, invalid argument.";
        return;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[Overcommit][Collect] Process response data.";
    const auto numaMemInfoSend = static_cast<NumaMemInfoSend*>(ctx);
    if (resCode != MEM_POOLING_OK) {
        numaMemInfoSend->sendResult_ = resCode;
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Overcommit][Collect] Send msg failed," << resCode << ".";
        return;
    }
    ResponseInfoSimpo response{};
    RmrsInStream builder(respData.data, respData.len);
    builder >> response;
    auto [code, message] = response.GetResponseInfo();
    numaMemInfoSend->sendResult_ = code;
    numaMemInfoSend->resJson_ = message;
    if (code != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Overcommit][Collect] "
                                                             "Query numa mem Info failed, retCode="
                                                          << code << ".";
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[Overcommit][Collect] "
                                                            "Handle result="
                                                         << message << ".";
    }
}

} // namespace mempooling::over_commit