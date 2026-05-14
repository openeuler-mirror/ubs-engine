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

#include "mp_sync_data_helper.h"

#include <map>

#include "ubse_logger.h"
#include "mp_configuration.h"

namespace mempooling::sync {
using namespace ubse::log;

MpSyncDataHelper& MpSyncDataHelper::Instance()
{
    static MpSyncDataHelper instance;
    return instance;
}

MpResult MpSyncDataHelper::Init()
{
    return MEM_POOLING_OK;
}

MpResult MpSyncDataHelper::DeInit()
{
    return MEM_POOLING_OK;
}

void GetSyncResultResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Ctx or respData is null.";
        return;
    }
    uint32_t& ret = *(static_cast<uint32_t*>(ctx));
    RmrsInStream builder(respData.data, respData.len);
    builder >> ret;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] ret=" << ret;
    return;
}

MpResult MpSyncDataHelper::SyncDataToNode(const std::vector<std::string>& nodeIdList, const uint32_t& opCode,
                                          const UbseByteBuffer& data)
{
    for (auto& nodeId : nodeIdList) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpElection][SyncData] SyncData to NodeId=" << nodeId << " start.";
        UbseComEndpoint endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = opCode, .address = nodeId};
        UbseByteBuffer reqData = data;
        uint32_t ret;
        uint32_t retRpc = UbseRpcSend(endpoint, reqData, &ret, GetSyncResultResHandler);
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpElection][SyncData] UbseRpcSend to NodeId=" << nodeId << " start.";
        if (retRpc != 0 || ret != 0) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpElection][SyncData] SyncData to NodeId=" << nodeId << " failed.";
            return MEM_POOLING_ERROR;
        }
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpElection][SyncData] SyncData to NodeId=" << nodeId << " finshed.";
    }
    return MEM_POOLING_OK;
}

} // namespace mempooling::sync