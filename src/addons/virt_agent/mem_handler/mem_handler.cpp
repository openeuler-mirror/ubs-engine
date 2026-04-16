/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "mem_handler.h"

#include <securec.h>
#include <ubse_error.h>
#include <ubse_mem_controller.h>
#include <ubse_timer.h>
#include "vm_configuration.h"
#include "vm_json_util.h"
#include "vm_string_util.h"
#include "alarm_handler.h"
#include "resource_collect.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;
using namespace ubse::timer;
using namespace ubse::mem::controller;
std::mutex MemHandler::timerTaskMutex;
std::string MemHandler::timerName = "waterMarkCollectTimer";
static const uint32_t ONE_SECOND_TO_MILLI_SECONDS = 1000;

MemHandler &MemHandler::GetInstance()
{
    static MemHandler gInstance;
    return gInstance;
}

VmResult MemHandler::Init()
{
    UBSE_LOG_INFO << "MemHandler initialization started.";
    auto collectPeriod = VmConfiguration::GetInstance().GetExportInterval();
    return UbseTimerHandlerRegister(timerName, CheckNumaWaterLine, collectPeriod);
}

void MemHandler::Terminate()
{
    std::unique_lock<std::mutex> lock(timerTaskMutex);
    UbseTimerHandlerUnregister(timerName);
}

bool MemHandler::IsVmExists(const NumaCpuInfo &numaCpuInfo, const HostVmDomainInfo &hostVmDomainInfo)
{
    // Check for the presence of VM
    return std::any_of(hostVmDomainInfo.vmDomainInfos.begin(), hostVmDomainInfo.vmDomainInfos.end(),
        [&numaCpuInfo](const VmDomainInfo &vmDomainInfo) {
            for (const auto& [numaId, vmDomainNumaInfo] : vmDomainInfo.numaMemInfo) {
                if (vmDomainNumaInfo.numaId == numaCpuInfo.numaId &&
                    vmDomainNumaInfo.socketId == numaCpuInfo.socketId) {
                    return true;
                }
            }
            return false;
        });
}

WatermarkWarningType MemHandler::WaterNotifyEvent(const NumaCpuInfo &numaCpuInfo, const size_t &borrowInfoSize,
                                                  Notify &notify)
{
    int16_t highWaterMark = 0;
    int16_t lowWaterMark = 0;
    try {
        highWaterMark = VmStringUtil::SafeStoi16(VmConfiguration::GetInstance().GetBorrowWatermark());
        lowWaterMark = VmStringUtil::SafeStoi16(VmConfiguration::GetInstance().GetLowWatermark());
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Vm parse waterMark failed. error: " << e.what();
        return WatermarkWarningType::NO_WARN;
    }
    const auto totalMem = SizeMb2Byte(numaCpuInfo.nrHugePage) * 2;
    const auto usedMem = SizeMb2Byte(numaCpuInfo.nrHugePage - numaCpuInfo.freeHugePage) * 2;
    const auto percent = static_cast<int>((static_cast<double>(usedMem) / static_cast<double>(totalMem)) * 100);
    if (percent < MIN_PERCENT || percent > MAX_PERCENT) {
        UBSE_LOG_ERROR << "error_percent=" << percent
                       << ", percent should be in range of [0, 100], nodeId=" << numaCpuInfo.nodeId
                       << ", socketId=" << numaCpuInfo.socketId << ", numaId=" << numaCpuInfo.numaId;
        return WatermarkWarningType::NO_WARN;
    }
    UBSE_LOG_INFO << "The percent is " << percent << ", high waterMark is " << highWaterMark << ", low waterMark is "
                  << lowWaterMark << ", nodeId=" << numaCpuInfo.nodeId << ", socketId=" << numaCpuInfo.socketId
                  << ", numaId=" << numaCpuInfo.numaId << ", borrowSize=" << borrowInfoSize;
    notify = Notify{
        .percent = percent,
        .highWaterMark = highWaterMark,
        .lowWaterMark = lowWaterMark,
        .nodeId = numaCpuInfo.nodeId,
        .socketId = static_cast<unsigned int>(numaCpuInfo.socketId),
        .numaId = static_cast<unsigned int>(numaCpuInfo.numaId),
        .memTotal = totalMem,
        .memUsed = usedMem,
        .memFree = totalMem - usedMem,
    };
    if (percent >= highWaterMark) {
        return WatermarkWarningType::HIGH_WATERMARK;
    }
    if (percent < lowWaterMark && borrowInfoSize > 0) {
        return WatermarkWarningType::LOW_WATERMARK;
    }
    return WatermarkWarningType::NO_WARN;
}

VmResult MemHandler::CheckNumaWaterLine()
{
    std::unique_lock<std::mutex> lock(timerTaskMutex, std::defer_lock);
    if (!lock.try_lock()) {
        UBSE_LOG_DEBUG << "Try lock failed. Continue.";
        return VM_OK;
    }
    HostNumaCpuInfo hostNumaCpuInfo{};
    HostVmDomainInfo hostVmDomainInfo{};
    // Get vm and numa info
    auto ret =
        ResourceCollect::GetInstance().UpdateGlobalNumaInfoMapAndGlobalNumaVMInfoMap(hostVmDomainInfo, hostNumaCpuInfo);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to update the vm data," << FormatRetCode(ret);
        return ret;
    }
    // Get the borrow info to determine whether to clear or return the memory.
    std::unordered_map<unsigned int, unsigned int> borrowInfo;
    ret = GetMemoryBorrowInfo(borrowInfo);
    if (ret != VM_OK) {
        return ret;
    }
    for (auto &numaCpuInfo : hostNumaCpuInfo.numaCpuInfos) {
        // Check whether the node is local.
        if (!numaCpuInfo.isLocal) {
            continue;
        }
        // Check whether VMs exist.
        if (!IsVmExists(numaCpuInfo, hostVmDomainInfo)) {
            // If no VM exists, check whether the VM is borrowed. If yes, send a clearance alarm.
            if (borrowInfo.count(numaCpuInfo.numaId) > 0) {
                UBSE_LOG_INFO << "WarningType=" << ToString(WatermarkWarningType::CLEAR_BORROW)
                              << ", nodeId=" << numaCpuInfo.nodeId << ", numaId=" << numaCpuInfo.numaId;
                NotifyReturnMem(numaCpuInfo);
            }
            continue;
        }
        // When the huge page is 0, the waterline is not calculated.
        if (numaCpuInfo.nrHugePage == 0) {
            UBSE_LOG_DEBUG << "HugePages=0, nodeId=" << numaCpuInfo.nodeId << ", socketId=" << numaCpuInfo.socketId
                           << ", numaId=" << numaCpuInfo.numaId;
            continue;
        }
        Notify notify{};
        const auto warningType = WaterNotifyEvent(numaCpuInfo, borrowInfo.count(numaCpuInfo.numaId), notify);
        if (warningType == WatermarkWarningType::NO_WARN) {
            continue;
        }
        UBSE_LOG_INFO << "WarningType=" << ToString(warningType) << ", nodeId=" << notify.nodeId
                      << ", numaId=" << notify.numaId << ", memUsed_in_MB=" << SizeByte2Mb(notify.memUsed)
                      << ", memTotal_in_MB=" << SizeByte2Mb(notify.memTotal);
        NotifyVm(warningType, notify);
    }
    return VM_OK;
}

VmResult MemHandler::NotifyVm(WatermarkWarningType warningType, const Notify &notify)
{
    std::string id;
    if (warningType == WatermarkWarningType::CLEAR_BORROW) {
        id = UBSE_MEM_CLEAR_EVENT_ID;
    } else {
        id = warningType == WatermarkWarningType::HIGH_WATERMARK ? UBSE_MEM_BORROW_EVENT_ID : UBSE_MEM_RETURN_EVENT_ID;
    }
    std::string json = notify.ToJson();

    auto ret = AlarmHandler::MemNotifyEventHandler(id, json);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Alarm handler returned failure, " << FormatRetCode(ret);
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "Alarm handler processed successfully.";
    return VM_OK;
}

VmResult MemHandler::GetBorrowedSizeMap(const std::vector<uint16_t> &remoteNumaIds,
                                        std::map<uint16_t, uint64_t> &numaBorrowedSizeMap)
{
    // Fill the map. The value is initialized to 0.
    for (const auto &id : remoteNumaIds) {
        UBSE_LOG_DEBUG << "Start to get borrowd size for numa " << id;
        numaBorrowedSizeMap[id] = 0;
    }
    std::vector<UbseNumaMemoryImportDebtInfo> debtInfos{};
    int maxRetries = 30;
    UbseResult res;
    while (maxRetries-- > 0) {
        res = UbseGetNumaMemImportDebtInfoWithLocalNode(debtInfos);
        if (res == UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS) {
            debtInfos.clear();
            std::this_thread::sleep_for(std::chrono::milliseconds(ONE_SECOND_TO_MILLI_SECONDS));
            continue;
        }
        break;
    }
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get local numaMemDebtInfo failed, res=" << static_cast<uint32_t>(res);
        return VM_ERROR;
    }
    UBSE_LOG_DEBUG << "debt info num=" << debtInfos.size();
    for (auto &debtInfo : debtInfos) {
        UBSE_LOG_DEBUG << "borrow name=" << debtInfo.name << ", remoteNumaId=" << debtInfo.remoteNumaId
                       << ", size=" << debtInfo.size;
        if (debtInfo.remoteNumaId < 0 || debtInfo.size == 0) {
            continue;
        }
        auto remoteNumaId = static_cast<uint16_t>(debtInfo.remoteNumaId);
        if (std::find(remoteNumaIds.begin(), remoteNumaIds.end(), remoteNumaId) != remoteNumaIds.end()) {
            if (numaBorrowedSizeMap[remoteNumaId] > std::numeric_limits<uint64_t>::max() - debtInfo.size) {
                UBSE_LOG_ERROR << "The remoteNumaId=" << remoteNumaId << " borrowed size exceeds the limit.";
                return VM_ERROR;
            }
            numaBorrowedSizeMap[remoteNumaId] += debtInfo.size;
        }
    }
    UBSE_LOG_INFO << "Got the numaBorrowedSizeMap successfully.";
    return VM_OK;
}

std::string MemHandler::ToString(WatermarkWarningType warning)
{
    switch (warning) {
        case WatermarkWarningType::NO_WARN:
            return "NO_WARN";
        case WatermarkWarningType::LOW_WATERMARK:
            return "LOW_WATERMARK";
        case WatermarkWarningType::HIGH_WATERMARK:
            return "HIGH_WATERMARK";
        case WatermarkWarningType::CLEAR_BORROW:
            return "CLEAR_BORROW";
    }
    return "";
}

VmResult MemHandler::TransNotify(const std::string &notifyMessage, Notify &notify)
{
    Document msgJson;
    msgJson.Parse(notifyMessage.c_str());
    if (msgJson.HasParseError()) {
        UBSE_LOG_ERROR << "Invalid JSON format, notifyMessage=" << notifyMessage;
        return VM_ERROR;
    }
    uint32_t ret = msgJson.HasMember("waterNotify") && msgJson["waterNotify"].IsBool();
    if (!ret) {
        notify.waterNotify = false;
        UBSE_LOG_INFO << "A Notify message may be from memory, message=" << notifyMessage;
        return VM_OK;
    }
    std::string nodeId;
    ret = VMJsonUtil::GetString(msgJson, "nodeId", nodeId);
    notify.nodeId = nodeId;
    double numVal = 0;
    ret |= VMJsonUtil::GetNumber(msgJson, "percent", numVal);
    notify.percent = static_cast<int>(numVal);
    ret |= VMJsonUtil::GetNumber(msgJson, "highWaterMark", numVal);
    notify.highWaterMark = static_cast<int>(numVal);
    ret |= VMJsonUtil::GetNumber(msgJson, "lowWaterMark", numVal);
    notify.lowWaterMark = static_cast<int>(numVal);
    ret |= VMJsonUtil::GetNumber(msgJson, "socketId", numVal);
    notify.socketId = static_cast<unsigned int>(numVal);
    ret |= VMJsonUtil::GetNumber(msgJson, "numaId", numVal);
    notify.numaId = static_cast<unsigned int>(numVal);
    ret |= VMJsonUtil::GetNumber(msgJson, "memTotal", numVal);
    notify.memTotal = static_cast<uint64_t>(numVal);
    ret |= VMJsonUtil::GetNumber(msgJson, "memUsed", numVal);
    notify.memUsed = static_cast<uint64_t>(numVal);
    ret |= VMJsonUtil::GetNumber(msgJson, "memFree", numVal);
    notify.memFree = static_cast<uint64_t>(numVal);
    return ret;
}

// borrowInfo only contains the NUMA IDs on this node that still have active memory borrowings
VmResult MemHandler::GetMemoryBorrowInfo(std::unordered_map<unsigned int, unsigned int> &borrowInfo)
{
    std::vector<UbseNumaMemoryImportDebtInfo> debtInfos{};
    auto res = UbseGetNumaMemImportDebtInfoWithLocalNode(debtInfos);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get local numaMemDebtInfo failed, res=" << static_cast<uint32_t>(res);
        return VM_ERROR;
    }

    UBSE_LOG_DEBUG << "DebtInfo size=" << debtInfos.size();
    auto ret = ResourceCollect::SyncGlobalBorrowMap(debtInfos);
    if (ret != VM_OK) {
        return ret;
    }
    int i = 0;
    for (const auto &debtInfo : debtInfos) {
        int16_t tempNumaId;
        // Copy first 2 bytes from usrInfo to tempNumaId (business logic alignment with rmrs)
        if (memcpy_s(&tempNumaId, sizeof(tempNumaId), debtInfo.usrInfo, sizeof(tempNumaId)) != EOK) {
            UBSE_LOG_ERROR << "memcpy_s failed.";
            return VM_ERROR;
        }
        UBSE_LOG_DEBUG << "num " << i << ", tempNumaId=" << tempNumaId;
        borrowInfo[tempNumaId] += 1;
        i++;
    }
    UBSE_LOG_INFO << "Finished getting memory borrow information.";
    return VM_OK;
}

void MemHandler::NotifyReturnMem(const NumaCpuInfo &nodeNumaInfo)
{
    const Notify notify{
        .percent = 0,
        .highWaterMark = 0,
        .lowWaterMark = 0,
        .nodeId = nodeNumaInfo.nodeId,
        .socketId = static_cast<unsigned int>(nodeNumaInfo.socketId),
        .numaId = static_cast<unsigned int>(nodeNumaInfo.numaId),
        .memTotal = 0,
        .memUsed = 0,
        .memFree = 0,
    };

    NotifyVm(WatermarkWarningType::CLEAR_BORROW, notify);
}
} // namespace vm
