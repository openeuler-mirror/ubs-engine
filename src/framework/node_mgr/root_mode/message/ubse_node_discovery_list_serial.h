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

#ifndef UBS_ENGINE_UBSE_NODE_DISCOVERY_LIST_SERIAL_H
#define UBS_ENGINE_UBSE_NODE_DISCOVERY_LIST_SERIAL_H

#include "ubse_base_message.h"
#include "ubse_node_discovery_serial.h"
#include "ubse_node_mgr_def.h"
#include "ubse_serial_util.h"

namespace ubse::nodeMgr {
using namespace ubse::message;
using namespace ubse::serial;

class UbseNodeDiscoveryListSerial : public UbseBaseMessage {
public:
    UbseNodeDiscoveryListSerial() = default;

    explicit UbseNodeDiscoveryListSerial(std::vector<UbseNodeStaticInfo> nodes)
    {
        nodeList_ = nodes;
    }

    explicit UbseNodeDiscoveryListSerial(uint8_t *data, size_t size)
    {
        SetInputRawData(data, size);
    }

    std::vector<UbseNodeStaticInfo> GetUbseNodeList()
    {
        return nodeList_;
    }

    void SetUbseNodeList(std::vector<UbseNodeStaticInfo> nodes)
    {
        nodeList_ = nodes;
    }

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

    std::string ToString() const override;

private:
    std::vector<UbseNodeStaticInfo> nodeList_{};
};
} // namespace ubse::nodeMgr

#endif // UBS_ENGINE_UBSE_NODE_DISCOVERY_LIST_SERIAL_H
