/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_mem_controller_dispatcher.h"

#include "message/ubse_mem_controller_def_serial.h"
#include "message/ubse_mem_fd_borrow_req_simpo.h"
#include "ubs_engine_mem.h"
#include "ubse_com_op_code.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_api.h"
#include "ubse_mem_async_processor.h"
#include "ubse_mem_buffer_convert.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_mem_rpc_processor.h"
#include "ubse_mem_util.h"
#include "ubse_mem_sign_verifier.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_controller.h"
#include "ubse_str_util.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::adapter_plugins::mmi;
using namespace message;
using namespace ubse::election;
using namespace api::server;
using namespace ubse::nodeController;
using namespace ubse::node::api;
using namespace ubse::mem::util;

const std::string MEM_FD_PERMISSION = "mem.fd";
const std::string MEM_NUMA_PERMISSION = "mem.numa";
const std::string MEM_SHM_PERMISSION = "mem.shm";
template <class TReq>
UbseResult SendToMasterIfNotMaster(std::string &masterNodeId, TReq &requestPtr, uint16_t moduleCode, uint16_t opCode)
{
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    SendParam sendParam{masterNodeId, moduleCode, opCode};
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ubseResponsePtr.";
        return UBSE_ERROR_NULLPTR;
    }
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RpcSend(sendParam, requestPtr, ubseResponsePtr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to Send to master, " << FormatRetCode(ret);
        return ret;
    }

    UBSE_LOG_INFO << "Success to Send to master";
    return ret;
}

UbseResult UbseMemControllerDispatcher::RegisterFdBorrowSdkDispatcher(
    const std::shared_ptr<UbseApiServerModule> &apiServer)
{
    auto ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_FD_CREATE), UbseMemFdBorrowDispatch, MEM_FD_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of UbseMemFdBorrowDispatch IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM), static_cast
        <uint16_t>(UbseMemOpCode::UBSE_MEM_FD_WITH_LEND_INFO), UbseMemFdBorrowWithLenderDispatch, MEM_FD_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of UbseMemFdBorrowWithLenderDispatch IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM), static_cast
        <uint16_t>(UbseMemOpCode::UBSE_MEM_FD_CREATE_WITH_CANDIDATE), UbseMemFdBorrowWithCandidate, MEM_FD_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of UbseMemFdBorrowWithCandidate IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseMemControllerDispatcher::RegisterFdSdkDispatcher()
{
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<api::server::UbseApiServerModule>();
    if (apiServer == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = RegisterFdBorrowSdkDispatcher(apiServer);
    if (ret != UBSE_OK) {
        return ret;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_FD_DELETE),
        UbseMemFdReturnDispatch, MEM_FD_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of UbseMemFdReturnDispatch IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_FD_PERMISSION), UbseMemFdPermissionDispatch, MEM_FD_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of UbseMemFdPermissionDispatch IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_FD_GET), UbseMemFdGetDispatch, MEM_FD_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of UbseMemFdGetDispatch IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_FD_LIST), UbseMemFdListDispatch, MEM_FD_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of UbseMemFdListDispatch IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseMemControllerDispatcher::RegisterNumaSdkDispatcher()
{
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<api::server::UbseApiServerModule>();
    if (apiServer == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_NUMA_CREATE),
        UbseMemNumaCreateHandler, MEM_NUMA_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of UbseMemNumaCreateHandler IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_NUMA_WITH_LEND_INFO),
        UbseMemNumaCreateWithLender, MEM_NUMA_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of UbseMemNumaCreateWithLender IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_NUMA_CREATE_WITH_CANDIDATE),
        UbseMemNumaBorrowWithCandidate, MEM_NUMA_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of UbseMemNumaBorrowWithCandidate IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_NUMA_DELETE),
        UbseMemNumaDelete, MEM_NUMA_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of UbseMemNumaDelete IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_NUMA_GET),
        UbseMemNumaGetDispatch, MEM_NUMA_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of UbseMemNumaGetDispatch IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_NUMA_LIST),
        UbseMemNumaListDispatch, MEM_NUMA_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of UbseMemNumaListDispatch IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseMemControllerDispatcher::RegisterShmSdkDispatcher()
{
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<api::server::UbseApiServerModule>();
    if (apiServer == nullptr) {
        return UBSE_ERROR;
    }
    auto ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                             static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_SHM_CREATE),
                                             MemShmCreateDispatcher, MEM_SHM_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of MemShmCreateDispatcher IPC-API failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_SHM_CREATE_WITH_AFFINITY),
                                        MemShmCreateDispatcherWithAffinity, MEM_SHM_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of MemShmCreateWithAffinityDispatcher IPC-API failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_SHM_CREATE_WITH_LENDER),
                                        MemShmCreateDispatcherWithLender, MEM_SHM_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of MemShmCreateDispatcherWithLender IPC-API failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_SHM_ATTACH),
                                        MemShmAttachDispatcher, MEM_SHM_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of MemShmAttachDispatcher IPC-API failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_SHM_DETACH),
                                        MemShmDetachDispatcher, MEM_SHM_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of MemShmDetachDispatcher IPC-API failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_SHM_DELETE),
                                        MemShmReturnDispatcher, MEM_SHM_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of MemShmReturnDispatcher IPC-API failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return RegisterShmQuerySdkDispatcher();
}

UbseResult UbseMemControllerDispatcher::RegisterShmQuerySdkDispatcher()
{
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<api::server::UbseApiServerModule>();
    if (apiServer == nullptr) {
        return UBSE_ERROR;
    }
    auto ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                             static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_SHM_GET),
                                             MemShmGetDispatcher, MEM_SHM_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of MemShmGetDispatcher IPC-API failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_SHM_LIST), MemShmListDispatcher,
                                        MEM_SHM_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of MemShmListDispatcher IPC-API failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_SHM_LIST_WITH_PREFIX),
                                        MemShmListWithPrefixDispatcher, MEM_SHM_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of MemShmListWithPrefixDispatcher IPC-API failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_SHM_MEMID_STATUS_GET),
                                        MemShmMemFaultGet, MEM_SHM_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of MemShmMemFaultGet IPC-API failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemControllerDispatcher::RegisterCliDispatcher()
{
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<api::server::UbseApiServerModule>();
    if (apiServer == nullptr) {
        return UBSE_ERROR;
    }
    auto ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                             static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_CLI_NODE_BORROW),
                                             UbseMemNodeBorrowInfoDispatch);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of NodeBorrowInfo IPC-API failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    ret = apiServer->RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                        static_cast<uint16_t>(UbseMemOpCode::UBSE_MEM_CLI_NODE_LEND),
                                        UbseMemNodeLendInfoDispatch);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of NodeLendInfo IPC-API failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemControllerDispatcher::RegisterSdkDispatcher()
{
    auto ret = RegisterShmSdkDispatcher();
    if (ret != UBSE_OK) {
        return ret;
    }
    ret = RegisterNumaSdkDispatcher();
    if (ret != UBSE_OK) {
        return ret;
    }
    ret = RegisterCliDispatcher();
    if (ret != UBSE_OK) {
        return ret;
    }
    return RegisterFdSdkDispatcher();
}

inline void SetBaseReqInfo(UbseMemBaseBorrowReq &req, const UbseRequestContext &context)
{
    req.requestId = context.requestId;
    req.udsInfo = GenUdsInfo(context);
}

inline void fillShmBorrowReqFlag(UbseMemShareBorrowReq &shareBorrowReq, uint64_t flag)
{
    // ubseMemPrivData adTrOchip赋默认值1和cacheableFlag赋默认值0
    shareBorrowReq.ubseMemPrivData.onePth = 1;
    shareBorrowReq.ubseMemPrivData.wrDelayComp = 0;
    shareBorrowReq.ubseMemPrivData.reduceDelayComp = 0;
    shareBorrowReq.ubseMemPrivData.cmoDelayComp = 0;
    shareBorrowReq.ubseMemPrivData.so = 0;
    shareBorrowReq.ubseMemPrivData.adTrOchip = 1;
    shareBorrowReq.ubseMemPrivData.cacheableFlag = 0;
    shareBorrowReq.shmAnonymous = false;

    if ((flag & UBS_MEM_FLAG_NO_WR_DELAY) != 0) {
        shareBorrowReq.ubseMemPrivData.wrDelayComp = 1;
        shareBorrowReq.ubseMemPrivData.reduceDelayComp = 1;
        shareBorrowReq.ubseMemPrivData.cmoDelayComp = 1;
    }
    if ((flag & UBS_MEM_FLAG_SHM_ANONYMOUS) != 0) {
        shareBorrowReq.shmAnonymous = true;
    }
    if ((flag & UBS_MEM_FLAG_CACHEABLE) != 0) {
        shareBorrowReq.ubseMemPrivData.cacheableFlag = 1;
    }
}

UbseResult UbseMemControllerDispatcher::ShmDispatcherToShmReq(const def::UbseMemShmDispatcher &memShmDispatcher,
                                                              UbseMemShareBorrowReq &shareBorrowReq)
{
    shareBorrowReq.name = memShmDispatcher.name;
    shareBorrowReq.size = memShmDispatcher.size;
    // 填充flag
    fillShmBorrowReqFlag(shareBorrowReq, memShmDispatcher.flag);
    error_t cpyRet =
        memcpy_s(shareBorrowReq.usrInfo, UBSE_MAX_USR_INFO_LEN, memShmDispatcher.usrInfo, UBSE_MAX_USR_INFO_LEN);
    if (cpyRet != EOK) {
        UBSE_LOG_ERROR << "usrInfo cpy failed, ret:" << cpyRet;
        return UBSE_ERROR;
    }
    shareBorrowReq.shmRegion.nodeNum = memShmDispatcher.shmRegion.nodeCnt;
    for (int i = 0; i < shareBorrowReq.shmRegion.nodeNum; i++) {
        ubse::adapter_plugins::mmi::UbseNodeInfo ubseNodeInfo;
        ubseNodeInfo.index = memShmDispatcher.shmRegion.slotIds[i];
        const std::string shmNodeId = std::to_string(memShmDispatcher.shmRegion.slotIds[i]);
        ubseNodeInfo.nodeId = shmNodeId;
        shareBorrowReq.shmRegion.nodelist.push_back(ubseNodeInfo);
    }

    // 回填provider
    for (int i = 0; i < memShmDispatcher.provider.nodeCnt; i++) {
        shareBorrowReq.providerList.push_back(std::to_string(memShmDispatcher.provider.slotIds[i]));
    }
    // 回填affinitySocketId
    shareBorrowReq.withAffinity.affinitySocketId = memShmDispatcher.affinitySocketId;
    return UBSE_OK;
}
UbseResult UbseMemControllerDispatcher::BufferToShmBorrowReq(const UbseIpcMessage &buffer,
                                                             UbseMemShareBorrowReqSimpoPtr &reqSimpo,
                                                             const std::string &requestNodeId,
                                                             const UbseRequestContext &context, bool withAffinity)
{
    def::UbseMemShmDispatcher shmDispatcher;
    uint32_t ret = UBSE_OK;
    if (withAffinity) {
        ret = UbseMemShmCreateWithAffinityReqUnpack(buffer, shmDispatcher);
    } else {
        ret = UbseMemShmCreateReqUnpack(buffer, shmDispatcher);
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "shm create req unpack failed, " << FormatRetCode(ret) + ", requestId=";
        return ret;
    }
    UbseMemShareBorrowReq shareBorrowReq{};
    shareBorrowReq.requestNodeId = requestNodeId;
    SetBaseReqInfo(shareBorrowReq, context);
    ret = ShmDispatcherToShmReq(shmDispatcher, shareBorrowReq);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "ShmDispatcherToShmReq failed, " << FormatRetCode(ret) + ", requestId=";
        return ret;
    }

    reqSimpo = new (std::nothrow) UbseMemShareBorrowReqSimpo();
    if (reqSimpo == nullptr) {
        UBSE_LOG_ERROR << "new obj failed.";
        return UBSE_ERROR_NULLPTR;
    }
    reqSimpo->SetUbseMemShareBorrowReq(shareBorrowReq);
    return UBSE_OK;
}
UbseResult UbseMemControllerDispatcher::BufferToShmAttachReq(const UbseIpcMessage &buffer,
                                                             UbseMemShareAttachReqSimpoPtr &reqSimpo,
                                                             std::string &requestNodeId,
                                                             const UbseRequestContext &context)
{
    UbseMemShareAttachReq shareAttachReq{};
    auto ret = UbseMemShmAttachReqUnpack(buffer, shareAttachReq);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "shm create req unpack failed, " << FormatRetCode(ret) + ", requestId=";
        return ret;
    }
    shareAttachReq.requestNodeId = requestNodeId;
    shareAttachReq.importNodeId = requestNodeId;
    SetBaseReqInfo(shareAttachReq, context);
    reqSimpo = new (std::nothrow) UbseMemShareAttachReqSimpo();
    if (reqSimpo == nullptr) {
        UBSE_LOG_ERROR << "new obj failed.";
        return UBSE_ERROR_NULLPTR;
    }
    reqSimpo->SetUbseMemShareAttachReq(shareAttachReq);
    return UBSE_OK;
}
UbseResult UbseMemControllerDispatcher::BufferToShmGetReq(const UbseIpcMessage &buffer, std::string &name)
{
    if (const auto ret = UbseMemShmGetReqUnpack(buffer, name); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "shm get req unpack failed, " << FormatRetCode(ret) + ", requestId=";
        return ret;
    }
    return UBSE_OK;
}
UbseResult UbseMemControllerDispatcher::BufferToShmStatusGetReq(const UbseIpcMessage &buffer, std::string &name)
{
    if (const auto ret = UbseMemShmtatusGetReqUnPack(buffer, name); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "shm status get req unpack failed, " << FormatRetCode(ret) + ", requestId=";
        return ret;
    }
    return UBSE_OK;
}
UbseResult UbseMemControllerDispatcher::BufferToShmDetachReq(const UbseIpcMessage &buffer,
                                                             UbseMemShareDetachReqSimpoPtr &reqSimpo,
                                                             std::string &requestNodeId,
                                                             const UbseRequestContext &context)
{
    UbseMemShareDetachReq shareDetachReq{};
    auto ret = UbseMemShmDetachReqUnpack(buffer, shareDetachReq);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "shm create req unpack failed, " << FormatRetCode(ret) + ", requestId=";
        return ret;
    }
    shareDetachReq.requestNodeId = requestNodeId;
    shareDetachReq.unImportNodeId = requestNodeId;
    SetBaseReqInfo(shareDetachReq, context);
    reqSimpo = new (std::nothrow) UbseMemShareDetachReqSimpo();
    if (reqSimpo == nullptr) {
        UBSE_LOG_ERROR << "new obj failed.";
        return UBSE_ERROR_NULLPTR;
    }
    reqSimpo->SetUbseMemShareDetachReq(shareDetachReq);
    return UBSE_OK;
}
UbseResult UbseMemControllerDispatcher::BufferToShmReturnReq(const UbseIpcMessage &buffer,
                                                             UbseMemReturnReqSimpoPtr &reqSimpo,
                                                             std::string &requestNodeId,
                                                             const UbseRequestContext &context)
{
    UbseMemReturnReq shareRetrunReq{};
    if (auto ret = UbseMemShmDeleteReqUnpack(buffer, shareRetrunReq); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "shm create req unpack failed, " << FormatRetCode(ret) + ", requestId=";
        return ret;
    }
    shareRetrunReq.requestNodeId = requestNodeId;
    SetBaseReqInfo(shareRetrunReq, context);
    shareRetrunReq.uid = context.clientInfo.uid;
    shareRetrunReq.gid = context.clientInfo.gid;
    reqSimpo = new (std::nothrow) UbseMemReturnReqSimpo();
    if (reqSimpo == nullptr) {
        UBSE_LOG_ERROR << "new obj failed.";
        return UBSE_ERROR_NULLPTR;
    }
    reqSimpo->SetUbseMemReturnReq(shareRetrunReq);
    return UBSE_OK;
}
uint32_t UbseMemControllerDispatcher::MemShmBorrowRespDispatcher(UbseMemOperationResp &resp)
{
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<api::server::UbseApiServerModule>();
    if (apiServer == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage message{};
    // 结构转换
    const auto ret = apiServer->SendResponse(resp.errorCode, resp.requestId, message);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send response, response errorcode=" << resp.errorCode << ", requestId="
                       << resp.requestId;
    }
    return ret;
}
uint32_t UbseMemControllerDispatcher::MemShmAttachRespDispatcher(UbseMemOperationResp &resp)
{
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<api::server::UbseApiServerModule>();
    if (apiServer == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage message{};
    // 根据name回查账本
    def::UbseMemShmDesc shmDesc{};
    auto ret = UbseMemShmGetByNodeId(resp.name, shmDesc, resp.requestNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "failed to get shm desc, " << FormatRetCode(ret);
    }
    // 结构转换
    UbseMemShmAttachResponsePack(shmDesc, message);
    ret = apiServer->SendResponse(resp.errorCode, resp.requestId, message);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send response, response errorcode=" << resp.errorCode << ", requestId="
                       << resp.requestId;
    }
    delete[] message.buffer;
    message.buffer = nullptr;
    return ret;
}
uint32_t UbseMemControllerDispatcher::MemShmDetachRespDispatcher(UbseMemOperationResp &resp)
{
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<api::server::UbseApiServerModule>();
    if (apiServer == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    // 取requestrId， 发给ApiServer
    UbseIpcMessage message{};
    // 结构转换
    return apiServer->SendResponse(resp.errorCode, resp.requestId, message);
}
uint32_t UbseMemControllerDispatcher::MemReturnRespDispatcher(UbseMemOperationResp &resp)
{
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<api::server::UbseApiServerModule>();
    if (apiServer == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    // 取requestrId， 发给ApiServer
    UbseIpcMessage message{};
    // 结构转换
    return apiServer->SendResponse(resp.errorCode, resp.requestId, message);
}
uint32_t UbseMemControllerDispatcher::MemShmCreateDispatcher(const UbseIpcMessage &buffer,
                                                             const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "shm create dispatcher, requestId=" << context.requestId;
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetInstance().GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }

    // buffer 转结构
    UbseMemShareBorrowReqSimpoPtr reqSimpoPtr{};
    ret = GetInstance().BufferToShmBorrowReq(buffer, reqSimpoPtr, localNodeId, context, false);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to convert buffer, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return ret;
    }

    if (IsHighSafety()) {
        auto req = reqSimpoPtr->GetUbseMemShareBorrowReq();
        if (const auto res =
                UbseMemSignVerifier::Sign("share", req.trustRingData.reqSignedData, req.trustRingData.trustRingId);
            res != UBSE_OK) {
            UBSE_LOG_ERROR << "Sign for request failed, " << FormatRetCode(res);
            return res;
        }
        reqSimpoPtr->SetUbseMemShareBorrowReq(req);
    }
    // 不是master调用RPC异步发送
    if (localNodeId != masterNodeId) {
        ret = SendToMasterIfNotMaster(masterNodeId, reqSimpoPtr, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                                      static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW));
    } else {
        // 是master，切换线程，提交给新线程
        ret = AsyncMemShmBorrowProcessor(reqSimpoPtr);
    }

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to send request, " << FormatRetCode(ret) + ", requestId=" << context.requestId;
        return ret;
    }
    UBSE_LOG_INFO << "shm create dispatcher send success, requestId=" << context.requestId;
    return UBSE_OK;
}

uint32_t UbseMemControllerDispatcher::MemShmCreateDispatcherWithAffinity(const UbseIpcMessage &buffer,
                                                                         const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "shm create with affinity dispatcher, requestId=" << context.requestId;
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetInstance().GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }

    // buffer 转结构
    UbseMemShareBorrowReqSimpoPtr reqSimpoPtr{};
    ret = GetInstance().BufferToShmBorrowReq(buffer, reqSimpoPtr, localNodeId, context, true);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to convert buffer, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return ret;
    }

    // 开启指定CPU平面进行创建
    auto req = reqSimpoPtr->GetUbseMemShareBorrowReq();
    req.withAffinity.enableCreateWithAffinity = true;
    req.withAffinity.createReqNodeId = localNodeId;
    if (IsHighSafety()) {
        if (const auto res =
                UbseMemSignVerifier::Sign("share", req.trustRingData.reqSignedData, req.trustRingData.trustRingId);
            res != UBSE_OK) {
            UBSE_LOG_ERROR << "Sign for request failed, " << FormatRetCode(res);
            return res;
            }
    }
    reqSimpoPtr->SetUbseMemShareBorrowReq(req);
    // 不是master调用RPC异步发送
    if (localNodeId != masterNodeId) {
        ret = SendToMasterIfNotMaster(masterNodeId, reqSimpoPtr, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                                      static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW));
    } else {
        // 是master，切换线程，提交给新线程
        ret = AsyncMemShmBorrowProcessor(reqSimpoPtr);
    }

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to send request, " << FormatRetCode(ret) + ", requestId=" << context.requestId;
        return ret;
    }
    UBSE_LOG_INFO << "shm create with affinity dispatcher send success, requestId=" << context.requestId;
    return UBSE_OK;
}

uint32_t UbseMemControllerDispatcher::MemShmCreateDispatcherWithLender(const UbseIpcMessage &buffer,
                                                                       const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "shm create with lender dispatcher, requestId=" << context.requestId;
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetInstance().GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }

    // buffer 转结构
    UbseMemShareBorrowReq req{};
    uint64_t flag;
    ret = UbseMemShmCreateWithLenderReqUnpack(buffer, req, flag);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemShmCreateWithLenderReqUnpack unpack failed, " << FormatRetCode(ret);
        return ret;
    }
    fillShmBorrowReqFlag(req, flag);
    req.requestNodeId = localNodeId;
    SetBaseReqInfo(req, context);

    UbseMemShareBorrowReqSimpoPtr reqSimpoPtr = new (std::nothrow) UbseMemShareBorrowReqSimpo();
    if (reqSimpoPtr == nullptr) {
        UBSE_LOG_ERROR << "reqSimpoPtr is nullptr";
        return UBSE_ERROR_NULLPTR;
    }
    reqSimpoPtr->SetUbseMemShareBorrowReq(req);
    if (IsHighSafety()) {
        if (const auto res =
            UbseMemSignVerifier::Sign("share", req.trustRingData.reqSignedData, req.trustRingData.trustRingId);
            res != UBSE_OK) {
                UBSE_LOG_ERROR << "Sign for request failed, " << FormatRetCode(res);
                return res;
            }
        reqSimpoPtr->SetUbseMemShareBorrowReq(req);
    }
    // 不是master调用RPC异步发送
    if (localNodeId != masterNodeId) {
        ret = SendToMasterIfNotMaster(masterNodeId, reqSimpoPtr, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                                      static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW));
    } else {
        // 是master，切换线程，提交给新线程
        ret = AsyncMemShmBorrowProcessor(reqSimpoPtr);
    }

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to send request, " << FormatRetCode(ret) + ", requestId=" << context.requestId;
        return ret;
    }
    UBSE_LOG_INFO << "shm create with lender dispatcher send success, requestId=" << context.requestId;
    return UBSE_OK;
}

uint32_t UbseMemControllerDispatcher::MemShmAttachDispatcher(const UbseIpcMessage &buffer,
                                                             const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "shm attach dispatcher, requestId=" << context.requestId;
    UbseMemControllerDispatcher dispatcher = UbseMemControllerDispatcher::GetInstance();

    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = dispatcher.GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }
    // buffer 转结构
    UbseMemShareAttachReqSimpoPtr reqSimpoPtr{};
    ret = dispatcher.BufferToShmAttachReq(buffer, reqSimpoPtr, localNodeId, context);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to convert buffer, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return ret;
    }
    // 不是master调用RPC异步发送
    if (localNodeId != masterNodeId) {
        ret = SendToMasterIfNotMaster(masterNodeId, reqSimpoPtr, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                                      static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_ATTACH));
    } else {
        // 是master，切换线程，提交给新线程
        ret = AsyncMemShmAttachProcessor(reqSimpoPtr);
    }

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to send request, " << FormatRetCode(ret) + ", requestId=" << context.requestId;
        return ret;
    }
    UBSE_LOG_INFO << "shm attach dispatcher send success, requestId=" << context.requestId;
    return UBSE_OK;
}

uint32_t UbseMemControllerDispatcher::MemShmMemFaultGet(const UbseIpcMessage &buffer, const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "shm mem status get dispatcher, requestId=" << context.requestId;

    const auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<api::server::UbseApiServerModule>();
    if (apiServer == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage message{};
    // buffer 转结构
    std::string name{};
    auto ret = GetInstance().BufferToShmStatusGetReq(buffer, name);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to convert buffer, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return apiServer->SendResponse(ret, context.requestId, message);
    }
    // 同步查询
    def::UbseMemShmMemStatusDesc shmStatusDesc{};
    ret = UbseMemShmStatusGet(name, shmStatusDesc);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get shm mem status, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return apiServer->SendResponse(ret, context.requestId, message);
    }
    // 包装响应
    ret = UbseMemShmMemFaultGetResponsePack(shmStatusDesc, message);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to pack res, " << FormatRetCode(ret) + ", requestId=" << context.requestId;
        return apiServer->SendResponse(ret, context.requestId, message);
    }
    ret = apiServer->SendResponse(ret, context.requestId, message);
    delete[] message.buffer;
    message.buffer = nullptr;
    return ret;
}

uint32_t UbseMemControllerDispatcher::MemShmGetDispatcher(const UbseIpcMessage &buffer,
                                                          const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "shm get dispatcher, requestId=" << context.requestId;
    const auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<api::server::UbseApiServerModule>();
    if (apiServer == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage message{};
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetInstance().GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return apiServer->SendResponse(ret, context.requestId, message);
    }
    // buffer 转结构
    std::string name{};
    ret = GetInstance().BufferToShmGetReq(buffer, name);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to convert buffer, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return apiServer->SendResponse(ret, context.requestId, message);
    }
    // 同步查询
    auto udsInfo = GenUdsInfo(context);
    def::UbseMemShmDesc shmDesc{};
    UBSE_LOG_INFO << "start to get shm, name=" << name << ", requestId=" << context.requestId;
    ret = UbseMemShmGet(name, shmDesc, &udsInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get shm, " << FormatRetCode(ret) << ", name=" << name
                       << ", requestId=" << context.requestId;
        return apiServer->SendResponse(ret, context.requestId, message);
    }
    // 包装响应
    ret = UbseMemShmGetResponsePack(shmDesc, message);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to pack res, " << FormatRetCode(ret) << ", name=" << name
                       << ", requestId=" << context.requestId;
    }
    ret = apiServer->SendResponse(ret, context.requestId, message);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send response, requestId=" << context.requestId;
    }
    delete[] message.buffer;
    message.buffer = nullptr;
    return ret;
}

uint32_t UbseMemControllerDispatcher::MemShmListDispatcher(const UbseIpcMessage &buffer,
                                                           const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "shm list dispatcher, requestId=" << context.requestId;
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<api::server::UbseApiServerModule>();
    if (apiServer == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage message{};
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetInstance().GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return apiServer->SendResponse(ret, context.requestId, message);
    }
    // 同步查询
    def::UbseMemDebtQueryRequest debtQueryRequest{};
    debtQueryRequest.udsInfo = GenUdsInfo(context);
    std::vector<def::UbseMemShmDesc> shmDescs{};
    ret = UbseMemShmList(debtQueryRequest, shmDescs);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "shm list get failed. " << FormatRetCode(ret);
        return apiServer->SendResponse(ret, context.requestId, message);
    }
    ret = UbseMemShmListResponsePack(shmDescs, message);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "pack list response failed. " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "shm list dispatcher end, requestId=" << context.requestId;
    ret = apiServer->SendResponse(ret, context.requestId, message);
    delete[] message.buffer;
    message.buffer = nullptr;
    return ret;
}

uint32_t UbseMemControllerDispatcher::MemShmListWithPrefixDispatcher(const UbseIpcMessage &buffer,
                                                                     const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "shm list dispatcher, requestId=" << context.requestId;
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<api::server::UbseApiServerModule>();
    if (apiServer == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage message{};
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetInstance().GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return ret;
    }
    // buffer 转结构
    def::UbseMemDebtQueryRequest debtQueryRequest{};
    ret = BufferToShmGetReq(buffer, debtQueryRequest.name);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to convert buffer, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return apiServer->SendResponse(ret, context.requestId, message);
    }
    // 同步查询
    debtQueryRequest.udsInfo = GenUdsInfo(context);
    std::vector<def::UbseMemShmDesc> shmDescs{};
    ret = UbseMemShmList(debtQueryRequest, shmDescs);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "shm list get failed. " << FormatRetCode(ret);
        return ret;
    }
    ret = UbseMemShmListResponsePack(shmDescs, message);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "pack list response failed. " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "shm list dispatcher end, requestId=" << context.requestId;
    ret = apiServer->SendResponse(ret, context.requestId, message);
    delete[] message.buffer;
    message.buffer = nullptr;
    return ret;
}

uint32_t UbseMemControllerDispatcher::MemShmDetachDispatcher(const UbseIpcMessage &buffer,
                                                             const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "shm detach dispatcher, requestId=" << context.requestId << "uid=" << context.clientInfo.uid
                  << "gid:=" << context.clientInfo.gid;
    UbseMemControllerDispatcher dispatcher = GetInstance();
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = dispatcher.GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return UBSE_ERROR;
    }
    // buffer 转结构
    UbseMemShareDetachReqSimpoPtr reqSimpoPtr{};
    ret = dispatcher.BufferToShmDetachReq(buffer, reqSimpoPtr, localNodeId, context);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to convert buffer, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return UBSE_ERROR;
    }
    // 不是master调用RPC异步发送
    if (localNodeId != masterNodeId) {
        ret = SendToMasterIfNotMaster(masterNodeId, reqSimpoPtr, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                                      static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_DETACH));
    } else {
        // 是master，切换线程，提交给新线程
        ret = AsyncMemShmDetachProcessor(reqSimpoPtr, masterNodeId);
    }

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to send request, " << FormatRetCode(ret) + ", requestId=" << context.requestId;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
uint32_t UbseMemControllerDispatcher::MemShmReturnDispatcher(const UbseIpcMessage &buffer,
                                                             const UbseRequestContext &context)
{
    // 获取主节点以及当前节点
    UBSE_LOG_INFO << "shm delete dispatcher, requestId=" << context.requestId << ", uid=" << context.clientInfo.uid
                  << "gid: " << context.clientInfo.gid;
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetInstance().GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return ret;
    }
    // buffer 转结构
    UbseMemReturnReqSimpoPtr reqSimpoPtr{};
    ret = GetInstance().BufferToShmReturnReq(buffer, reqSimpoPtr, localNodeId, context);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to convert buffer, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return ret;
    }

    UBSE_LOG_INFO << "return request name=" << reqSimpoPtr.Get()->GetUbseMemReturnReq().name;
    // 不是master调用RPC异步发送
    if (localNodeId != masterNodeId) {
        ret = SendToMasterIfNotMaster(masterNodeId, reqSimpoPtr,
                                      static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
                                      static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_SHARE_RETURN));
    } else {
        // 是master，切换线程，提交给新线程
        ret = AsyncMemShmReturnProcessor(reqSimpoPtr, masterNodeId);
    }

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to send request, " << FormatRetCode(ret) + ", requestId=" << context.requestId;
        return ret;
    }
    return UBSE_OK;
}
UbseResult UbseMemControllerDispatcher::GetMasterAndLocalNodeId(std::string &masterNodeId, std::string &localNodeId)
{
    // 获取主节点以及当前节点
    UbseRoleInfo masterInfo{};
    if (const auto ret = UbseGetMasterInfo(masterInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get master info, " << FormatRetCode(ret);
        return ret;
    }
    UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
        return ret;
    }
    masterNodeId = std::move(masterInfo.nodeId);
    localNodeId = std::move(currentRoleInfo.nodeId);
    return UBSE_OK;
}

uint32_t UbseMemControllerDispatcher::UbseMemFdBorrowRpc(UbseMemFdBorrowReq &req, const UbseRequestContext &context)
{
    std::string masterNodeId{};
    std::string localNodeId{};
    UbseMemControllerDispatcher dispatcher = UbseMemControllerDispatcher::GetInstance();
    auto ret = dispatcher.GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, "
                       << FormatRetCode(ret) + ", requestId=" << context.requestId;
        return UBSE_ERROR;
    }
    req.importNodeId = localNodeId;
    req.requestNodeId = localNodeId;
    SetBaseReqInfo(req, context);

    if (IsHighSafety()) {
        if (const auto res =
                UbseMemSignVerifier::Sign("fd", req.trustRingData.reqSignedData, req.trustRingData.trustRingId);
            res != UBSE_OK) {
            UBSE_LOG_ERROR << "Sign for request failed, " << FormatRetCode(res);
            return res;
            }
    }

    SendParam sendParam{masterNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_FD_BORROW)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }

    UbseMemFdBorrowReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemFdBorrowReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemFdBorrowReq(req);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr";
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to Send to fd borrow req to master, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "Success to Send to fd borrow req to master, requestId=" << req.requestId;
    return UBSE_OK;
}

uint32_t UbseMemControllerDispatcher::UbseMemFdBorrowDispatch(const UbseIpcMessage &buffer,
                                                              const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "UbseMemFdBorrowDispatch, requestId=" << context.requestId;
    // buffer 转结构
    UbseMemFdBorrowReq req{};
    auto ret = UbseMemCreateReqUnpack(buffer, req);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemFdBorrowReq unpack failed, " << FormatRetCode(ret);
        return ret;
    }
    return UbseMemFdBorrowRpc(req, context);
}

uint32_t UbseMemControllerDispatcher::UbseMemFdBorrowWithLenderDispatch(const UbseIpcMessage &buffer,
                                                                        const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "UbseMemFdBorrowWithLenderDispatch, requestId=" << context.requestId;
    // buffer 转结构
    UbseMemFdBorrowReq req{};
    auto ret = UbseMemCreateWithLenderReqUnpack(buffer, req);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemFdBorrowReq unpack failed, " << FormatRetCode(ret);
        return ret;
    }
    return UbseMemFdBorrowRpc(req, context);
}

uint32_t UbseMemControllerDispatcher::UbseMemFdBorrowWithCandidate(const UbseIpcMessage &buffer,
                                                                   const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "UbseMemFdBorrowWithCandidate, requestId=" << context.requestId;
    // buffer 转结构
    UbseMemFdBorrowReq req{};
    auto ret = UbseMemCreateWithCandidateReqUnpack(buffer, req);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemFdBorrowReq unpack failed, " << FormatRetCode(ret);
        return ret;
    }
    return UbseMemFdBorrowRpc(req, context);
}

uint32_t UbseMemControllerDispatcher::UbseMemFdReturnDispatch(const UbseIpcMessage &buffer,
                                                              const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "UbseMemFdReturnDispatch, requestId=" << context.requestId;
    UbseMemReturnReq req{};
    UbseRoleInfo masterInfo{};
    if (auto ret = UbseGetMasterInfo(masterInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get master info, " << FormatRetCode(ret);
        return ret;
    }
    UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
        return ret;
    }
    if (auto ret = UbseMemFdDeleteReqUnpack(buffer, req); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to unpack req " << FormatRetCode(ret);
        return ret;
    }
    req.requestNodeId = currentRoleInfo.nodeId;
    req.importNodeId = currentRoleInfo.nodeId;
    SetBaseReqInfo(req, context);
    SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
                        static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_RETURN)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }

    UbseMemReturnReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemReturnReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemReturnReq(req);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to Send fd borrow request, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

uint32_t UbseMemControllerDispatcher::UbseMemFdPermissionDispatch(const UbseIpcMessage &buffer,
                                                                  const UbseRequestContext &context)
{
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    UbseMemControllerDispatcher dispatcher = UbseMemControllerDispatcher::GetInstance();
    auto ret = dispatcher.GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret) + ", requestId="
                       << context.requestId;
        return UBSE_ERROR;
    }
    UbseMemFdPermissionReq fdPermissionReq{};
    ret = UbseMemFdPermissionReqUnpack(buffer, fdPermissionReq);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to unpack fd permission, "
                       << FormatRetCode(ret) + ", requestId=" << context.requestId;
        return ret;
    }
    fdPermissionReq.requestNodeId = localNodeId;
    SetBaseReqInfo(fdPermissionReq, context);
    UbseBaseMessagePtr reqMessagePtr = new (std::nothrow) UbseMemFdPermissionReqMessage(fdPermissionReq);
    UbseBaseMessagePtr respMessagePtr = new (std::nothrow) UbseMemFdPermissionRespMessage();

    SendParam sendParam{masterNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
                        static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_PERMISSION)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }

    ret = comModule->RpcSend(sendParam, reqMessagePtr, respMessagePtr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to Send to master, " << FormatRetCode(ret) << ", requestId=" << context.requestId;
        return ret;
    }
    UBSE_LOG_INFO << "Success to Send to master";
    auto response = UbseBaseMessage::DeConvert<UbseMemFdPermissionRespMessage>(respMessagePtr);
    auto fdPermissionResp = response->fdPermissionResp;
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServer == nullptr) {
        UBSE_LOG_ERROR << "Failed to get api server";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage responseMessage{nullptr, 0};
    return apiServer->SendResponse(fdPermissionResp.result, fdPermissionResp.requestId, responseMessage);
}

uint32_t GetNodeInfo(const std::string &nodeId, ubse::nodeController::UbseNodeInfo &node)
{
    UBSE_LOG_INFO << "Node id=" << nodeId;
    auto nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    for (const auto &nodeInfo : nodeInfos) {
        if (nodeInfo.second.nodeId == nodeId) {
            node = nodeInfo.second;
            break;
        }
    }
    if (node.nodeId.empty()) {
        UBSE_LOG_ERROR << "Failed to find node info , node=" << nodeId;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseMemControllerDispatcher::UbseMemFdGetDispatch(const UbseIpcMessage &buffer,
                                                           const UbseRequestContext &context)
{
    std::string name{};
    auto ret = UbseMemNameUnpack(buffer, name);
    if (ret != UBSE_OK) {
        return ret;
    }
    UbseUdsInfo udsInfo = GenUdsInfo(context);
    ubse::mem::def::UbseMemFdDesc fdDesc;
    UbseIpcMessage resp{};
    ret = ubse::mem::controller::UbseMemFdGet(name, fdDesc, &udsInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemFdGet failed, " << FormatRetCode(ret);
        return ret;
    }
    // 打包
    ret = UbseMemFdDescPack(fdDesc, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemFdDescPack failed, ret=" << ret;
        return ret;
    }
    auto apiServerModule = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        delete[] resp.buffer;
        resp.buffer = nullptr;
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
    }
    delete[] resp.buffer;
    resp.buffer = nullptr;
    return ret;
}
uint32_t UbseMemControllerDispatcher::UbseMemFdListDispatch(const UbseIpcMessage &buffer,
                                                            const UbseRequestContext &context)
{
    std::vector<ubse::mem::def::UbseMemFdDesc> fdList{};
    UbseUdsInfo udsInfo = GenUdsInfo(context);
    auto ret = UbseMemFdList(udsInfo, fdList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemFdList failed" << FormatRetCode(ret);
        return ret;
    }
    UbseIpcMessage resp{};
    ret = UbseMemFdDescListPack(fdList, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemFdDescListPack failed" << FormatRetCode(ret);
        return ret;
    }
    auto apiServerModule = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        delete[] resp.buffer;
        resp.buffer = nullptr;
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
    }
    delete[] resp.buffer;
    resp.buffer = nullptr;
    return ret;
}

uint32_t GetSrcSocketId(UbseMemNumaBorrowReq &req)
{
    if (req.linkInfo.lenderPort == -1) {
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "lenderNode=" << req.linkInfo.lenderNode << ", lenderSocketId="
                  << req.linkInfo.lenderSocketId << ", lenderPortId=" << req.linkInfo.lenderPort;
    const auto exportNodeId = req.linkInfo.lenderNode;
    const auto exportSocketId = req.linkInfo.lenderSocketId;
    const auto exportPort = req.linkInfo.lenderPort;
    const auto importNodeId = req.importNodeId;
    ubse::nodeController::UbseNodeInfo exportNodeInfo{};
    if (GetNodeInfo(exportNodeId, exportNodeInfo)) {
        return UBSE_ERROR;
    }
    UbseCpuLocation importLocation{};
    for (const auto &[cpuLocation, cpuInfo] : exportNodeInfo.cpuInfos) {
        if (cpuInfo.socketId == exportSocketId) {
            UBSE_LOG_INFO << "Success to find export cpu info, export chip Id=" << cpuLocation.chipId;
            const auto it = cpuInfo.portInfos.find(std::to_string(exportPort));
            if (it == cpuInfo.portInfos.end() || it->second.portStatus == PortStatus::DOWN) {
                UBSE_LOG_ERROR << "The link is not exist. lenderSocketId=" << req.linkInfo.lenderSocketId
                               << ", lenderPortId=" << req.linkInfo.lenderPort;
                return UBSE_ERROR;
            }
            auto ret = ubse::utils::ConvertStrToUint32(it->second.remoteChipId, importLocation.chipId);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to convert, remoteChipId=" << it->second.remoteChipId;
                return ret;
            }
            importLocation.nodeId = it->second.remoteSlotId;
            break;
        }
    }
    if (importLocation.nodeId != importNodeId) {
        UBSE_LOG_ERROR << "Failed to find import location.";
        return UBSE_ERROR;
    }
    ubse::nodeController::UbseNodeInfo importNodeInfo{};
    if (GetNodeInfo(importNodeId, importNodeInfo) != UBSE_OK) {
        return UBSE_ERROR;
    }
    const auto iter = importNodeInfo.cpuInfos.find(importLocation);
    if (iter == importNodeInfo.cpuInfos.end()) {
        UBSE_LOG_ERROR << "The link is not exist.";
        return UBSE_ERROR;
    }
    req.srcSocket = iter->second.socketId;
    return UBSE_OK;
}

uint32_t GetSrcNuma(UbseMemNumaBorrowReq &req)
{
    if (req.linkInfo.lenderPort == -1) {
        return UBSE_OK;
    }
    auto importNodeId = req.importNodeId;
    auto srcSocket = req.srcSocket;
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(importNodeId);
    if (nodeInfo.nodeId.empty()) {
        UBSE_LOG_ERROR << "Failed to find node info, nodeId=" << importNodeId;
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "Src socket=" << srcSocket << ", import node id=" << importNodeId;
    for (const auto &numaInfo : nodeInfo.numaInfos) {
        if (numaInfo.second.socketId == srcSocket) {
            req.srcNuma = numaInfo.first.numaId;
            UBSE_LOG_INFO << "Src numa=" << req.srcNuma;
            return UBSE_OK;
        }
    }
    UBSE_LOG_ERROR << "Failed to find correct numa.";
    return UBSE_ERROR;
}

uint32_t MemNumaBorrowRpc(const std::string &masterNodeId, const std::string &localNodeId,
    const UbseMemNumaBorrowReq &req)
{
    // 不是master调用RPC异步发送
    if (localNodeId != masterNodeId) {
        UbseMemNumaBorrowReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemNumaBorrowReqSimpo();
        if (ubseRequestPtr == nullptr) {
            UBSE_LOG_ERROR << "Failed to new ptr";
            return UBSE_ERROR_NULLPTR;
        }
        ubseRequestPtr->SetUbseMemNumaBorrowReq(req);
        UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
        if (ubseResponsePtr == nullptr) {
            UBSE_LOG_ERROR << "Failed to new ptr";
            return UBSE_ERROR_NULLPTR;
        }
        SendParam sendParam{masterNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                            static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_NUMA_BORROW)};

        auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
        if (comModule == nullptr) {
            UBSE_LOG_ERROR << "Getting ComModule failed.";
            return UBSE_ERROR_NULLPTR;
        }
        const auto ret = comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to Send to numa borrow req to master, " << FormatRetCode(ret);
            return ret;
        }
        UBSE_LOG_INFO << "Success to Send to numa borrow req to master, requestId=" << req.requestId;
        return ret;
    }
    // 是master，切换线程，提交给新线程
    return DoNumaBorrowAsync(req);
}

uint32_t UbseMemControllerDispatcher::UbseMemNumaBorrowRpc(UbseMemNumaBorrowReq &req, const UbseRequestContext &context)
{
    std::string masterNodeId{};
    std::string localNodeId{};
    UbseMemControllerDispatcher dispatcher = UbseMemControllerDispatcher::GetInstance();
    auto ret = dispatcher.GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, "
                       << FormatRetCode(ret) + ", requestId=" << context.requestId;
        return UBSE_ERROR;
    }
    req.importNodeId = localNodeId;
    req.requestNodeId = localNodeId;
    SetBaseReqInfo(req, context);
    if (GetSrcSocketId(req) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get src socket for borrow via specifying link.";
        return UBSE_ERR_LINK_NOT_EXIST;
    }
    if (GetSrcNuma(req) != UBSE_OK) {
        return UBSE_ERR_FIND_SRC_NUMA;
    }
    if (IsHighSafety()) {
        if (const auto res =
                UbseMemSignVerifier::Sign("numa", req.trustRingData.reqSignedData, req.trustRingData.trustRingId);
            res != UBSE_OK) {
            UBSE_LOG_ERROR << "Sign for request failed, " << FormatRetCode(res);
            return res;
        }
    }

    return MemNumaBorrowRpc(masterNodeId, localNodeId, req);
}
UbseResult UbseMemControllerDispatcher::UbseMemNumaCreateHandler(const UbseIpcMessage &buffer,
                                                                 const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "UbseMemNumaCreate, requestId=" << context.requestId;
    // buffer 转结构
    UbseMemNumaBorrowReq req{};
    auto ret = UbseMemNumaCreateReqUnpack(buffer, req);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemNumaBorrowReq unpack failed, " << FormatRetCode(ret);
        return ret;
    }
    return UbseMemNumaBorrowRpc(req, context);
}

UbseResult UbseMemControllerDispatcher::UbseMemNumaCreateWithLender(const UbseIpcMessage &buffer,
                                                                    const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "UbseMemNumaCreateWithLender, requestId=" << context.requestId;
    // buffer 转结构
    UbseMemNumaBorrowReq req{};
    auto ret = UbseMemNumaCreateLenderReqUnpack(buffer, req);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemNumaBorrowReq unpack failed, " << FormatRetCode(ret);
        return ret;
    }
    return UbseMemNumaBorrowRpc(req, context);
}

uint32_t UbseMemControllerDispatcher::UbseMemNumaBorrowWithCandidate(const UbseIpcMessage &buffer,
                                                                     const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "UbseMemNumaBorrowWithCandidate, requestId=" << context.requestId;
    // buffer 转结构
    UbseMemNumaBorrowReq req{};
    auto ret = UbseMemNumaCreateWithCandidateReqUnpack(buffer, req);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemNumaBorrowReq unpack failed, " << FormatRetCode(ret);
        return ret;
    }
    return UbseMemNumaBorrowRpc(req, context);
}

UbseResult UbseMemControllerDispatcher::UbseMemNumaDelete(const UbseIpcMessage &buffer,
                                                          const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "UbseMemNumaDelete, requestId=" << context.requestId;
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    UbseMemControllerDispatcher dispatcher = UbseMemControllerDispatcher::GetInstance();
    auto ret = dispatcher.GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, "
                       << FormatRetCode(ret) + ", requestId=" << context.requestId;
        return UBSE_ERROR;
    }
    // buffer 转结构
    UbseMemReturnReq req{};
    UbseMemNumaDeleteUnpack(buffer, req);
    req.requestNodeId = localNodeId;
    req.importNodeId = localNodeId;
    SetBaseReqInfo(req, context);
    UbseMemReturnReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemReturnReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemReturnReq(req);
    // 不是master调用RPC异步发送
    if (localNodeId != masterNodeId) {
        UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
        if (ubseResponsePtr == nullptr) {
            UBSE_LOG_ERROR << "Failed to new ptr";
            return UBSE_ERROR_NULLPTR;
        }
        SendParam sendParam{masterNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
                            static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_NUMA_RETURN)};

        auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
        if (comModule == nullptr) {
            UBSE_LOG_ERROR << "Getting ComModule failed.";
            return UBSE_ERROR_NULLPTR;
        }
        ret = comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to Send to numa return req to master, " << FormatRetCode(ret);
            return ret;
        }
        UBSE_LOG_INFO << "Success to Send to numa return req to master";
        return ret;
    }
    // 是master，切换线程，提交给新线程
    DoReturnAsync(req, masterNodeId);
    return UBSE_OK;
}

UbseResult UbseMemControllerDispatcher::UbseMemNumaGetDispatch(const UbseIpcMessage &buffer,
                                                               const UbseRequestContext &context)
{
    std::string name{};
    auto ret = UbseMemNameUnpack(buffer, name);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseStringUnpack," << FormatRetCode(ret);
        return ret;
    }
    UbseUdsInfo udsInfo = GenUdsInfo(context);
    UbseIpcMessage resp{};
    ubse::mem::def::UbseMemNumaDesc memNumaDesc{};
    ret = ubse::mem::controller::UbseMemNumaGet(name, memNumaDesc, &udsInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemNumaGet," << FormatRetCode(ret);
        return ret;
    }
    // 打包
    ret = UbseMemNumaDescPack(memNumaDesc, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemNumaDescPack," << FormatRetCode(ret);
        return ret;
    }

    auto apiServerModule = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        delete[] resp.buffer;
        resp.buffer = nullptr;
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
    }
    delete[] resp.buffer;
    resp.buffer = nullptr;
    return ret;
}

UbseResult UbseMemControllerDispatcher::UbseMemNumaListDispatch(const UbseIpcMessage &buffer,
                                                                const UbseRequestContext &context)
{
    UbseUdsInfo udsInfo = GenUdsInfo(context);
    std::vector<ubse::mem::def::UbseMemNumaDesc> cMemNumaDescList{};
    uint32_t ret = ubse::mem::controller::UbseMemNumaList(udsInfo, cMemNumaDescList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemNumaList," << FormatRetCode(ret);
        return ret;
    }
    UbseIpcMessage resp{};
    // 打包
    ret = UbseMemNumaDescListPack(cMemNumaDescList, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemNumaDescListPack," << FormatRetCode(ret);
        return ret;
    }

    auto apiServerModule = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        delete[] resp.buffer;
        resp.buffer = nullptr;
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
    }
    delete[] resp.buffer;
    resp.buffer = nullptr;
    return UBSE_OK;
}

def::UbseMemNumaDesc ConvertOperationRespToNumaDesc(const UbseMemOperationResp &resp)
{
    def::UbseMemNumaDesc numaDesc{};
    numaDesc.name = resp.name;
    uint64_t size;
    auto ret = ConvertStrToUint64(resp.realSize, size);
    if (ret == UBSE_OK) {
        numaDesc.size = size;
    }
    uint32_t importSlotId;
    ret = ConvertStrToUint32(resp.requestNodeId, importSlotId);
    if (ret == UBSE_OK) {
        numaDesc.importNode.slotId = importSlotId;
    }
    numaDesc.numaId = resp.remoteNumaId;
    return numaDesc;
};

UbseResult UbseMemControllerDispatcher::UbseMemNumaBorrowRespHandler(const UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "Numa Borrow Resp, name=" << resp.name << ", requestId=" << resp.requestId;
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServer == nullptr) {
        UBSE_LOG_ERROR << "Failed to get api server";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage message{nullptr, 0};
    // 取requestrId, 发给ApiServer
    const auto requestId = resp.requestId;
    // 结构转换
    uint32_t status = resp.errorCode;
    if (status != UBSE_OK) {
        return apiServer->SendResponse(status, requestId, message);
    }
    auto numaDesc = def::UbseMemNumaDesc();
    auto ret = UbseMemNumaGet(resp.name, numaDesc, nullptr);
    if (ret != UBSE_OK) {
        // 获取借用信息失败, 使用UbseMemOperationResp构建返回信息
        UBSE_LOG_ERROR << "Failed to get numa desc, " << FormatRetCode(ret);
        numaDesc = ConvertOperationRespToNumaDesc(resp);
    }
    // 使用借用信息构建返回信息
    auto packRet = UbseMemNumaDescPack(numaDesc, message);
    if (packRet != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to pack UbseMemNumaDesc, " << FormatRetCode(ret);
        status = UBSE_ERROR_SERIALIZE_FAILED;
    }
    ret = apiServer->SendResponse(status, requestId, message);
    delete[] message.buffer;
    return ret;
}

UbseResult UbseMemControllerDispatcher::UbseMemNumaReturnRespHandler(const UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "Numa Return Resp, name=" << resp.name << ", requestId=" << resp.requestId;
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServer == nullptr) {
        UBSE_LOG_ERROR << "Failed to get api server";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage message{};
    // 取requestrId， 发给ApiServer
    auto requestId = resp.requestId;
    message.buffer = nullptr;
    message.length = 0;
    auto ret = apiServer->SendResponse(resp.errorCode, requestId, message);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send response, response errorcode=" << resp.errorCode
                       << ", requestId=" << requestId;
    }
    return ret;
}

uint32_t UbseMemControllerDispatcher::UbseMemNodeBorrowInfoDispatch(const UbseIpcMessage &buffer,
                                                                    const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "UbseMemNodeBorrowInfo, requestId=" << context.requestId;
    std::vector<def::UbseNodeBorrowInfo> nodeBorrowInfo{};
    auto ret = UbseMemNodeBorrowInfoQuery(nodeBorrowInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query node borrow info, " << FormatRetCode(ret);
        return ret;
    }
    // 优先导入节点排序
    std::sort(nodeBorrowInfo.begin(), nodeBorrowInfo.end(),
              [](const def::UbseNodeBorrowInfo &a, const def::UbseNodeBorrowInfo &b) {
                    if (a.borrowSlotId == b.borrowSlotId) {
                        return a.lendSlotId < b.lendSlotId;
                    }
                    return a.borrowSlotId < b.borrowSlotId;
                });
    serial::UbseSerialization serialization{};
    if (!UbseNodeBorrowInfosSerialize(serialization, nodeBorrowInfo)) {
        return UBSE_ERROR;
    }
    auto apiServerModule = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage response{serialization.GetBuffer(), static_cast<uint32_t>(serialization.GetLength())};
    if (!response.buffer) {
        UBSE_LOG_ERROR << "Serialization response failed.";
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " NodeBorrowHandle response send failed, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseMemControllerDispatcher::UbseMemNodeLendInfoDispatch(const UbseIpcMessage &buffer,
                                                                  const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "UbseMemNodeLendInfo, requestId=" << context.requestId;
    std::vector<def::UbseNodeBorrowInfo> nodeBorrowInfo{};
    auto ret = UbseMemNodeBorrowInfoQuery(nodeBorrowInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query node borrow info, " << FormatRetCode(ret);
        return ret;
    }
    // 优先导出节点排序
    std::sort(nodeBorrowInfo.begin(), nodeBorrowInfo.end(),
              [](const def::UbseNodeBorrowInfo &a, const def::UbseNodeBorrowInfo &b) {
                    if (a.lendSlotId == b.lendSlotId) {
                        return a.borrowSlotId < b.borrowSlotId;
                    }
                    return a.lendSlotId < b.lendSlotId;
                });
    serial::UbseSerialization serialization{};
    if (!UbseNodeBorrowInfosSerialize(serialization, nodeBorrowInfo)) {
        return UBSE_ERROR;
    }
    auto apiServerModule = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage response{serialization.GetBuffer(), static_cast<uint32_t>(serialization.GetLength())};
    if (!response.buffer) {
        UBSE_LOG_ERROR << "Serialization response failed.";
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " NodeLendHandle response send failed, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller
