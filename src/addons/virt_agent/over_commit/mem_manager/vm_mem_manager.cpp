/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "vm_mem_manager.h"

#include <ubse_logger.h>

namespace vm::overcommit {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;

VmMemManager::VmMemManager(MemOpStruct memOpStruct) : memOpStruct(std::move(memOpStruct)) {}

bool VmMemManager::RunPreflight(MemOpType type) const
{
    bool check = true;
    check &= memOpStruct.type == type;
    return check;
}

VmResult VmMemManager::RunMemBorrow(MemBorrowExecuteResult& borrowResult)
{
    MemOpMetadata borrowData = memOpStruct.memOpMetadata;

    const SrcMemoryBorrowParam borrowParam = {
        .srcNid = memOpStruct.nodeLoc.hostId,
        .srcSocketId = memOpStruct.nodeLoc.socketId,
        .srcNumaId = memOpStruct.nodeLoc.numaId,
        .uid = memOpStruct.memOpMetadata.userInfo.uid,
        .username = memOpStruct.memOpMetadata.userInfo.username,
    };
    // Memory Borrowing (Interface Provided by RMRS Component)
    const auto UBSRMRSMemBorrow = MempoolingModule::UBSRMRSMemBorrow();
    if (UBSRMRSMemBorrow == nullptr) {
        UBSE_LOG_ERROR << "[borrow] get mem borrow function ptr failed.";
        return VM_ERROR;
    }
    try {
        uint32_t res = UBSRMRSMemBorrow(borrowParam, borrowData.borrowSizes, borrowData.waterMark, borrowResult);
        if (VmResultFail(res)) {
            UBSE_LOG_ERROR << "[borrow] borrow memory failed, task end. " << FormatRetCode(res);
            return VM_ERROR;
        }
        // If the borrowed result indicates that `borrowIds` is empty, it is considered a borrowing failure.
        // It needs to be removed from the result set.
        CleanEmptyBorrowRes(borrowResult);
        if (borrowResult.borrowIds.empty()) {
            UBSE_LOG_ERROR << "[borrow] borrow result is empty, borrow memory failed, task end.";
            return VM_ERROR;
        }
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "[borrow] run UBSRMRSMemBorrow failed, task end. Error message: " << e.what();
        return VM_ERROR;
    }

    UBSE_LOG_INFO << "[borrow] succeed to borrow memory.";
    return VM_OK;
}

VmResult VmMemManager::OutMemMigrate()
{
    const auto UBSRMRSMemMigrate = MempoolingModule::UBSRMRSMemMigrate();
    if (UBSRMRSMemMigrate == nullptr) {
        UBSE_LOG_ERROR << "[borrow] get mem migrate function ptr failed.";
        return VM_ERROR;
    }
    MemOpMetadata migrateData = memOpStruct.memOpMetadata;
    const SrcMemoryBorrowParam borrowParam = {
        .srcNid = memOpStruct.nodeLoc.hostId,
        .srcSocketId = memOpStruct.nodeLoc.socketId,
        .srcNumaId = memOpStruct.nodeLoc.numaId,
    };
    MemBorrowExecuteResult borrowResult;
    borrowResult.borrowIds = migrateData.borrowIds;
    borrowResult.presentNumaIds = migrateData.presentNumaIds;
    try {
        VmResult ret = UBSRMRSMemMigrate(borrowParam, migrateData.vmPresetParam, borrowResult);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "[migrate] migrate failed, start to rollback borrow mem. " << FormatRetCode(ret);
            return VM_ERROR;
        }
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "[migrate] run UBSRMRSMemMigrate failed. Error message: " << e.what();
        return VM_ERROR;
    }

    UBSE_LOG_INFO << "[migrate] succeed to migrate memory.";
    return VM_OK;
}

VmResult VmMemManager::OutMemReturn()
{
    MemOpMetadata returnData = memOpStruct.memOpMetadata;
    UBSE_LOG_INFO << "[return] vm memory return for node=" << memOpStruct.nodeLoc.hostId
                  << ", numaId=" << memOpStruct.nodeLoc.numaId
                  << ", memNames=" << VectorUtil::VectorToString(returnData.borrowIds);

    const SrcMemoryBorrowParam borrowParam = {
        .srcNid = memOpStruct.nodeLoc.hostId,
        .srcSocketId = memOpStruct.nodeLoc.socketId,
        .srcNumaId = memOpStruct.nodeLoc.numaId,
    };
    const auto UBSRMRSMemReturn = MempoolingModule::UBSRMRSMemReturn();
    if (UBSRMRSMemReturn == nullptr) {
        UBSE_LOG_ERROR << "[return] get mem return function ptr failed.";
        return VM_ERROR;
    }
    try {
        VmResult ret = UBSRMRSMemReturn(borrowParam, returnData.borrowIds, returnData.pids);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "[return] return memory failed, " << FormatRetCode(ret);
            return VM_ERROR;
        }
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "[return] run UBSRMRSMemReturn failed. Error message: " << e.what();
        return VM_ERROR;
    }

    UBSE_LOG_INFO << "[return] succeed to return memory.";
    return VM_OK;
}

void VmMemManager::CleanEmptyBorrowRes(MemBorrowExecuteResult& result)
{
    // Create a temporary storage vector
    std::vector<std::string> newBorrowIds;
    std::vector<uint16_t> newPresentNumaId;

    // Traverse borrowIds and presentNumaId. If borrowIds is not empty, add new borrowIds and presentNumaId.
    for (size_t i = 0; i < result.borrowIds.size(); ++i) {
        if (!result.borrowIds[i].empty()) {
            newBorrowIds.push_back(result.borrowIds[i]);
            newPresentNumaId.push_back(result.presentNumaIds[i]);
        }
    }

    // Replace old borrowed information with new information.
    result.borrowIds.swap(newBorrowIds);
    result.presentNumaIds.swap(newPresentNumaId);
}
} // namespace vm::overcommit