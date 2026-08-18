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
#include "ubse_thread_pool.h"
#include "mp_error.h"
#include "response_info_simpo.h"
namespace mempooling {
struct SimplifiedFaultRecordsInNode;
namespace over_commit {
class OverCommitFaultManagementHandler {
public:
    static uint32_t GetVmNumaInfoMapRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
    static void GetVmNumaInfoMapResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
    // 执行大页配置、setRemoteNumaInfo
    static uint32_t MemIdExecuteRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
    static void MemIdExecuteResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
    // 执行迁回与归还
    static uint32_t MemIdReturnExecuteRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
    static void MemIdReturnExecuteResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
    // 执行直接归还
    static uint32_t MemIdReturnDirectlyExecuteRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
    static void MemIdReturnDirectlyExecuteResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
    // 停止pid冷热流动
    static uint32_t DisableSmapProcessMigrateRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
    static void DisableSmapProcessMigrateResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
    // 开启pid冷热流动
    static uint32_t EnableSmapProcessMigrateRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
    static void EnableSmapProcessMigrateResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
    // 处理涉及故障借出节点的借入节点
    static uint32_t FaultNumaProcessRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
    static void FaultNumaProcessResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);

    static uint32_t FaultHandleMemBorrowRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
    static void FaultHandleMemBorrowResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
    // 处理涉及故障借出节点的借入节点（简化流程）
    static uint32_t SimplifiedFaultNumaProcessRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
    static void SimplifiedFaultNumaProcessResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
};

// 简化故障处理线程池单例：构造时按配置（rmrs.fault.simplified）创建，进程生命周期内复用
class SimplifiedFaultTaskExecutor {
public:
    static SimplifiedFaultTaskExecutor& Instance()
    {
        static SimplifiedFaultTaskExecutor instance;
        return instance;
    }

    // 获取简化故障处理线程池（配置未开启或创建失败时为 nullptr）
    const ubse::task_executor::UbseTaskExecutorPtr& GetTaskExecutor() const
    {
        return taskExecutor_;
    }

private:
    SimplifiedFaultTaskExecutor();
    ~SimplifiedFaultTaskExecutor() = default;
    SimplifiedFaultTaskExecutor(const SimplifiedFaultTaskExecutor&) = delete;
    SimplifiedFaultTaskExecutor& operator=(const SimplifiedFaultTaskExecutor&) = delete;

    ubse::task_executor::UbseTaskExecutorPtr taskExecutor_;
};
// 借入节点侧简化故障处理：按占用升序、进程级并行（线程池并行度 4）
MpResult ProcessSimplifiedFaultPids(const SimplifiedFaultRecordsInNode& records,
                                    const ubse::task_executor::UbseTaskExecutorPtr& taskExecutor);
} // namespace over_commit
} // namespace mempooling

#endif // MEMPOOLING_OVER_COMMIT_FAULT_MANAGEMENT_HANDLER_H
