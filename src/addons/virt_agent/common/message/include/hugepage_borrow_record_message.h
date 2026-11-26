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

#ifndef HUGEPAGE_BORROW_RECORD_MESSAGE_H
#define HUGEPAGE_BORROW_RECORD_MESSAGE_H

#include <utility>
#include <vector>

#include "base_message.h"
#include "vm_struct.h"

namespace vm {
class HugePageBorrowRecordMessage final : public BaseMessage {
public:
    HugePageBorrowRecordMessage() = default;

    explicit HugePageBorrowRecordMessage(std::vector<HugePageBorrowRecord> records) : records_(std::move(records)) {}

    explicit HugePageBorrowRecordMessage(uint8_t* rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    void SetData(const std::vector<HugePageBorrowRecord>& records)
    {
        records_ = records;
    }

    std::vector<HugePageBorrowRecord> GetData() const
    {
        return records_;
    }

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    std::vector<HugePageBorrowRecord> records_{};
};
} // namespace vm

#endif // HUGEPAGE_BORROW_RECORD_MESSAGE_H
