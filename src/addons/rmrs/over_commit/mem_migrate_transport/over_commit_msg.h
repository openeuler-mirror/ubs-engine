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

#ifndef OVER_COMMIT_MSG_H
#define OVER_COMMIT_MSG_H

#include "ubse_def.h"
#include "mempooling_message.h"
#include "over_commit_fault_memid_module.h"

namespace mempooling::over_commit {

class OverCommitMsg {
public:
    static MpResult GetVmNumaInfoMapRpc(std::string importNodeId,
                                        std::vector<VmNumaInfoWithSocket>& vmNumaInfoWithSocketList,
                                        uint16_t remoteNumaId);

    static MpResult GetVmNumaInfoMapLocal(std::vector<VmNumaInfoWithSocket>& vmNumaInfoWithSocketList,
                                          uint16_t localNumaId);

    static MpResult GetVmNumaInfoMapRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);

    static MpResult GetLocalNumaVms(uint16_t remoteNumaId, std::vector<VmNumaInfoWithSocket>& vmNumaInfoWithSocketList);

    static void GetVmNumaInfoMapResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);

    static MpResult NumaMemInfoCollectRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);

    static MpResult SyncBindTypeDataRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);

    // RPC: 向指定节点查询 NumaBindType
    static MpResult GetNumaBindTypeRpc(const std::string& targetNodeId, const std::string& queryNodeId,
                                       GetNumaBindTypeResult& result);

    // RPC 接收处理函数
    static MpResult GetNumaBindTypeRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);

    // RPC 响应处理函数
    static void GetNumaBindTypeResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);

    static void SetResponse(ResponseInfoSimpo& response, const MpResult& retCode, const std::string& msg,
                            UbseByteBuffer& resBuffer);

    inline static void DefaultFreeFunc(const uint8_t* data)
    {
        delete[] data;
    }

private:
    static MpResult SyncDataToStandByNode(ResponseInfoSimpo& response, const UbseByteBuffer& req, UbseByteBuffer& resp,
                                          const std::string& currentNodeId);
};

} // namespace mempooling::over_commit

#endif // OVER_COMMIT_MSG_H