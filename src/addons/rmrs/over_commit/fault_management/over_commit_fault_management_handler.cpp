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

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>

#include "ubse_thread_pool.h"
#include "mem_borrow_executor.h"
#include "mem_manager.h"
#include "mp_configuration.h"

#include "over_commit_fault_management_handler.h"
#include "over_commit_fault_memid_module.h"
#include "over_commit_fault_node_module.h"

namespace mempooling::over_commit {

const int MEMID_SUCCESS_RESPONSE_DATA_LENGTH = 1;
const int MEMID_FAIL_RESPONSE_DATA_LENGTH = 2;

// 简化故障处理线程池：构造时按配置项（rmrs.fault.simplified）初始化，进程生命周期内复用
SimplifiedFaultTaskExecutor::SimplifiedFaultTaskExecutor()
{
    if (!MpConfiguration::GetInstance().GetFaultSimplified()) {
        return;
    }
    // 节点内进程级并行：UbseTaskExecutor 线程池，并行度 4
    constexpr uint16_t kFaultParallelThreadNum = 4;
    constexpr uint32_t kFaultParallelQueueCapacity = 1024;
    taskExecutor_ = ubse::task_executor::UbseTaskExecutor::Create("RmrsFaultSimplified", kFaultParallelThreadNum,
                                                                  kFaultParallelQueueCapacity);
    if (taskExecutor_ == nullptr || !taskExecutor_->Start()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][FaultManagement] Failed to create simplified fault task executor.";
        taskExecutor_ = nullptr;
    }
}

// 故障处理相关Handler
// memid级别故障处理：获取节点上的VM numa info
uint32_t OverCommitFaultManagementHandler::GetVmNumaInfoMapRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[OverCommit][FaultManagement] GetVmNumaInfoMapRecvHandler start.";

    OverCommitFaultVmNumaInfoParam param{};
    RmrsInStream inBuilder(req.data, req.len);
    inBuilder >> param;
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    MpResult ret =
        OverCommitFaultMemIdModule::Instance().GetRemoteNumaVms(param.remoteNumaId, vmNumaInfoWithSocketList);

    OverCommitVmRemoteNumaInfoResult result{.vmNumaInfoWithSocketList = vmNumaInfoWithSocketList, .retCode = ret};
    RmrsOutStream builder;
    builder << result;
    resp.len = builder.GetSize();
    resp.data = builder.GetBufferPointer();
    resp.freeFunc = [](uint8_t* data) {
        delete[] data;
    };
    if (MEM_POOLING_OK != ret) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][FaultManagement] GetVmNumaInfoMapRecvHandler failed res=" << ret << ".";
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[OverCommit][FaultManagement] GetVmNumaInfoMapRecvHandler end.";
    return ret;
}

void OverCommitFaultManagementHandler::GetVmNumaInfoMapResHandler(void* ctx, const UbseByteBuffer& respData,
                                                                  uint32_t resCode)
{
    if (ctx == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit][FaultManagement] Ctx is null.";
        return;
    }
    auto* result = static_cast<OverCommitVmRemoteNumaInfoResult*>(ctx);
    if (resCode != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][FaultManagement] RPC transport error, resCode=" << resCode;
        result->retCode = resCode; // 写入错误码
        // 注意：此时 respData 可能无效，不反序列化
        return;
    }
    // resCode OK 时，要求 respData 有效
    if (respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][FaultManagement] Empty response data despite OK resCode.";
        result->retCode = MEM_POOLING_ERROR;
        return;
    }
    RmrsInStream builder(respData.data, respData.len);
    builder >> (*result);
}

// memid级别故障处理：执行大页配置、setRemoteNumaInfo
uint32_t OverCommitFaultManagementHandler::MemIdExecuteRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit][FaultManagement] MemIdExecuteRecvHandler start.";

    OverCommitFaultMemIdExecuteParam param{};
    uint64_t adjustSize = 0;
    RmrsInStream builder(req.data, req.len);
    builder >> param;
    // 判断故障新借的远端numa大小是否够迁，不够的话修正借用大小
    auto res = OverCommitFaultMemIdModule::Instance().CheckBorrowedMemSizeForPidMigrate(param, adjustSize);
    if (res == MEM_POOLING_OK) {
        res = OverCommitFaultMemIdModule::Instance().AdjustFaultHandleBorrowedMemSize(param, adjustSize);
        if (res == MEM_POOLING_OK) {
            res = OverCommitFaultMemIdModule::Instance().MemIdExecute(param);
        } else {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Borrowed mem is not enough to migrate.";
        }
    }
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][FaultManagement] Recv MemIdExecuteRecvHandler res=" << res << ".";
        resp.len = MEMID_FAIL_RESPONSE_DATA_LENGTH;
        resp.data = new (std::nothrow) uint8_t[resp.len]{};
        if (resp.data == nullptr) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(res);
        resp.data[1] = 0;
    } else {
        resp.len = MEMID_SUCCESS_RESPONSE_DATA_LENGTH;
        resp.data = new (std::nothrow) uint8_t[resp.len]{};
        if (resp.data == nullptr) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(res);
    }

    resp.freeFunc = [](uint8_t* p) {
        if (p != nullptr) {
            delete[] p;
        }
    };
    return res;
}

void OverCommitFaultManagementHandler::MemIdExecuteResHandler(void* ctx, const UbseByteBuffer& respData,
                                                              uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit][FaultManagement] Ctx or respData is null.";
        return;
    }
    auto* result = static_cast<uint32_t*>(ctx);
    if (resCode != MEM_POOLING_OK || respData.len != MEMID_SUCCESS_RESPONSE_DATA_LENGTH) {
        *result = MEM_POOLING_ERROR;
        return;
    }
    *result = MEM_POOLING_OK;
}

// memid级别故障处理：不迁回，直接归还
uint32_t OverCommitFaultManagementHandler::MemIdReturnDirectlyExecuteRecvHandler(const UbseByteBuffer& req,
                                                                                 UbseByteBuffer& resp)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[OverCommit][FaultManagement] MemIdReturnDirectlyExecuteRecvHandler start.";

    std::string borrowId{};
    RmrsInStream builder(req.data, req.len);
    builder >> borrowId;
    MpResult ret = MemBorrowExecutor::Instance().MemFreeWithOps(borrowId, true, false, true);
    if (MEM_POOLING_OK != ret) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][FaultManagement] Recv MemIdReturnDirectlyExecuteRecvHandler ret=" << ret << ".";
        resp.len = MEMID_FAIL_RESPONSE_DATA_LENGTH;
        resp.data = new (std::nothrow) uint8_t[resp.len]{};
        if (resp.data == nullptr) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(ret);
        resp.data[1] = 0;
    } else {
        resp.len = MEMID_SUCCESS_RESPONSE_DATA_LENGTH;
        resp.data = new (std::nothrow) uint8_t[resp.len]{};
        if (resp.data == nullptr) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(ret);
    }

    resp.freeFunc = [](uint8_t* p) {
        if (p != nullptr) {
            delete[] p;
        }
    };
    return ret;
}

// pid级别关闭冷热流动
uint32_t OverCommitFaultManagementHandler::DisableSmapProcessMigrateRecvHandler(const UbseByteBuffer& req,
                                                                                UbseByteBuffer& resp)
{
    std::vector<pid_t> pids;
    RmrsInStream builder(req.data, req.len);
    builder >> pids;
    for (auto pid : pids) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][FaultManagement] Disable process migrate, pid=" << pid << ".";
    }
    int retSmap = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 0, 0);
    if (MEM_POOLING_OK != static_cast<MpResult>(retSmap)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "SmapEnableProcessMigrateHelper faild enable=" << 0 << ", retSmap=" << retSmap << ".";
        resp.len = MEMID_FAIL_RESPONSE_DATA_LENGTH;
        resp.data = new (std::nothrow) uint8_t[resp.len]{};
        if (resp.data == nullptr) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(retSmap);
        resp.data[1] = 0;
    } else {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "SmapEnableProcessMigrateHelper successed, enable=" << 0 << ".";
        PidSmapEnableCompleted::Instance().Update(pids);
        resp.len = MEMID_SUCCESS_RESPONSE_DATA_LENGTH;
        resp.data = new (std::nothrow) uint8_t[resp.len]{};
        if (resp.data == nullptr) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(retSmap);
    }
    resp.freeFunc = [](uint8_t* p) {
        if (p != nullptr) {
            delete[] p;
        }
    };
    return retSmap;
}

void OverCommitFaultManagementHandler::DisableSmapProcessMigrateResHandler(void* ctx, const UbseByteBuffer& respData,
                                                                           uint32_t resCode)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "DisableSmapProcessMigrateResHandler resCode = " << resCode;
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit][FaultManagement] Ctx or respData is null.";
        return;
    }
    auto* result = static_cast<uint32_t*>(ctx);
    if (resCode != MEM_POOLING_OK || respData.len != MEMID_SUCCESS_RESPONSE_DATA_LENGTH) {
        *result = MEM_POOLING_ERROR;
        return;
    }
    *result = MEM_POOLING_OK;
}

uint32_t OverCommitFaultManagementHandler::EnableSmapProcessMigrateRecvHandler(const UbseByteBuffer& req,
                                                                               UbseByteBuffer& resp)
{
    std::vector<pid_t> pids;
    RmrsInStream builder(req.data, req.len);
    builder >> pids;
    uint32_t ret;
    int successCount = 0;
    std::vector<pid_t> successRemovePids;
    for (auto pid : pids) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][FaultManagement] Enable process migrate, pid=" << pid << ".";
        std::vector<pid_t> enablePid = {pid};
        // enable = 1, flags = 0 （可根据需要调整）
        int retSmap = MpSmapHelper::SmapEnableProcessMigrateHelper(enablePid.data(), enablePid.size(), 1, 0);
        if (MEM_POOLING_OK != static_cast<MpResult>(retSmap)) {
            continue;
        } else {
            successCount++;
            successRemovePids.push_back(pid);
        }
    }

    if (successCount == 0) {
        ret = MEM_POOLING_ERROR;
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "SmapEnableProcessMigrateHelper failed enable=" << 1 << ", retSmap=" << ret << ".";
        resp.len = MEMID_FAIL_RESPONSE_DATA_LENGTH;
        resp.data = new (std::nothrow) uint8_t[resp.len]{};
        if (resp.data == nullptr) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(ret);
        resp.data[1] = 0;
    } else {
        ret = MEM_POOLING_OK;
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "SmapEnableProcessMigrateHelper succeeded, enable=" << 1 << ".";
        PidSmapEnableCompleted::Instance().Remove(successRemovePids);
        resp.len = MEMID_SUCCESS_RESPONSE_DATA_LENGTH;
        resp.data = new (std::nothrow) uint8_t[resp.len]{};
        if (resp.data == nullptr) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(ret);
    }
    resp.freeFunc = [](uint8_t* p) {
        if (p != nullptr) {
            delete[] p;
        }
    };
    return ret;
}

// 在 OverCommitFaultManagementHandler 中增加响应处理函数
void OverCommitFaultManagementHandler::EnableSmapProcessMigrateResHandler(void* ctx, const UbseByteBuffer& respData,
                                                                          uint32_t resCode)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "EnableSmapProcessMigrateResHandler resCode = " << resCode;
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit][FaultManagement] Ctx or respData is null.";
        return;
    }
    auto* result = static_cast<uint32_t*>(ctx);
    if (resCode != MEM_POOLING_OK || respData.len != MEM_POOLING_ERROR) {
        *result = MEM_POOLING_ERROR;
        return;
    }
    *result = MEM_POOLING_OK;
}

void OverCommitFaultManagementHandler::MemIdReturnDirectlyExecuteResHandler(void* ctx, const UbseByteBuffer& respData,
                                                                            uint32_t resCode)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "MemIdReturnDirectlyExecuteResHandler resCode = " << resCode;
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit][FaultManagement] Ctx or respData is null.";
        return;
    }
    auto* result = static_cast<uint32_t*>(ctx);
    if (resCode != MEM_POOLING_OK || respData.len != MEMID_SUCCESS_RESPONSE_DATA_LENGTH) {
        *result = MEM_POOLING_ERROR;
        return;
    }
    *result = MEM_POOLING_OK;
}

// memid级别故障处理：内存迁回 + 归还
uint32_t OverCommitFaultManagementHandler::MemIdReturnExecuteRecvHandler(const UbseByteBuffer& req,
                                                                         UbseByteBuffer& resp)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[OverCommit][FaultManagement] MemIdReturnExecuteRecvHandler start.";

    std::string borrowId{};
    RmrsInStream builder(req.data, req.len);
    builder >> borrowId;
    MpResult ret = MemBorrowExecutor::Instance().MemFreeWithOps(borrowId, true, true, true);
    if (MEM_POOLING_OK != ret) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][FaultManagement] Recv MemIdReturnExecuteRecvHandler ret=" << ret << ".";
        resp.len = MEMID_FAIL_RESPONSE_DATA_LENGTH;
        resp.data = new (std::nothrow) uint8_t[resp.len]{};
        if (resp.data == nullptr) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(ret);
        resp.data[1] = 0;
    } else {
        resp.len = MEMID_SUCCESS_RESPONSE_DATA_LENGTH;
        resp.data = new (std::nothrow) uint8_t[resp.len]{};
        if (resp.data == nullptr) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(ret);
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][FaultManagement] Start to clear fault process borrowId .";
        MpResult retBorrowIdInFaultProcess = BorrowIdInFaultProcess::Instance().Clear();
        if (retBorrowIdInFaultProcess != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Clear fault process borrowId failed. ret="
                << retBorrowIdInFaultProcess << ".";
        }
    }

    resp.freeFunc = [](uint8_t* p) {
        if (p != nullptr) {
            delete[] p;
        }
    };
    return ret;
}

void OverCommitFaultManagementHandler::MemIdReturnExecuteResHandler(void* ctx, const UbseByteBuffer& respData,
                                                                    uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit][FaultManagement] Ctx or respData is null.";
        return;
    }
    auto* result = static_cast<uint32_t*>(ctx);
    if (resCode != MEM_POOLING_OK || respData.len != MEMID_SUCCESS_RESPONSE_DATA_LENGTH) {
        *result = MEM_POOLING_ERROR;
        return;
    }
    *result = MEM_POOLING_OK;
}

// 故障numa处理：在借入节点上借用内存、迁出pid、归还内存
uint32_t OverCommitFaultManagementHandler::FaultNumaProcessRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    LOG_DEBUG << "FaultNumaProcessRecvHandler start.";
    FaultRecordsInNode faultRecordsInNode;
    RmrsInStream builder(req.data, req.len);
    builder >> faultRecordsInNode;
    MpResult ret = OverCommitFaultNodeModule::Instance().BorrowInNodeProcess(faultRecordsInNode);
    if (MEM_POOLING_OK != ret) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][FaultManagement] Recv FaultNumaProcessRecvHandler ret=" << ret << ".";
        resp.len = MEMID_FAIL_RESPONSE_DATA_LENGTH;
        resp.data = new (std::nothrow) uint8_t[resp.len]{};
        if (resp.data == nullptr) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(ret);
        resp.data[1] = 0;
    } else {
        resp.len = MEMID_SUCCESS_RESPONSE_DATA_LENGTH;
        resp.data = new (std::nothrow) uint8_t[resp.len]{};
        if (resp.data == nullptr) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(ret);
    }

    resp.freeFunc = [](uint8_t* p) {
        if (p != nullptr) {
            delete[] p;
        }
    };
    return ret;
}

void OverCommitFaultManagementHandler::FaultNumaProcessResHandler(void* ctx, const UbseByteBuffer& respData,
                                                                  uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        LOG_ERROR << "Ctx or respData is null.";
        return;
    }
    auto* result = static_cast<uint32_t*>(ctx);
    if (resCode != MEM_POOLING_OK || respData.len != MEMID_SUCCESS_RESPONSE_DATA_LENGTH) {
        *result = MEM_POOLING_ERROR;
        return;
    }
    *result = MEM_POOLING_OK;
}

// 故障处理：在借入节点上借用内存
uint32_t OverCommitFaultManagementHandler::FaultHandleMemBorrowRecvHandler(const UbseByteBuffer& req,
                                                                           UbseByteBuffer& resp)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultHandleMemBorrow] FaultHandleMemBorrowRecvHandler start.";
    if (req.data == nullptr || req.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleMemBorrow] FaultHandleMemBorrowRecvHandler req.data is null.";
        return MEM_POOLING_ERROR;
    }

    MemBorrowExecuteParam param;
    RmrsInStream builderIn(req.data, req.len);
    builderIn >> param;

    MemBorrowExecuteResult borrowExecuteResult;
    MpResult ret = MempoolBorrowModule::MemBorrowExecuteInOverCommit(
        param.srcParam, param.borrowSizes,
        mempooling::WaterMark({.highWaterMark = param.highWaterMark, .lowWaterMark = param.lowWaterMark}),
        borrowExecuteResult, true);
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultHandleMemBorrow] MemBorrowExecute Result=" << borrowExecuteResult.ToString() << ".";
    FaultHandleMemBorrowResult faultHandleMemBorrowResult = {.borrowIds = borrowExecuteResult.borrowIds,
                                                             .presentNumaId = borrowExecuteResult.presentNumaId};
    if (MEM_POOLING_OK != ret || borrowExecuteResult.borrowIds.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleMemBorrow] MemBorrowExecute Failed, ret=" << ret << ".";
        faultHandleMemBorrowResult.retCode = MEM_POOLING_ERROR;
    } else {
        faultHandleMemBorrowResult.retCode = MEM_POOLING_OK;
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultHandleMemBorrow] FaultHandleMemBorrow sucess.";
    }

    RmrsOutStream builderOut;
    builderOut << faultHandleMemBorrowResult;
    resp.data = builderOut.GetBufferPointer();
    resp.len = builderOut.GetSize();
    resp.freeFunc = [](uint8_t* data) {
        delete[] data;
    };

    return MEM_POOLING_OK;
}

void OverCommitFaultManagementHandler::FaultHandleMemBorrowResHandler(void* ctx, const UbseByteBuffer& respData,
                                                                      uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultHandleMemBorrowResHandler] invalid rpc response.";
        return;
    }

    auto* faultHandleMemBorrowResult = static_cast<FaultHandleMemBorrowResult*>(ctx);
    faultHandleMemBorrowResult->retCode = MEM_POOLING_ERROR;

    if (resCode != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleMemBorrowResHandler] rpc failed, res=" << resCode << ".";
        return;
    }

    RmrsInStream inBuilder(respData.data, respData.len);
    inBuilder >> (*faultHandleMemBorrowResult);
}

uint32_t SetHandlerResponseFromResult(UbseByteBuffer& resp, MpResult result)
{
    if (result != MEM_POOLING_OK) {
        resp.len = MEMID_FAIL_RESPONSE_DATA_LENGTH;
    } else {
        resp.len = MEMID_SUCCESS_RESPONSE_DATA_LENGTH;
    }
    resp.data = new (std::nothrow) uint8_t[resp.len]{};
    if (resp.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][FaultManagement] Failed to allocate memory, size=" << resp.len << ".";
        return MEM_POOLING_ERROR;
    }
    resp.data[0] = static_cast<uint8_t>(result);
    if (result != MEM_POOLING_OK) {
        resp.data[1] = 0;
    }
    resp.freeFunc = [](uint8_t* p) {
        if (p != nullptr) {
            delete[] p;
        }
    };
    return static_cast<uint32_t>(result);
}

static bool IsProcessAlive(pid_t pid)
{
    if (pid <= 0) {
        return false;
    }
    return kill(pid, 0) == 0 || errno != ESRCH;
}

static MpResult FreeBorrowRecordsDirectly(const std::vector<BorrowRecord>& records)
{
    MpResult finalRet = MEM_POOLING_OK;
    for (const auto& rec : records) {
        auto ret = MemBorrowExecutor::Instance().MemFreeWithOps(rec.name, true, false, true);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] FreeBorrowRecordsDirectly failed for " << rec.name;
            finalRet = MEM_POOLING_ERROR;
        }
    }
    return finalRet;
}

static std::vector<uint16_t> CollectFaultRemoteNumaIds(const SimplifiedFaultRecordsInNode& records)
{
    std::unordered_set<uint16_t> numaSet;
    for (const auto& [pid, borrowRecords] : records.pidBorrowMap) {
        for (const auto& rec : borrowRecords) {
            if (rec.borrowRemoteNuma >= 0) {
                numaSet.insert(static_cast<uint16_t>(rec.borrowRemoteNuma));
            }
        }
    }
    return {numaSet.begin(), numaSet.end()};
}

MpResult ProcessSimplifiedFaultPids(const SimplifiedFaultRecordsInNode& records,
                                    const ubse::task_executor::UbseTaskExecutorPtr& taskExecutor)
{
    if (taskExecutor == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][FaultManagement] Simplified fault task executor is not initialized.";
        return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
    }
    auto numaIds = CollectFaultRemoteNumaIds(records);
    FaultNumaReservedGuard reservedGuard;
    for (auto numaId : numaIds) {
        if (!FaultNumaReservedLock::Instance().TryReserve(numaId)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Fault source NUMA already reserved, numaId=" << numaId << ".";
            return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
        }
        reservedGuard.numaIds.push_back(numaId);
    }
    FaultNumaLockGuard lockGuard;
    for (auto numaId : numaIds) {
        FaultNumaLock::Instance().AcquireExclusive(numaId);
        lockGuard.exclusiveNumaIds.push_back(numaId);
    }

    // 进程占用大小升序排序：小占用进程优先处理（RFC 占用大小排序）
    std::vector<std::pair<pid_t, uint64_t>> pidSizeList;
    for (const auto& entry : records.pidBorrowMap) {
        uint64_t totalSize = 0;
        for (const auto& rec : entry.second) {
            totalSize += rec.size;
        }
        pidSizeList.emplace_back(entry.first, totalSize);
    }
    std::sort(pidSizeList.begin(), pidSizeList.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) {
            return a.second < b.second;
        }
        return a.first < b.first;
    });

    // 节点内进程级并行：使用 OverCommitFaultManagementHandler 构造时初始化的线程池（并行度 4）
    std::atomic<int> failCount{0};
    std::atomic<MpResult> highestPriorityError{MEM_POOLING_OK};
    for (const auto& pidSizePair : pidSizeList) {
        pid_t pid = pidSizePair.first;
        int64_t startTime = records.pidStartTimeMap.count(pid) ? records.pidStartTimeMap.at(pid) : 0;
        std::vector<BorrowRecord> borrowRecords = records.pidBorrowMap.at(pid);
        std::vector<SimplifiedFaultPidAllocTarget> allocTargets;
        auto allocIt = records.pidAllocMap.find(pid);
        if (allocIt != records.pidAllocMap.end()) {
            allocTargets = allocIt->second;
        }
        if (!taskExecutor->Execute([pid, startTime, borrowRecords, allocTargets, &failCount, &highestPriorityError]() {
                MpResult pidResult = MEM_POOLING_OK;
                if (!IsProcessAlive(pid)) {
                    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
                        << "[OverCommit][FaultManagement] Process dead, directly freeing borrowIds, pid=" << pid;
                    if (FreeBorrowRecordsDirectly(borrowRecords) != MEM_POOLING_OK) {
                        pidResult = MEM_POOLING_FAULT_RETURN_MEM_ERROR;
                    }
                } else {
                    pidResult = ProcessSinglePidFault(pid, startTime, borrowRecords, allocTargets);
                }
                if (pidResult != MEM_POOLING_OK) {
                    UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                        << "pid = " << pid << " return error code " << pidResult;
                    failCount++;
                    // 记录最高优先级错误码（而非首个失败码）
                    MpResult expected = highestPriorityError.load();
                    do {
                        if (expected != MEM_POOLING_OK && get_higher_priority_error(expected, pidResult) == expected) {
                            break; // 当前已持有更高优先级错误码，无需更新
                        }
                    } while (!highestPriorityError.compare_exchange_weak(expected, pidResult));
                }
            })) {
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Executor submit failed, pid=" << pid;
            failCount++;
            MpResult expected = MEM_POOLING_OK;
            highestPriorityError.compare_exchange_strong(expected, MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR);
        }
    }
    taskExecutor->Wait();
    // 线程池由 OverCommitFaultManagementHandler 持有，进程生命周期内复用，不在每次调用后 Stop

    if (failCount == 0) {
        return MEM_POOLING_OK;
    }

    return highestPriorityError.load();
}

uint32_t OverCommitFaultManagementHandler::SimplifiedFaultNumaProcessRecvHandler(const UbseByteBuffer& req,
                                                                                 UbseByteBuffer& resp)
{
    LOG_DEBUG << "SimplifiedFaultNumaProcessRecvHandler start.";
    RmrsInStream in(req.data, req.len);
    SimplifiedFaultRecordsInNode records;
    if (SimplifiedFaultRecordsInNodeDeserialization(in, records) != MEM_POOLING_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][FaultManagement] SimplifiedFaultNumaProcessRecvHandler deserialization failed.";
        return SetHandlerResponseFromResult(resp, MEM_POOLING_ERROR);
    }

    MpResult finalResult =
        ProcessSimplifiedFaultPids(records, SimplifiedFaultTaskExecutor::Instance().GetTaskExecutor());

    LOG_DEBUG << "SimplifiedFaultNumaProcessRecvHandler end, result=" << finalResult << ".";
    return SetHandlerResponseFromResult(resp, finalResult);
}

void OverCommitFaultManagementHandler::SimplifiedFaultNumaProcessResHandler(void* ctx, const UbseByteBuffer& respData,
                                                                            uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        LOG_ERROR << "SimplifiedFaultNumaProcessResHandler ctx or respData is null.";
        return;
    }
    auto* result = static_cast<uint32_t*>(ctx);
    // 优先读 payload 中回填的具体错误码(单字节);仅当 payload 无效时 fallback 到 resCode
    if (respData.len == MEMID_SUCCESS_RESPONSE_DATA_LENGTH || respData.len == MEMID_FAIL_RESPONSE_DATA_LENGTH) {
        *result = static_cast<uint32_t>(respData.data[0]);
        return;
    }
    if (resCode != MEM_POOLING_OK) {
        *result = resCode;
        return;
    }
    *result = MEM_POOLING_ERROR;
}

} // namespace mempooling::over_commit