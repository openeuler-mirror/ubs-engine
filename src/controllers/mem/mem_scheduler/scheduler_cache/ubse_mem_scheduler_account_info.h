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
#ifndef UBSE_MEM_SCHEDULER_ACCOUNT_H
#define UBSE_MEM_SCHEDULER_ACCOUNT_H
#include <cstdint>
#include <string>
#include "ubse_noncopyable.h"
#include "../ubse_mem_types.h"

namespace ubse::mem::scheduler {
class SchedulerNodeManager;
enum class BorrowedType
{
    FD,   // Fd借用
    NUMA, // Numa借用
    ADDR, // 指定地址借用
    SHM,  // 共享内存借用
};

enum class AccountState
{
    IMPORT_EXPORT_EXIST,
    ONLY_EXPORT_EXIST,
    ONLY_IMPORT_EXIST,
    BOTH_NOT_EXIST,
};

struct SchedulerAccountID {
    std::string requestName{};
    std::string nodeId{};
    BorrowedType type;

    bool operator<(const SchedulerAccountID& other) const
    {
        if (requestName != other.requestName) {
            return requestName < other.requestName;
        }
        if (nodeId != other.nodeId) {
            return nodeId < other.nodeId;
        }
        return type < other.type;
    }
};

class SchedulerAccountInfo {
public:
    SchedulerAccountInfo(std::vector<ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo> importNumaLocs,
                         std::vector<ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo> exportNumaLocs, BorrowedType type,
                         uint32_t blockSize);

    [[nodiscard]] std::string GetExportNodeId() const
    {
        return exportNumaLocs_[0].nodeId;
    }
    [[nodiscard]] std::string GetImportNodeId() const
    {
        return importNumaLocs_.empty() ? "" : importNumaLocs_[0].nodeId;
    }
    [[nodiscard]] AccountState GetAccountState() const
    {
        return state_;
    }

    [[nodiscard]] const std::vector<ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo>& GetExportNumaLocs() const
    {
        return exportNumaLocs_;
    }

    void UpdateAccountState(adapter_plugins::mmi::UbseMemState state, SchedulerNodeManager* node);

    ~SchedulerAccountInfo() = default;

private:
    void UpdateByBothNotExist(adapter_plugins::mmi::UbseMemState state, SchedulerNodeManager* node);
    void UpdateStateByOnlyExportExist(adapter_plugins::mmi::UbseMemState state, SchedulerNodeManager* node);
    void UpdateStateByOnlyImportExist(adapter_plugins::mmi::UbseMemState state, SchedulerNodeManager* node);
    void UpdateStateByBothExist(adapter_plugins::mmi::UbseMemState state, SchedulerNodeManager* node);

    void AddNumaBorrow(SchedulerNodeManager* node) const;
    void SubNumaBorrow(SchedulerNodeManager* node) const;
    void AddNumaLend(SchedulerNodeManager* node) const;
    void SubNumaLend(SchedulerNodeManager* node) const;
    void AddNumaShare(SchedulerNodeManager* node) const;
    void SubNumaShare(SchedulerNodeManager* node) const;

    std::vector<ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo> importNumaLocs_;
    std::vector<ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo> exportNumaLocs_;
    uint32_t blockSize_;
    BorrowedType type_;
    AccountState state_{AccountState::BOTH_NOT_EXIST};
};

} // namespace ubse::mem::scheduler
#endif // UBSE_MEM_SCHEDULER_ACCOUNT_H
