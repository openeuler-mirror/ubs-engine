/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

#include "hugepage_borrow_record_message.h"

#include "vm_def.h"
#include "vm_serial_util.h"

namespace vm {
namespace {
// The upper limit of persisted records, used to protect deserialization.
constexpr size_t MAX_HUGEPAGE_BORROW_RECORD_NUM = 4096;
} // namespace

VmResult HugePageBorrowRecordMessage::Serialize()
{
    VmSerialization out;
    size_t recordSize = records_.size();
    out << recordSize;
    for (const auto& record : records_) {
        if (record.borrowId.size() > MAX_BORROW_ID_LENGTH) {
            return VM_ERROR_INVAL;
        }
        out << record.borrowId;
        out << record.pageSize;
    }
    if (!out.Check()) {
        return VM_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = true;
    return VM_OK;
}

VmResult HugePageBorrowRecordMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    size_t recordSize = 0;
    in >> recordSize;
    if (!in.Check()) {
        return VM_ERROR;
    }
    if (recordSize > MAX_HUGEPAGE_BORROW_RECORD_NUM) {
        return VM_ERROR_DESERIALIZE_ERROR;
    }
    records_.clear();
    records_.reserve(recordSize);
    for (size_t i = 0; i < recordSize; ++i) {
        HugePageBorrowRecord record{};
        in >> record.borrowId;
        in >> record.pageSize;
        if (!in.Check()) {
            return VM_ERROR;
        }
        if (record.borrowId.size() > MAX_BORROW_ID_LENGTH) {
            return VM_ERROR_DESERIALIZE_ERROR;
        }
        records_.emplace_back(std::move(record));
    }
    return VM_OK;
}
} // namespace vm
