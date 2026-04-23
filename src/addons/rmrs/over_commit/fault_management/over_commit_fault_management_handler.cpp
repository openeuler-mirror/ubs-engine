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

#include "over_commit_fault_management_handler.h"
#include "over_commit_fault_memid_module.h"
#include "mem_borrow_executor.h"
#include "over_commit_fault_node_module.h"

namespace mempooling::over_commit {

const int MEMID_SUCCESS_RESPONSE_DATA_LENGTH = 1;
const int MEMID_FAIL_RESPONSE_DATA_LENGTH = 2;

// 故障处理相关Handler
// memid级别故障处理：获取节点上的VM numa info
uint32_t OverCommitFaultManagementHandler::GetVmNumaInfoMapRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[OverCommit][FaultManagement] GetVmNumaInfoMapRecvHandler start.";

    OverCommitFaultVmNumaInfoParam param{};
    RmrsInStream inBuilder(req.data, req.len);
    inBuilder >> param;
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    MpResult ret =
        OverCommitFaultMemIdModule::Instance().GetRemoteNumaVms(param.remoteNumaId, vmNumaInfoWithSocketList);

    OverCommitFaultVmNumaInfoResult result{.vmNumaInfoWithSocketList = vmNumaInfoWithSocketList};
    RmrsOutStream builder;
    builder << result;
    resp.len = builder.GetSize();
    resp.data = builder.GetBufferPointer();
    resp.freeFunc = [](uint8_t *data) {
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

void OverCommitFaultManagementHandler::GetVmNumaInfoMapResHandler(void *ctx, const UbseByteBuffer &respData,
                                                                  uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit][FaultManagement] Ctx or respData is null.";
        return;
    }
    OverCommitFaultVmNumaInfoResult result;
    auto *overCommitFaultVmNumaInfoResult = static_cast<OverCommitFaultVmNumaInfoResult *>(ctx);
    if (resCode != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][FaultManagement] Send error, res=" << resCode << ".";
    } else {
        RmrsInStream builder(respData.data, respData.len);
        builder >> result;
    }
    *overCommitFaultVmNumaInfoResult = result;
}

// memid级别故障处理：执行大页配置、setRemoteNumaInfo
uint32_t OverCommitFaultManagementHandler::MemIdExecuteRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit][FaultManagement] MemIdExecuteRecvHandler start.";

    OverCommitFaultMemIdExecuteParam param{};
    RmrsInStream builder(req.data, req.len);
    builder >> param;
    MpResult res = OverCommitFaultMemIdModule::Instance().MemIdExecute(param);
    if (MEM_POOLING_OK != res) {
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

    resp.freeFunc = [](uint8_t *p) {
        if (p != nullptr) {
            delete[] p;
        }
    };
    return res;
}
void OverCommitFaultManagementHandler::MemIdExecuteResHandler(void *ctx, const UbseByteBuffer &respData,
                                                              uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit][FaultManagement] Ctx or respData is null.";
        return;
    }
    auto *result = static_cast<uint32_t *>(ctx);
    if (resCode != MEM_POOLING_OK || respData.len != MEM_POOLING_ERROR) {
        *result = MEM_POOLING_ERROR;
        return;
    }
    *result = MEM_POOLING_OK;
}

// memid级别故障处理：不迁回，直接归还
uint32_t OverCommitFaultManagementHandler::MemIdReturnDirectlyExecuteRecvHandler(const UbseByteBuffer &req,
                                                                                 UbseByteBuffer &resp)
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

    resp.freeFunc = [](uint8_t *p) {
        if (p != nullptr) {
            delete[] p;
        }
    };
    return ret;
}

// pid级别关闭冷热流动
uint32_t OverCommitFaultManagementHandler::DisableSmapProcessMigrateRecvHandler(const UbseByteBuffer &req,
                                                                                UbseByteBuffer &resp)
{
    std::vector<pid_t> pids;
    RmrsInStream builder(req.data, req.len);
    builder >> pids;
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
        resp.len = MEMID_SUCCESS_RESPONSE_DATA_LENGTH;
        resp.data = new (std::nothrow) uint8_t[resp.len]{};
        if (resp.data == nullptr) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit][FaultManagement] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(retSmap);
    }
    resp.freeFunc = [](uint8_t *p) {
        if (p != nullptr) {
            delete[] p;
        }
    };
    return retSmap;
}

void OverCommitFaultManagementHandler::DisableSmapProcessMigrateResHandler(void *ctx, const UbseByteBuffer &respData,
                                                                           uint32_t resCode)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "DisableSmapProcessMigrateResHandler resCode = " << resCode;
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit][FaultManagement] Ctx or respData is null.";
        return;
    }
    auto *result = static_cast<uint32_t *>(ctx);
    if (resCode != MEM_POOLING_OK || respData.len != MEM_POOLING_ERROR) {
        *result = MEM_POOLING_ERROR;
        return;
    }
    *result = MEM_POOLING_OK;
}

void OverCommitFaultManagementHandler::MemIdReturnDirectlyExecuteResHandler(void *ctx, const UbseByteBuffer &respData,
                                                                            uint32_t resCode)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "MemIdReturnDirectlyExecuteResHandler resCode = " << resCode;
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit][FaultManagement] Ctx or respData is null.";
        return;
    }
    auto *result = static_cast<uint32_t *>(ctx);
    if (resCode != MEM_POOLING_OK || respData.len != MEM_POOLING_ERROR) {
        *result = MEM_POOLING_ERROR;
        return;
    }
    *result = MEM_POOLING_OK;
}

// memid级别故障处理：内存迁回 + 归还
uint32_t OverCommitFaultManagementHandler::MemIdReturnExecuteRecvHandler(const UbseByteBuffer &req,
                                                                         UbseByteBuffer &resp)
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
    }

    resp.freeFunc = [](uint8_t *p) {
        if (p != nullptr) {
            delete[] p;
        }
    };
    return ret;
}

void OverCommitFaultManagementHandler::MemIdReturnExecuteResHandler(void *ctx, const UbseByteBuffer &respData,
                                                                    uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit][FaultManagement] Ctx or respData is null.";
        return;
    }
    auto *result = static_cast<uint32_t *>(ctx);
    if (resCode != MEM_POOLING_OK || respData.len != MEM_POOLING_ERROR) {
        *result = MEM_POOLING_ERROR;
        return;
    }
    *result = MEM_POOLING_OK;
}

// 故障numa处理：在借入节点上借用内存、迁出pid、归还内存
uint32_t OverCommitFaultManagementHandler::FaultNumaProcessRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
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

    resp.freeFunc = [](uint8_t *p) {
        if (p != nullptr) {
            delete[] p;
        }
    };
    return ret;
}

void OverCommitFaultManagementHandler::FaultNumaProcessResHandler(void *ctx, const UbseByteBuffer &respData,
                                                                  uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        LOG_ERROR << "Ctx or respData is null.";
        return;
    }
    auto *result = static_cast<uint32_t *>(ctx);
    if (resCode != MEM_POOLING_OK || respData.len != MEM_POOLING_ERROR) {
        *result = MEM_POOLING_ERROR;
        return;
    }
    *result = MEM_POOLING_OK;
}

} // namespace mempooling::over_commit