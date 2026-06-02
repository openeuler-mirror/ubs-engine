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

#ifndef UBS_ENGINE_UBSE_NODE_DISCOVERY_SERIAL_H
#define UBS_ENGINE_UBSE_NODE_DISCOVERY_SERIAL_H

#include "ubse_base_message.h"
#include "ubse_node_mgr_def.h"
#include "ubse_serial_util.h"

namespace ubse::nodeMgr {
using namespace ubse::message;
using namespace ubse::serial;
class UbseNodeDiscoverySerial : public UbseBaseMessage {
public:
    UbseNodeDiscoverySerial() = default;

    explicit UbseNodeDiscoverySerial(UbseNodeStaticInfo superNode)
    {
        node_ = std::move(superNode);
    }

    explicit UbseNodeDiscoverySerial(uint8_t *data, size_t size)
    {
        SetInputRawData(data, size);
    }

    UbseNodeStaticInfo GetUbseSuperNode()
    {
        return node_;
    }

    static void SerializeUbseNode(UbseSerialization &out, UbseNodeStaticInfo &node);
    static UbseResult DeSerializeUbseNode(UbseDeSerialization &in, UbseNodeStaticInfo &node);

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UbseNodeStaticInfo node_;
};
} // namespace ubse::nodeDiscovery

#endif // UBS_ENGINE_UBSE_NODE_DISCOVERY_SERIAL_H
