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
#ifndef UBSE_MEM_SCHEDULER_ACCOUNT_MANAGER_H
#define UBSE_MEM_SCHEDULER_ACCOUNT_MANAGER_H
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <typeinfo>

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_types.h"
#include "ubse_mmi_def.h"
#include "ubse_noncopyable.h"
#include "scheduler_cache/ubse_mem_scheduler_account_info.h"

namespace ubse::mem::scheduler {
class SchedulerNodeManager;
#define MODULE_LOG_NAME "ubse_mem_scheduler"

template <typename T>
struct MemObjType;

template <>
struct MemObjType<adapter_plugins::mmi::UbseMemFdBorrowImportObj> {
    static constexpr BorrowedType TYPE = BorrowedType::FD;
};
template <>
struct MemObjType<adapter_plugins::mmi::UbseMemFdBorrowExportObj> {
    static constexpr BorrowedType TYPE = BorrowedType::FD;
};
template <>
struct MemObjType<adapter_plugins::mmi::UbseMemNumaBorrowImportObj> {
    static constexpr BorrowedType TYPE = BorrowedType::NUMA;
};
template <>
struct MemObjType<adapter_plugins::mmi::UbseMemNumaBorrowExportObj> {
    static constexpr BorrowedType TYPE = BorrowedType::NUMA;
};
template <>
struct MemObjType<adapter_plugins::mmi::UbseMemShareBorrowImportObj> {
    static constexpr BorrowedType TYPE = BorrowedType::SHM;
};
template <>
struct MemObjType<adapter_plugins::mmi::UbseMemShareBorrowExportObj> {
    static constexpr BorrowedType TYPE = BorrowedType::SHM;
};
template <>
struct MemObjType<adapter_plugins::mmi::UbseMemAddrBorrowImportObj> {
    static constexpr BorrowedType TYPE = BorrowedType::ADDR;
};
template <>
struct MemObjType<adapter_plugins::mmi::UbseMemAddrBorrowExportObj> {
    static constexpr BorrowedType TYPE = BorrowedType::ADDR;
};

struct NumaBorrowInfo {
    std::set<uint32_t> borrowedByRequestor;
    std::map<uint32_t, uint32_t> borrowerCount;
};

class SchedulerAccountManager : Noncopyable {
public:
    SchedulerAccountManager() = default;

    // 非拥有语义：node_ 的生命周期由 SchedulerImpl 管理，SchedulerImpl 保证 node_ 先销毁
    void SetNodeManager(SchedulerNodeManager* node)
    {
        node_ = node;
    }

    UbseResult UpdateSchedulerAccount(const std::string& name,
                                      const adapter_plugins::mmi::UbseMemAlgoResult& schedulerResult,
                                      adapter_plugins::mmi::UbseMemState state, BorrowedType type);

    // Filter support
    [[nodiscard]] uint64_t GetTotalBorrowedSize(const std::string& name) const;
    [[nodiscard]] bool HasBorrowedFrom(const NodeId& importNodeId, const NodeId& exportNodeId) const;
    [[nodiscard]] bool HasBorrowedFromSocket(const NodeId& importNodeId, const NodeId& exportNodeId,
                                             SocketId socketId) const;
    [[nodiscard]] uint32_t CountDistinctLenders(const NodeId& importNodeId) const;
    [[nodiscard]] uint32_t CountDistinctBorrowers(const NodeId& exportNodeId) const;

    [[nodiscard]] NumaBorrowInfo GetNumaBorrowInfo(const NodeId& exportNodeId, SocketId socketId,
                                                   const NodeId& importNodeId) const;

    void Clear();

private:
    std::map<SchedulerAccountID, SchedulerAccountInfo> borrowAccount_;
    std::map<SchedulerAccountID, SchedulerAccountInfo> shareAccount_;
    SchedulerNodeManager* node_{};
};
#undef MODULE_LOG_NAME
} // namespace ubse::mem::scheduler
#endif // UBSE_MEM_SCHEDULER_ACCOUNT_MANAGER_H
