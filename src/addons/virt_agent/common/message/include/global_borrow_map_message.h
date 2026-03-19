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

#ifndef GLOBAL_BORROW_MAP_MESSAGE_H
#define GLOBAL_BORROW_MAP_MESSAGE_H

#include "base_message.h"
#include "vm_struct.h"

namespace vm {
class GlobalBorrowMapMessage final : public BaseMessage {
public:
    GlobalBorrowMapMessage() = default;

    explicit GlobalBorrowMapMessage(std::unordered_map<std::string, BorrowIdStatus> globalBorrowMap,
                                    const uint64_t &index)
        : globalBorrowMap_(std::move(globalBorrowMap)),
          index_(index){};

    explicit GlobalBorrowMapMessage(uint8_t *rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    void SetGlobalBorrowMap(const std::unordered_map<std::string, BorrowIdStatus> &globalBorrowMap,
                            const uint64_t &index)
    {
        globalBorrowMap_ = globalBorrowMap;
        index_ = index;
    }

    std::unordered_map<std::string, BorrowIdStatus> GetGlobalBorrowMap() const
    {
        return globalBorrowMap_;
    }

    uint64_t GetIndex() const
    {
        return index_;
    }

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    std::unordered_map<std::string, BorrowIdStatus> globalBorrowMap_{};
    uint64_t index_{};
};
} // namespace vm

#endif // GLOBAL_BORROW_MAP_MESSAGE_H
