/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef UBS_ENGINE_UBSENODEINFOSERIALIZE_H
#define UBS_ENGINE_UBSENODEINFOSERIALIZE_H

#include "ubse_base_message.h"
#include "ubse_node_controller.h"

namespace ubse::nodeController {

class UbseNodeInfoSerialize : public message::UbseBaseMessage {
public:
    UbseNodeInfoSerialize() = default;

    explicit UbseNodeInfoSerialize(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    inline void SetUbseNodeInfo(UbseNodeInfo info)
    {
        nodeInfo_ = std::move(info);
    }

    inline UbseNodeInfo GetUbseNodeInfo()
    {
        return nodeInfo_;
    }

    common::def::UbseResult Serialize() override;

    common::def::UbseResult Deserialize() override;

private:
    UbseNodeInfo nodeInfo_;
};
} // namespace ubse::nodeController

#endif //UBS_ENGINE_UBSENODEINFOSERIALIZE_H
