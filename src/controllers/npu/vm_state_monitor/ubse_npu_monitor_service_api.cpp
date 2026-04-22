/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * UbseEngine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_npu_monitor_service_api.h"

#include <mutex>
#include <optional>
#include <securec.h>
#include <unordered_set>
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_npu_resource_collection.h"
#include "ubse_os_util.h"
#include "ubse_pointer_process.h"
#include "ubse_xml.h"

namespace ubse::npu::vm_monitor {
using namespace ubse::log;
using namespace ubse::common::def;
using namespace ubse::npu::controller;
using namespace ubse::utils;
UBSE_DEFINE_THIS_MODULE ("ubse");

static LibvirtMonitor g_monitor("qemu:///system");
constexpr char NPU_RESET_CMDS[] =
        "ipmitool raw 0x30 0x93 0xdb 0x07 0x00 0x8f 0x5c 0x00 0x00 0x80 0xff 0x%02x 0x00 0x00 0xc0 "
        "0x00 0x00 0x00 0x01 0xff";

UbseResult ResetNpu(const uint8_t &chipId)
{
    char realCmd[128]; // 128:数组长度
    static_assert(sizeof(NPU_RESET_CMDS) <= sizeof(realCmd) - 1);
    if (sprintf_s(realCmd, sizeof(realCmd), NPU_RESET_CMDS, chipId) == -1) {
        UBSE_LOG_ERROR << "Failed to generate ipmi command.";
        return UBSE_ERROR;
    }
    std::string result;
    UbseResult ret = UbseOsUtil::Exec(realCmd, result);
    UBSE_LOG_INFO << "ipmi reset npu ret: " << ret << " ,output: " << result;
    return ret;
}

bool QueryAndReset(const std::string &busInstance)
{
    auto devPtr = ResourceCollection::GetInstance().GetDeviceByGuid(busInstance);
    if (devPtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to get device resource:" << busInstance;
        return false;
    }
    auto devBusiPtr = CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(devPtr);
    if (devBusiPtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to find bus instance: " << busInstance;
        return false;
    }
    auto subIDevs = devBusiPtr->GetSubDevIdev();
    if (subIDevs.empty()) {
        UBSE_LOG_ERROR << "No sub vfe found for businstance: " << busInstance;
        return false;
    }
    for (auto &idev : subIDevs) {
        if (idev == nullptr) {
            continue;
        }
        auto david = idev->GetBondingDevDavid();
        if (david == nullptr) {
            UBSE_LOG_WARN << "Failed to get david device of vfe:" << idev->GetIdStr() << ", businstance: " <<
                busInstance;
            continue;
        }
        auto loc = david->GetDeviceLoc();
        ResetNpu(loc.chipId);
    }
    return true;
}

std::string GetBusInstance(std::string_view xmlStr)
{
    std::string guid;
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(xmlStr.data());
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "Failed to get ubse xml.";
        return guid;
    }
    const auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK) {
        UBSE_LOG_ERROR << "Failed to parse ubse xml.";
        return guid;
    }
    ubseXml = ubseXml->Child("devices");
    if (!ubseXml) {
        UBSE_LOG_ERROR << "Failed to get devices.";
        return guid;
    }
    int num = 0;
    auto controller = ubseXml->Child("controller", num);
    while (controller != nullptr) {
        std::string type = controller->Attr("type");
        if (type == "ub") {
            ubseXml = ubseXml->Child("devices");
            controller = ubseXml->Child("controller", num);
            auto sourceNode = controller->Child("source");
            if (sourceNode == nullptr) {
                UBSE_LOG_ERROR << "Failed to get source node.";
                return guid;
            }
            auto busInstanceNode = sourceNode->Child("businstance");
            if (busInstanceNode == nullptr) {
                UBSE_LOG_ERROR << "Failed to get businstance node.";
                return guid;
            }
            guid = busInstanceNode->Attr("guid");
            if (guid.empty()) {
                UBSE_LOG_ERROR << "Failed to get guid node.";
                return guid;
            }
            return guid;
        }
        ubseXml = ubseXml->Child("devices");
        num++;
        controller = ubseXml->Child("controller", num);
    }
    return guid;
}

static void VmStateCallback(std::string_view name, VirDomainEventType event, const std::shared_ptr<char> &xmlPtr)
{
    if (xmlPtr == nullptr) {
        UBSE_LOG_ERROR << "xmlPtr is nullptr.";
        return;
    }
    auto guid_str = GetBusInstance(xmlPtr.get());
    if (guid_str.empty()) {
        UBSE_LOG_ERROR << "guid is empty";
        return;
    }
    guid_str.erase(std::remove_if(guid_str.begin(), guid_str.end(), [](char c) { return c == '-'; }), guid_str.end());
    ResetNpuOfBusInstance(guid_str, event);
}

UbseResult StartVMMonitor()
{
    g_monitor.SetCallBack(VmStateCallback);
    if (!g_monitor.Start()) {
        UBSE_LOG_ERROR << "Failed to start VM monitor.";
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "Libvirt monitor running";
    return UBSE_OK;
}


void ResetNpuOfBusInstance(const std::string &busInstance, VirDomainEventType event)
{
    static const std::unordered_set<VirDomainEventType> handleEvents = {
        VirDomainEventType::VIR_DOMAIN_EVENT_STARTED,
        VirDomainEventType::VIR_DOMAIN_EVENT_STOPPED
    };
    if (handleEvents.find(event) == handleEvents.end()) {
        return;
    }
    static std::unordered_map<std::string, VirDomainEventType> record;
    static std::mutex recordMutex;
    std::lock_guard<std::mutex> lock(recordMutex);
    if (const auto it = record.find(busInstance); it != record.end()) {
        if (it->second == event) {
            return;
        }
        it->second = event;
    } else {
        record[busInstance] = event;
    }
    QueryAndReset(busInstance);
}
}