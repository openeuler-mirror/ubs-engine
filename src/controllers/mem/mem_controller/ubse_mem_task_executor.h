/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_MEM_TASK_EXECUTOR_H
#define UBSE_MEM_TASK_EXECUTOR_H

#include <memory>
#include <string>

#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_mem_controller_api_agent.h"
#include "ubse_request_id_util.h"
#include "ubse_str_util.h"

namespace ubse::mem::controller {
using namespace ubse::mem::controller::agent;

// 内存操作接口定义
class UbseMemOperation {
public:
    virtual ~UbseMemOperation() = default;

    // 纯虚函数
    virtual const char *GetOperationName() const = 0;
    virtual uint32_t Execute(UbseMemOperationResp &resp) = 0;
    virtual bool BuildResponseData(UbseIpcMessage &responseMessage) = 0;

    // 通用属性访问（所有 req 都继承自 UbseMemBaseBorrowReq）
    virtual const std::string &GetName() const = 0;
    virtual const std::string &GetRequestNodeId() const = 0;
    virtual uint64_t GetRequestId() const = 0;
};

// 内存操作模板中间层
template <typename ReqType>
class UbseBaseMemOperation : public UbseMemOperation {
public:
    explicit UbseBaseMemOperation(ReqType &&req) : req_(std::move(req)) {}

    // 统一实现通用属性访问
    const std::string &GetName() const override
    {
        return req_.name;
    }

    const std::string &GetRequestNodeId() const override
    {
        return req_.requestNodeId;
    }

    uint64_t GetRequestId() const override
    {
        return req_.requestId;
    }

protected:
    ReqType req_; // 子类可以直接访问 req_
};

// Numa Create 操作
class UbseMemNumaCreateOperation : public UbseBaseMemOperation<UbseMemNumaBorrowReq> {
public:
    using UbseBaseMemOperation::UbseBaseMemOperation;

    const char *GetOperationName() const override;
    uint32_t Execute(UbseMemOperationResp &resp) override;
    bool BuildResponseData(UbseIpcMessage &responseMessage) override;

private:
    UbseMemOperationResp resp;
};

// Fd Create 操作
class UbseMemCliFdCreateDispatch : public UbseBaseMemOperation<UbseMemFdBorrowReq> {
public:
    using UbseBaseMemOperation::UbseBaseMemOperation;

    const char *GetOperationName() const override;
    uint32_t Execute(UbseMemOperationResp &resp) override;
    bool BuildResponseData(UbseIpcMessage &responseMessage) override;

private:
    UbseMemOperationResp resp;
};

// Create 操作
class UbseMemShmCreateOperation : public UbseBaseMemOperation<UbseMemShareBorrowReq> {
public:
    using UbseBaseMemOperation::UbseBaseMemOperation;

    const char *GetOperationName() const override;
    uint32_t Execute(UbseMemOperationResp &resp) override;
    bool BuildResponseData(UbseIpcMessage &responseMessage) override;
};

// Attach 操作
class UbseMemShmAttachOperation : public UbseBaseMemOperation<UbseMemShareAttachReq> {
public:
    using UbseBaseMemOperation::UbseBaseMemOperation;

    const char *GetOperationName() const override;
    uint32_t Execute(UbseMemOperationResp &resp) override;
    bool BuildResponseData(UbseIpcMessage &responseMessage) override;
};

// Detach 操作
class UbseMemShmDetachOperation : public UbseBaseMemOperation<UbseMemShareDetachReq> {
public:
    using UbseBaseMemOperation::UbseBaseMemOperation;

    const char *GetOperationName() const override;
    uint32_t Execute(UbseMemOperationResp &resp) override;
    bool BuildResponseData(UbseIpcMessage &responseMessage) override;
};

// 任务执行器
class UbseMemTaskExecutor {
public:
    static uint32_t ExecuteOperationAsync(uint64_t requestId, std::unique_ptr<UbseMemOperation> operation);

    static uint32_t PrepareRequest(const UbseRequestContext &context, UbseMemBaseBorrowReq &req);

private:
    static void ExecuteTask(std::shared_ptr<UbseMemOperation> operation, uint64_t requestId);
};

template <typename OpType, typename ReqType>
uint32_t ExecuteOperationAsync(const UbseRequestContext &context, ReqType &&req)
{
    auto ret = UbseMemTaskExecutor::PrepareRequest(context, req);
    if (ret != UBSE_OK) {
        return ret;
    }

    // 使用 std::make_unique 创建具体的 Operation 实例
    return UbseMemTaskExecutor::ExecuteOperationAsync(context.requestId, std::make_unique<OpType>(std::move(req)));
}
} // namespace ubse::mem::controller
#endif // RACK_MANAGER_UBSE_MEM_TASK_EXECUTOR_H
