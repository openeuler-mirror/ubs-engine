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

#ifndef VM_HAM_MAKE_DECISION_MSG_H
#define VM_HAM_MAKE_DECISION_MSG_H

#include "base_message.h"

namespace vm {
struct InputParams {
    uint32_t vmMemoryMB;
    std::string uuid;
    std::string destHostName;
    uint32_t destNumaId;
};

class HamMakeDecisionMsg : public BaseMessage {
public:
    HamMakeDecisionMsg() = default;

    explicit HamMakeDecisionMsg(InputParams inputParams) : inputParams_(std::move(inputParams)) {}

    explicit HamMakeDecisionMsg(uint8_t* rawData, uint32_t size)
    {
        SetInputRawData(rawData, size);
    }

    InputParams GetInputParams() const
    {
        return inputParams_;
    }

    VmResult Serialize() override;

    VmResult Deserialize() override;

private:
    InputParams inputParams_{};
};
} // namespace vm

#endif // VM_HAM_MAKE_DECISION_MSG_H
