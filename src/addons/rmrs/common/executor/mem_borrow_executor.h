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

#ifndef MEM_BORROW_EXECUTOR_H
#define MEM_BORROW_EXECUTOR_H

#include <fstream>
#include <ostream>
#include <string>

#include "ubse_mem_controller.h"
#include "mem_json_def.h"
#include "mem_manager.h"
#include "mp_error.h"
#include "mp_smap_controller.h"

namespace mempooling {

using namespace mempooling::smap;
using namespace ubse::mem::controller;

class MemBorrowExecutor {
public:
    static MemBorrowExecutor& Instance()
    {
        static MemBorrowExecutor instance;
        return instance;
    }

    MpResult MemBorrow(const std::string& attachNode, const RackCreateResourceWaterBorrowAttr& attr,
                       std::string& borrowId, int16_t& presentNumaId, bool isBorrowIdPersistence = true);
    MpResult MemFree(const std::string& name);

    MpResult MemFreeWithOps(const std::string& name, bool isForceDelete, bool smapBack, bool isFault = false);

    MpResult MemFreeWithOpsBySmap(const std::string& name, const std::string& deleteName, bool isFault = false);

    MpResult MemFreeWithOpsByMemfabric(const std::string &name, const std::string &deleteName, bool isFault = false);

    MpResult HandleTimeoutFree(const std::string& name, const std::vector<BorrowRecord>& borrowRecords);
    
    MpResult GetBorrowRecordForSmapParams(const std::string &name, BorrowRecord &record, bool isFault);

    MpResult GenerateSmapParams(const std::string &name, MigrateBackMsg &migrateBackMsg, EnableNodeMsg &enableMsg,
                                std::string &importNodeId, bool isFault = false);
    MpResult SmapMigreatBackRpc(const std::string importNodeId, const MigrateBackMsg &migrateBackMsg);

    MpResult GenerateUniqueId(const std::string& nodeId, std::string& str, const bool isFault = false);

    MpResult RemoveBorrowIdRedirectionRecursively(const std::string& name);

    MpResult PrepareMemNumaCreateParams(const std::string attachNode, const RackCreateResourceWaterBorrowAttr& attr,
                                        UbseMemBorrower& borrower, std::vector<UbseMemNumaLender>& lenders,
                                        uint8_t usrInfo[ubse::mem::controller::UBSE_MAX_USR_INFO_LEN]);

private:
    MemBorrowExecutor() = default;
    ~MemBorrowExecutor() = default;
    MemBorrowExecutor(const MemBorrowExecutor&) = delete;
    MemBorrowExecutor& operator=(const MemBorrowExecutor&) = delete;
};

class MpMemBorrowExecutorModule : public MpSubModule {
public:
    MpResult Init() override
    {
        // 注册内存迁出策略消息处理器
        auto ret = DeleteFailedBorrowIds();
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MSG] Failed to clean up the deprecated memory resource.";
            // 可靠性保障，不阻塞主功能
        }
        return MEM_POOLING_OK;
    }
    void DeInit() override
    {
        return;
    }
    const std::string Name() override
    {
        return "BorrowExecutor";
    }

private:
    MpResult DeleteFailedBorrowIds();
};

} // namespace mempooling

#endif // MEM_BORROW_EXECUTOR_H
