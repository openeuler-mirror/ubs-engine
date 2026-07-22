// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#include "ubse_mem_async_processor.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_dispatcher.h"
#include "ubse_thread_pool_module.h"
#include "message/ubse_mem_numa_borrow_req_simpo.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::task_executor;
UbseTaskExecutorPtr GetExecutor(const std::string& name)
{
    auto taskExecutor = ubse::context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        return nullptr;
    }
    auto resourceExecutor = taskExecutor->Get(name);
    if (resourceExecutor == nullptr) {
        return nullptr;
    }
    return resourceExecutor;
}
message::UbseResult AsyncMemShmBorrowProcessor(message::UbseMemShareBorrowReqSimpoPtr request)
{
    const auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR_NULLPTR;
    }
    // 使用线程池异步执行
    resourceExecutor->Execute([request]() {
        message::UbseMemOperationResp resp{};
        UbseMemShareBorrow(request.Get()->GetUbseMemShareBorrowReq(), resp);
    });
    return UBSE_OK;
}
message::UbseResult AsyncMemShmBorrowRespProcessor(message::UbseMemOperationRespSimpoPtr request)
{
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR_NULLPTR;
    }
    // 使用线程池异步执行
    resourceExecutor->Execute([request]() {
        auto resp = request.Get()->GetUbseMemOperationResp();
        UbseMemControllerDispatcher::MemShmBorrowRespDispatcher(resp);
    });
    return UBSE_OK;
}

message::UbseResult AsyncMemShmAttachProcessor(message::UbseMemShareAttachReqSimpoPtr request)
{
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR_NULLPTR;
    }
    // 使用线程池异步执行
    resourceExecutor->Execute([request]() {
        message::UbseMemOperationResp resp{};
        UbseMemShareAttach(request.Get()->GetUbseMemShareAttachReq(), resp);
    });
    return UBSE_OK;
}
message::UbseResult AsyncMemShmAttachRespProcessor(message::UbseMemOperationRespSimpoPtr request)
{
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR_NULLPTR;
    }
    // 使用线程池异步执行
    resourceExecutor->Execute([request]() {
        auto resp = request.Get()->GetUbseMemOperationResp();
        UbseMemControllerDispatcher::MemShmAttachRespDispatcher(resp);
    });
    return UBSE_OK;
}

// todo 入参
message::UbseResult AsyncMemShmDetachProcessor(message::UbseMemShareDetachReqSimpoPtr request,
                                               const std::string& realRequestNodeId)
{
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR_NULLPTR;
    }
    // 使用线程池异步执行
    resourceExecutor->Execute([request, realRequestNodeId]() {
        message::UbseMemOperationResp resp{};
        UbseMemShareDetach(request.Get()->GetUbseMemShareDetachReq(), resp, realRequestNodeId);
    });
    return UBSE_OK;
}
message::UbseResult AsyncMemShmDetachRespProcessor(message::UbseMemOperationRespSimpoPtr request)
{
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR_NULLPTR;
    }
    // 使用线程池异步执行
    resourceExecutor->Execute([request]() {
        auto resp = request.Get()->GetUbseMemOperationResp();
        UbseMemControllerDispatcher::MemShmDetachRespDispatcher(resp);
    });
    return UBSE_OK;
}

message::UbseResult AsyncMemShmReturnProcessor(message::UbseMemReturnReqSimpoPtr request,
                                               const std::string& realRequestNodeId)
{
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR_NULLPTR;
    }
    // 使用线程池异步执行
    resourceExecutor->Execute([request, realRequestNodeId]() {
        UbseMemOperationResp resp{};
        UbseMemShareReturn(request.Get()->GetUbseMemReturnReq(), resp, realRequestNodeId);
    });
    return UBSE_OK;
}
message::UbseResult AsyncMemCommonReturnRespProcessor(message::UbseMemOperationRespSimpoPtr request)
{
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR_NULLPTR;
    }
    // 使用线程池异步执行
    resourceExecutor->Execute([request]() {
        auto resp = request.Get()->GetUbseMemOperationResp();
        UbseMemControllerDispatcher::MemReturnRespDispatcher(resp);
    });
    return UBSE_OK;
}

UbseResult DoNumaBorrowAsync(const UbseMemNumaBorrowReq& request)
{
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    resourceExecutor->Execute([request]() {
        UbseMemOperationResp resp{};
        UbseMemNumaBorrow(request, resp);
    });
    return UBSE_OK;
}

UbseResult DoNumaBorrowRespAsync(const UbseMemOperationResp& resp)
{
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    resourceExecutor->Execute([resp]() { UbseMemControllerDispatcher::UbseMemNumaBorrowRespHandler(resp); });
    return UBSE_OK;
}

UbseResult DoReturnAsync(const UbseMemReturnReq& request, const std::string& realRequestNodeId)
{
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    resourceExecutor->Execute([request, realRequestNodeId]() {
        UbseMemOperationResp resp{};
        UbseMemNumaReturn(request, resp, realRequestNodeId);
    });
    return UBSE_OK;
}

UbseResult DoReturnRespAsync(const UbseMemOperationResp& resp)
{
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    resourceExecutor->Execute([resp]() { UbseMemControllerDispatcher::UbseMemNumaReturnRespHandler(resp); });
    return UBSE_OK;
}

} // namespace ubse::mem::controller