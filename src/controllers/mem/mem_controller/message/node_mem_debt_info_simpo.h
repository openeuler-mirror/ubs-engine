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

#ifndef UBSE_MANAGER_NODE_MEM_DEBT_INFO_SIMPO_H
#define UBSE_MANAGER_NODE_MEM_DEBT_INFO_SIMPO_H
#include "ubse_base_message.h"
#include "ubse_mmi_interface.h"
namespace ubse::mem::controller::message {
using namespace ubse::message;
using namespace ubse::adapter_plugins::mmi;

class NodeMemDebtInfoSimpo : public UbseBaseMessage {
public:
    NodeMemDebtInfoSimpo() = default;
    explicit NodeMemDebtInfoSimpo(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetNodeMemDebtInfoSimpo(NodeMemDebtInfoMap& sourceData)
    {
        data_ = std::move(sourceData);
    }
    inline NodeMemDebtInfoMap GetNodeMemDebtInfoMap()
    {
        return data_;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    NodeMemDebtInfoMap data_;
};
using NodeMemDebtInfoSimpoPtr = Ref<NodeMemDebtInfoSimpo>;
} // namespace ubse::mem::controller::message

#endif // UBSE_MANAGER_NODE_MEM_DEBT_INFO_SIMPO_H
