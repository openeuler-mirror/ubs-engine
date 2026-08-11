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

#ifndef MEMPOOLING_OVER_COMMIT_PID_FAULT_HANDLER_H
#define MEMPOOLING_OVER_COMMIT_PID_FAULT_HANDLER_H

#include "ubse_def.h"

namespace mempooling::over_commit {

/**
 * @brief PID粒度故障处理 - 借入节点侧 RPC Handler
 *
 * 运行在借入节点(agent)上，接收借出节点(master)的RPC请求:
 * - PidQueryRecvHandler: 查询本节点上使用故障远端numa的PID分布
 * - PidExecuteRecvHandler: 执行借用→迁移→归还流程
 */
class PidFaultHandler {
public:
    // PID查询: 接收故障numa列表，返回本节点上的PID内存分布
    static uint32_t PidQueryRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
    static void PidQueryResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);

    // PID执行: 接收执行请求，在本节点执行借用→迁移→归还
    static uint32_t PidExecuteRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
    static void PidExecuteResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
};

} // namespace mempooling::over_commit

#endif // MEMPOOLING_OVER_COMMIT_PID_FAULT_HANDLER_H
