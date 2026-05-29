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

#ifndef MEM_MIGRATE_MSG_H
#define MEM_MIGRATE_MSG_H

#include "base_message.h"

namespace vm {
struct MemMigrateInputParams {
    std::string opt;
    std::string uuid;
};

class MemMigrateMsg : public BaseMessage {
public:
    MemMigrateMsg() = default;

    explicit MemMigrateMsg(MemMigrateInputParams& inputParams) : inputParams_(std::move(inputParams)) {}

    explicit MemMigrateMsg(uint8_t* rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    MemMigrateInputParams GetInputParams() const
    {
        return inputParams_;
    }

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    MemMigrateInputParams inputParams_{};
};
} // namespace vm

#endif // MEM_MIGRATE_MSG_H
