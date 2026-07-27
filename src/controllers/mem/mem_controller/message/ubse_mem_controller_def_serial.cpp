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
#include "ubse_mem_controller_def_serial.h"

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"

namespace ubse::mem::controller::message {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::serial;

static bool UbseNodeBorrowInfoSerialize(UbseSerialization& serialization, const def::UbseNodeBorrowInfo& nodeBorrowInfo)
{
    serialization << nodeBorrowInfo.borrowSlotId << nodeBorrowInfo.borrowHostname << nodeBorrowInfo.lendSlotId
                  << nodeBorrowInfo.lendHostname << nodeBorrowInfo.size;
    return serialization.Check();
}

static bool UbseNodeBorrowInfoDeserialize(UbseDeSerialization& deSerialization, def::UbseNodeBorrowInfo& nodeBorrowInfo)
{
    deSerialization >> nodeBorrowInfo.borrowSlotId >> nodeBorrowInfo.borrowHostname >> nodeBorrowInfo.lendSlotId >>
        nodeBorrowInfo.lendHostname >> nodeBorrowInfo.size;
    return deSerialization.Check();
}

bool UbseNodeBorrowInfosSerialize(UbseSerialization& serialization,
                                  const std::vector<def::UbseNodeBorrowInfo>& nodeBorrowInfos)
{
    serialization << array_len_insert(nodeBorrowInfos.size());
    for (const auto& nodeBorrowInfo : nodeBorrowInfos) {
        if (!UbseNodeBorrowInfoSerialize(serialization, nodeBorrowInfo)) {
            UBSE_LOG_ERROR << "Serialize UbseNodeBorrowInfo failed";
            return false;
        }
    }
    return true;
}

bool UbseNodeBorrowInfosDeserialize(UbseDeSerialization& deSerialization,
                                    std::vector<def::UbseNodeBorrowInfo>& nodeBorrowInfos)
{
    nodeBorrowInfos.clear();
    size_t size;
    deSerialization >> array_len_capture(size);
    if (!deSerialization.Check()) {
        return false;
    }
    for (size_t i = 0; i < size; i++) {
        def::UbseNodeBorrowInfo nodeBorrowInfo{};
        if (!UbseNodeBorrowInfoDeserialize(deSerialization, nodeBorrowInfo)) {
            UBSE_LOG_ERROR << "Deserialize UbseNodeBorrowInfo failed";
            return false;
        }
        nodeBorrowInfos.push_back(nodeBorrowInfo);
    }
    return true;
}

bool UbseCliShmDescSerialize(const ubse::mem::def::UbseMemShmDesc& shmDesc, const std::string& importNodeId,
                             UbseSerialization& serialization)
{
    serialization << shmDesc.name << shmDesc.totalMemSize << shmDesc.exportNode.slotId << enum_v(shmDesc.state);
    uint32_t importSlotId = 0;
    std::vector<uint64_t> memIds;
    ubse::mem::controller::UbseMemStage importState = UbseMemStage::UBSE_NOT_EXIST;
    for (const auto& importDesc : shmDesc.importDesc) {
        if (std::to_string(importDesc.importNode.slotId) == importNodeId) {
            importSlotId = importDesc.importNode.slotId;
            memIds = importDesc.memIds;
            importState = importDesc.state;
            break;
        }
    }
    serialization << importSlotId << enum_v(importState);
    serialization << array_len_insert(memIds.size());
    for (auto memId : memIds) {
        serialization << memId;
    }
    serialization << array_len_insert(shmDesc.region.size());
    for (auto slotId : shmDesc.region) {
        serialization << slotId;
    }
    return serialization.Check();
}
} // namespace ubse::mem::controller::message
