// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#ifndef UBSE_MANAGER_UBSE_MEM_CONTROLLER_DISPATCHER_H
#define UBSE_MANAGER_UBSE_MEM_CONTROLLER_DISPATCHER_H
#include "ubse_ipc_server.h"

#include "message/ubse_mem_operation_resp_simpo.h"
#include "message/ubse_mem_return_req_simpo.h"
#include "message/ubse_mem_share_attach_req_simpo.h"
#include "message/ubse_mem_share_borrow_req_simpo.h"
#include "message/ubse_mem_share_detach_req_simpo.h"
#include "ubse_mem_controller_def.h"
#include "ubse_api_server_module.h"

namespace ubse::mem::controller {
using namespace ubse::ipc;
using namespace message;
class UbseMemControllerDispatcher {
public:
    static UbseMemControllerDispatcher &GetInstance()
    {
        static UbseMemControllerDispatcher instance;
        return instance;
    }

    static UbseResult RegisterSdkDispatcher();
    // shm resp
    static uint32_t MemShmBorrowRespDispatcher(UbseMemOperationResp &resp);
    static uint32_t MemShmAttachRespDispatcher(UbseMemOperationResp &resp);
    static uint32_t MemShmDetachRespDispatcher(UbseMemOperationResp &resp);
    static uint32_t MemReturnRespDispatcher(UbseMemOperationResp &resp);
    // numa resp
    static UbseResult UbseMemNumaBorrowRespHandler(const UbseMemOperationResp &resp);
    static UbseResult UbseMemNumaReturnRespHandler(const UbseMemOperationResp &resp);

private:
    // shm
    static uint32_t MemShmCreateDispatcher(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t MemShmCreateDispatcherWithAffinity(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t MemShmCreateDispatcherWithLender(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t MemShmAttachDispatcher(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t MemShmGetDispatcher(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t MemShmListDispatcher(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t MemShmDetachDispatcher(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t MemShmReturnDispatcher(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t MemShmMemFaultGet(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t MemShmListWithPrefixDispatcher(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t UbseMemShmGetMemIdByImportDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context);

    // fd
    static uint32_t UbseMemFdBorrowDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t UbseMemFdBorrowWithLenderDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t UbseMemFdBorrowWithCandidate(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t UbseMemFdReturnDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t UbseMemFdPermissionDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t UbseMemFdGetDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t UbseMemFdListDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t UbseMemFdGetMemIdByImportDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context);

    // numa
    static uint32_t UbseMemNumaCreateHandler(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t UbseMemNumaCreateWithLender(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t UbseMemNumaBorrowWithCandidate(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t UbseMemNumaDelete(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t UbseMemNumaGetDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t UbseMemNumaListDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t UbseMemNumaGetMemIdByImportDispatch(const UbseIpcMessage &buffer,
                                                        const UbseRequestContext &context);

    // cli
    static uint32_t UbseMemNodeBorrowInfoDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context);
    static uint32_t UbseMemNodeLendInfoDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context);

private:
    UbseResult GetMasterAndLocalNodeId(std::string &masterNodeId, std::string &localNodeId);
    UbseResult BufferToShmBorrowReq(const UbseIpcMessage &buffer, UbseMemShareBorrowReqSimpoPtr &reqSimpo,
                                    const std::string &requestNodeId, const UbseRequestContext &context,
                                    bool withAffinity);
    UbseResult BufferToShmAttachReq(const UbseIpcMessage &buffer, UbseMemShareAttachReqSimpoPtr &reqSimpo,
                                    std::string &requestNodeId, const UbseRequestContext &context);
    static UbseResult BufferToShmGetReq(const UbseIpcMessage &buffer, std::string &name);
    UbseResult BufferToShmStatusGetReq(const UbseIpcMessage &buffer, std::string &name);
    UbseResult BufferToShmDetachReq(const UbseIpcMessage &buffer, UbseMemShareDetachReqSimpoPtr &reqSimpo,
                                    std::string &requestNodeId, const UbseRequestContext &context);
    UbseResult BufferToShmReturnReq(const UbseIpcMessage &buffer, UbseMemReturnReqSimpoPtr &reqSimpo,
                                    std::string &requestNodeId, const UbseRequestContext &context);
    UbseResult ShmDispatcherToShmReq(const def::UbseMemShmDispatcher &memShmDispatcher,
                                     UbseMemShareBorrowReq &shareBorrowReq);
    static UbseResult RegisterFdSdkDispatcherCreate();
    static UbseResult RegisterFdSdkDispatcherDelete();
    static UbseResult RegisterFdSdkDispatcherQuery();
    static UbseResult RegisterFdSdkDispatcher();
    static UbseResult RegisterNumaSdkDispatcherCreate();
    static UbseResult RegisterNumaSdkDispatcherDelete();
    static UbseResult RegisterNumaSdkDispatcherQuery();
    static UbseResult RegisterNumaSdkDispatcher();
    static UbseResult RegisterShmSdkDispatcherCreate();
    static UbseResult RegisterShmSdkDispatcherDelete();
    static UbseResult RegisterShmSdkDispatcherQuery();
    static UbseResult RegisterShmSdkDispatcher();
    static UbseResult RegisterCliDispatcher();
    static uint32_t UbseMemNumaBorrowRpc(UbseMemNumaBorrowReq &req, const UbseRequestContext &context);
    static uint32_t UbseMemFdBorrowRpc(UbseMemFdBorrowReq &req, const UbseRequestContext &context);
};
} // namespace ubse::mem::controller
#endif // UBSE_MANAGER_UBSE_MEM_CONTROLLER_DISPATCHER_H