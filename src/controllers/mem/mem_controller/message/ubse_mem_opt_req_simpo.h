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

#ifndef UBSE_MEM_OPT_REQ_SIMPO_H
#define UBSE_MEM_OPT_REQ_SIMPO_H

#include "ubse_base_message.h"
#include "ubse_mem_controller.h"

namespace ubse::mem::controller::message {
using ubse::mem::controller::UbseMemBorrowType;
using ubse::message::UbseBaseMessage;
using ubse::utils::Ref;

class UbseMemOptReqSimpo : public UbseBaseMessage {
public:
    UbseMemOptReqSimpo() = default;
    explicit UbseMemOptReqSimpo(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    void SetOptRequest(const std::string& inputName, const std::string& nodeId,
                       const UbseMemBorrowType& inputBorrowType)
    {
        name_ = inputName;
        importNodeId_ = nodeId;
        borrowType_ = inputBorrowType;
    }

    const std::string& GetName()
    {
        return name_;
    }

    UbseMemBorrowType GetType()
    {
        return borrowType_;
    }

    const std::string& GetImportNodeId()
    {
        return importNodeId_;
    }

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    std::string name_;
    UbseMemBorrowType borrowType_;
    std::string importNodeId_;
};

using UbseMemOptReqSimpoPtr = Ref<UbseMemOptReqSimpo>;
} // namespace ubse::mem::controller::message
#endif