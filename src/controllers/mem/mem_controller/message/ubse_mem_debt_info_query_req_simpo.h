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

#ifndef UBSE_MEM_DEBT_INFO_QUERY_REQ_SIMPO_H
#define UBSE_MEM_DEBT_INFO_QUERY_REQ_SIMPO_H
#include "ubse_base_message.h"
namespace ubse::mem::controller::message {
using namespace ubse::message;
class NodeMemDebtInfoQueryReqSimpo : public UbseBaseMessage {
public:
    NodeMemDebtInfoQueryReqSimpo() = default;
    explicit NodeMemDebtInfoQueryReqSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetNodeId(std::string sourceData)
    {
        this->nodeId_ = std::move(sourceData);
    }
    inline std::string GetNodeId()
    {
        return nodeId_;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    std::string nodeId_;
};
using NodeMemDebtInfoQueryReqSimpoPtr = Ref<NodeMemDebtInfoQueryReqSimpo>;
} // namespace ubse::mem::controller::message
#endif // UBSE_MEM_DEBT_INFO_QUERY_REQ_SIMPO_H
