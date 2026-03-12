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

#include "container_service.h"

#include <ubse_logger.h>
#include "vm_mem_adapter.h"

namespace vm::overcommit {
UBSE_DEFINE_THIS_MODULE("vm_plugin");
void ContainerService::Run() {}

VmResult ContainerService::MemBorrow(const NodeLocInfo &nodeLocInfo, const std::vector<uint64_t> &borrowSizes,
                                     const WaterMark &waterMark, const UserInfo &userInfo,
                                     MemBorrowExecuteResult &borrowResult)
{
    BuildBorrowTask(nodeLocInfo, borrowSizes, waterMark, userInfo);
    if (!RunPreflight(MemOpType::BORROW)) {
        UBSE_LOG_ERROR << "Precondition not ready.";
        return VM_ERROR;
    }

    return RunMemBorrow(borrowResult);
}

VmResult ContainerService::MemMigrate(const NodeLocInfo &nodeLocInfo,
                                      const std::unordered_set<std::string> &borrowIdSet,
                                      const std::vector<VMPresetParam> &vmPresetParamList)
{
    std::unordered_map<std::string, uint16_t> borrowRemoteMaps;
    auto ret = VmMemAdapter::GetMemoryRemoteNumaIds(borrowIdSet, borrowRemoteMaps);
    if (ret != VM_OK) {
        return ret;
    }
    std::vector<std::string> borrowIds;
    std::vector<uint16_t> presentNumaIds;
    for (const auto &borrow : borrowRemoteMaps) {
        borrowIds.push_back(borrow.first);
        presentNumaIds.push_back(borrow.second);
    }

    BuildMigrateTask(nodeLocInfo, borrowIds, presentNumaIds, vmPresetParamList);
    if (!RunPreflight(MemOpType::MIGRATE)) {
        UBSE_LOG_ERROR << "Precondition not ready.";
        return VM_ERROR;
    }

    return OutMemMigrate();
}

VmResult ContainerService::MemReturn(const NodeLocInfo &nodeLocInfo, const std::vector<std::string> &borrowIds,
                                     const std::vector<pid_t> &pids)
{
    BuildReturnTask(nodeLocInfo, borrowIds, pids);
    if (!RunPreflight(MemOpType::RETURN)) {
        UBSE_LOG_ERROR << "Precondition not ready.";
        return VM_ERROR;
    }

    return OutMemReturn();
}

void ContainerService::BuildBorrowTask(const NodeLocInfo &nodeLocInfo, const std::vector<uint64_t> &borrowSizes,
                                       const WaterMark &waterMark, const UserInfo &userInfo)
{
    this->memOpStruct.type = MemOpType::BORROW;
    this->memOpStruct.nodeLoc = nodeLocInfo;
    this->memOpStruct.memOpMetadata.borrowSizes = borrowSizes;
    this->memOpStruct.memOpMetadata.waterMark = waterMark;
    this->memOpStruct.memOpMetadata.userInfo = userInfo;
}

void ContainerService::BuildMigrateTask(const NodeLocInfo &nodeLocInfo, const std::vector<std::string> &borrowIds,
                                        const std::vector<uint16_t> &presentNumaIds,
                                        const std::vector<VMPresetParam> &vmPresetParam)
{
    this->memOpStruct.type = MemOpType::MIGRATE;
    this->memOpStruct.nodeLoc = nodeLocInfo;
    this->memOpStruct.memOpMetadata.borrowIds = borrowIds;
    this->memOpStruct.memOpMetadata.presentNumaIds = presentNumaIds;
    this->memOpStruct.memOpMetadata.vmPresetParam = vmPresetParam;
}

void ContainerService::BuildReturnTask(const NodeLocInfo &nodeLocInfo, const std::vector<std::string> &borrowIds,
                                       const std::vector<pid_t> &pids)
{
    this->memOpStruct.type = MemOpType::RETURN;
    this->memOpStruct.nodeLoc = nodeLocInfo;
    this->memOpStruct.memOpMetadata.borrowIds = borrowIds;
    this->memOpStruct.memOpMetadata.pids = pids;
}

} // namespace vm::overcommit