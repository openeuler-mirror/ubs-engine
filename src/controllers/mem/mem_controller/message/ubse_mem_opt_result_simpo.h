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

#ifndef UBSE_MEM_OPT_RESULT_SIMPO_H
#define UBSE_MEM_OPT_RESULT_SIMPO_H

#include "ubse_base_message.h"
#include "ubse_mem_controller.h"

namespace ubse::mem::controller::message {
using namespace ubse::message;
using namespace ubse::mem::controller;

class UbseMemOptResultSimpo : public UbseBaseMessage {
public:
    UbseMemOptResultSimpo() = default;
    explicit UbseMemOptResultSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    void SetResp(const UbseMemResult &input)
    {
        resp_ = input;
    }

    UbseMemResult GetResp()
    {
        return resp_;
    }

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UbseMemResult resp_;
};

using UbseMemOptResultSimpoPtr = Ref<UbseMemOptResultSimpo>;
} // namespace ubse::mem::controller::message
#endif