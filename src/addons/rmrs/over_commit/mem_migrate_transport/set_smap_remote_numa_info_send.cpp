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

#include "set_smap_remote_numa_info_send.h"

#include "ubse_com.h"
#include "ubse_logger.h"

#include "mempooling_message.h"
#include "mp_configuration.h"
#include "response_info_simpo.h"
#include "set_smap_remote_numa_info_trans_msg.h"

namespace mempooling::over_commit {
using namespace ubse::log;
using namespace ubse::com;
using namespace mempooling::message;

MpResult SetSmapRemoteNumaInfoSend::SendMsg()
{
    const UbseComEndpoint endpoint = {MP_MODULE_CODE, OPCODE_SET_SMAP_REMOTE_NUMA_INFO, srcMemoryBorrowParam_.srcNid};
    UbseByteBuffer reqData{};
    auto ret = CreateRequestData(reqData);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][SetSmapRemoveNumaInfo] "
                                                             "Create request data failed";
        return ret;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][SetSmapRemoveNumaInfo] Send request start.";
    ret = UbseRpcSend(endpoint, reqData, this, RespHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][SetSmapRemoveNumaInfo] "
                                                             "Send request failed.";
        return ret;
    }
    if (sendResult_ != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemReturn][SetSmapRemoveNumaInfo] Handle response failed," << sendResult_ << ".";
        return sendResult_;
    }

    return MEM_POOLING_OK;
}

MpResult SetSmapRemoteNumaInfoSend::CreateRequestData(UbseByteBuffer& reqData) const
{
    // 生成overCommitMemMigrateTransMsg simpo对象并序列化
    auto setSmapRemoteNumaInfoMsg = SetSmapRemoteNumaInfoTransMsg(
        SetSmapRemoteNumaInfoTrans({.srcMemoryBorrowParam = srcMemoryBorrowParam_, .memBorrowInfos = memBorrowInfos_}));

    RmrsOutStream builder;
    builder << setSmapRemoteNumaInfoMsg;
    // 生成rpc消息
    reqData.data = builder.GetBufferPointer();
    if (reqData.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemReturn][SetSmapRemoveNumaInfo] Get buffer pointer failed.";
        return MEM_POOLING_ERROR;
    }
    reqData.len = builder.GetSize();
    reqData.freeFunc = DefaultFreeFunc;
    return MEM_POOLING_OK;
}

void SetSmapRemoteNumaInfoSend::RespHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][SetSmapRemoveNumaInfo] Process response data.";
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemReturn][SetSmapRemoveNumaInfo] Ctx or respData is null.";
        return;
    }
    const auto setSmapRemoteNumaInfoSend = static_cast<SetSmapRemoteNumaInfoSend*>(ctx);
    if (resCode != MEM_POOLING_OK) {
        setSmapRemoteNumaInfoSend->sendResult_ = resCode;
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemReturn][SetSmapRemoveNumaInfo] Send msg failed, ret=" << resCode << ".";
        return;
    }

    ResponseInfoSimpo response{};
    RmrsInStream builder(respData.data, respData.len);
    builder >> response;
    auto [code, message] = response.GetResponseInfo();
    setSmapRemoteNumaInfoSend->sendResult_ = code;
    if (code != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][OSTurboSmap][SetSmapRemoveNumaInfo] "
                                                             "Set smap remote numa info failed, retCode="
                                                          << code << ".";
    } else {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][OSTurboSmap][SetSmapRemoveNumaInfo] "
                                                             "Handle result="
                                                          << response.ToString() << ".";
    }
}
} // namespace mempooling::over_commit