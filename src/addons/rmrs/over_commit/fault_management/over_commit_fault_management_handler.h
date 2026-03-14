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

#ifndef MEMPOOLING_OVER_COMMIT_FAULT_MANAGEMENT_HANDLER_H
#define MEMPOOLING_OVER_COMMIT_FAULT_MANAGEMENT_HANDLER_H
#include "ubse_def.h"
#include "response_info_simpo.h"
namespace mempooling::over_commit {
class OverCommitFaultManagementHandler {
public:
    // 获取节点上的VM numa info
    static uint32_t GetVmNumaInfoMapRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);
    static void GetVmNumaInfoMapResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode);
    // 执行大页配置、setRemoteNumaInfo
    static uint32_t MemIdExecuteRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);
    static void MemIdExecuteResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode);
    // 执行大页配置、setRemoteNumaInfo
    static uint32_t MemIdReturnExecuteRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);
    static void MemIdReturnExecuteResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode);
    // 处理涉及故障借出节点的借入节点
    static uint32_t FaultNumaProcessRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);
    static void FaultNumaProcessResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode);
};
} // namespace mempooling::over_commit

#endif // MEMPOOLING_OVER_COMMIT_FAULT_MANAGEMENT_HANDLER_H
