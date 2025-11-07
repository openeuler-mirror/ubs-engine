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
#include "ubse_mem_resource.h"
namespace ubse::mem::controller::message {
using namespace ubse::message;

class NodeMemDebtInfoSimpo : public UbseBaseMessage {
public:
    NodeMemDebtInfoSimpo() = default;
    explicit NodeMemDebtInfoSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetNodeMemDebtInfoSimpo(ubse::resource::mem::NodeMemDebtInfoMap &sourceData)
    {
        data = std::move(sourceData);
    }
    inline ubse::resource::mem::NodeMemDebtInfoMap GetNodeMemDebtInfoMap()
    {
        return data;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    ubse::resource::mem::NodeMemDebtInfoMap data;
};
using NodeMemDebtInfoSimpoPtr = Ref<NodeMemDebtInfoSimpo>;
}


#endif // UBSE_MANAGER_NODE_MEM_DEBT_INFO_SIMPO_H
