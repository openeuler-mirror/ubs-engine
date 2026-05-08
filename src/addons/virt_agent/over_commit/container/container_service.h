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

#ifndef VM_CONTAINER_SERVICE_H
#define VM_CONTAINER_SERVICE_H

#include <unordered_map>
#include <unordered_set>

#include "vm_mem_manager.h"

namespace vm::overcommit {

class ContainerService : public VmMemManager {
public:
    using VmMemManager::VmMemManager;
    ContainerService() = default;
    void Run() override;

public:
    VmResult MemBorrow(const NodeLocInfo &nodeLocInfo, const std::vector<uint64_t> &borrowSizes,
                       const WaterMark &waterMark, const UserInfo &userInfo, MemBorrowExecuteResult &borrowResult);

    VmResult MemMigrate(const NodeLocInfo &nodeLocInfo, const std::unordered_set<std::string> &borrowIdSet,
                        const std::vector<VMPresetParam> &vmPresetParamList);

    VmResult MemReturn(const NodeLocInfo &nodeLocInfo, const std::vector<std::string> &borrowIds,
                       const std::vector<pid_t> &pids);

private:
    void BuildBorrowTask(const NodeLocInfo &nodeLocInfo, const std::vector<uint64_t> &borrowSizes,
                         const WaterMark &waterMark, const UserInfo &userInfo);
    void BuildMigrateTask(const NodeLocInfo &nodeLocInfo, const std::vector<std::string> &borrowIds,
                          const std::vector<uint16_t> &presentNumaIds, const std::vector<VMPresetParam> &vmPresetParam);
    void BuildReturnTask(const NodeLocInfo &nodeLocInfo, const std::vector<std::string> &borrowIds,
                         const std::vector<pid_t> &pids);
};
} // namespace vm::overcommit

#endif // VM_CONTAINER_SERVICE_H
