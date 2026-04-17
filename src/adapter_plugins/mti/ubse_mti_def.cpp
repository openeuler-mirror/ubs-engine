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
#include "adapter_plugins/mti/ubse_mti_def.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_smbios.h"
#include "ubse_str_util.h"

namespace ubse::adapter_plugins::mti {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::common::def;

UbseDevName::UbseDevName(const std::string& name) : devName(name) {}
UbseDevName::UbseDevName(std::string&& name) : devName(std::move(name)) {}
UbseDevName::UbseDevName(const std::string& nodeId, const std::string& socketId) : devName(nodeId + "-" + socketId) {}

bool UbseDevName::operator==(const UbseDevName& other) const
{
    return this->devName == other.devName;
}
bool UbseDevName::operator<(const UbseDevName& other) const
{
    return this->devName < other.devName;
}
uint32_t UbseDevName::SplitDevName(std::string& nodeId, std::string& socketId) const
{
    size_t pos = devName.find('-');
    if (pos == std::string::npos) {
        return UBSE_ERROR;
    }
    nodeId = devName.substr(0, pos);
    socketId = devName.substr(pos + 1);
    return UBSE_OK;
}
UbseDevPortName::UbseDevPortName(const std::string& slotId, const std::string& chipId, const std::string& cardId,
                                 const std::string& portId)
    : name(slotId + ":" + chipId + ":" + cardId + ":" + portId)
{
}
UbseDevPortName::UbseDevPortName(const std::string& name) : name(name) {}
UbseDevPortName::UbseDevPortName(std::string&& name) : name(std::move(name)) {}

bool UbseDevPortName::operator==(const UbseDevPortName& other) const
{
    return name == other.name;
}

std::size_t UbseDevPortNameHash::operator()(const UbseDevPortName& obj) const
{
    auto hash = std::hash<std::string>{}(obj.name);
    return hash;
}

std::size_t UbseDevNameHash::operator()(const UbseDevName& obj) const
{
    auto hash = std::hash<std::string>{}(obj.devName);
    return hash;
}

bool ConvertSlotIdToNodeId(const std::string &slotId, std::string &nodeId)
{
    uint32_t slot;
    if (utils::ConvertStrToUint32(slotId, slot) != UBSE_OK) {
        UBSE_LOG_ERROR << "slotId is not a number: " << slotId;
        return false;
    }
    uint16_t podId;
    if (auto ret = adapter_plugins::smbios::UbseSmbios::GetInstance().GetPodId(podId); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get bios data pod_id failed, " << FormatRetCode(ret);
        return false;
    }

    nodeId = std::to_string(slot + (podId * NO_8));
    return true;
}

bool ConvertNodeIdToSlotId(const std::string &nodeId, std::string &slotId)
{
    uint32_t node;
    if (utils::ConvertStrToUint32(nodeId, node) != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId is not a number: " << nodeId;
        return false;
    }
    uint16_t podId = (node - NO_1) / NO_8;
    slotId = std::to_string(node - (podId * NO_8));
    return true;
}
} // namespace ubse::adapter_plugins::mti