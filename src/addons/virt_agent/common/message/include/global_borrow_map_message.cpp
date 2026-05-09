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

#include "global_borrow_map_message.h"

#include "vm_def.h"
#include "vm_serial_util.h"

namespace vm {
VmResult GlobalBorrowMapMessage::Serialize()
{
    VmSerialization out;
    auto size = globalBorrowMap_.size();
    out << size;
    for (auto &borrowIdStatusItem : globalBorrowMap_) {
        out << borrowIdStatusItem.first;
        BorrowIdStatus &borrowIdStatus = borrowIdStatusItem.second;
        out << borrowIdStatus.borrowId;
        out << borrowIdStatus.presentNumaId;
        out << enum_v(borrowIdStatus.memMigrateStatus);
        VMNodeLocInfo &vmNodeLocInfo = borrowIdStatus.nodeLocInfo;
        out << vmNodeLocInfo.hostName;
        out << vmNodeLocInfo.hostId;
        out << vmNodeLocInfo.socketId;
        out << vmNodeLocInfo.numaId;
    }
    out << index_;
    if (!out.Check()) {
        return VM_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = true;
    return VM_OK;
}

VmResult GlobalBorrowMapMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    size_t mapSize;
    in >> mapSize;
    if (!in.Check()) {
        return VM_ERROR;
    }
    if (mapSize > MAX_BORROW_ID_COUNT) {
        return VM_ERROR_DESERIALIZE_ERROR;
    }
    try {
        globalBorrowMap_.reserve(mapSize);
    } catch (const std::exception &e) {
        return VM_ERROR_NOMEM;
    }

    for (size_t i = 0; i < mapSize; ++i) {
        std::string first;
        BorrowIdStatus second;
        VMNodeLocInfo &vmNodeLocInfo = second.nodeLocInfo;
        in >> first;
        in >> second.borrowId;
        in >> second.presentNumaId;
        in >> enum_v(second.memMigrateStatus);
        in >> vmNodeLocInfo.hostName;
        in >> vmNodeLocInfo.hostId;
        in >> vmNodeLocInfo.socketId;
        in >> vmNodeLocInfo.numaId;
        globalBorrowMap_.insert(std::make_pair(first, second));
    }
    in >> index_;
    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}
} // namespace vm