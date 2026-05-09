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

#ifndef MEM_HANDLER_H
#define MEM_HANDLER_H

#include <map>
#include <mutex>
#include <unordered_map>

#include "vm_error.h"
#include "vm_info.h"
#include "vm_numa_info.h"

namespace vm {

const std::string UBSE_MEM_BORROW_EVENT_ID = "RegisterMemEventNotifyFunc_high";
const std::string UBSE_MEM_RETURN_EVENT_ID = "RegisterMemEventNotifyFunc_low";
const std::string UBSE_MEM_CLEAR_EVENT_ID = "RegisterMemEventNotifyFunc_clear";

constexpr uint16_t MOVE20_BIT = 20;
constexpr uint16_t MOVE2_BIT = 2;
constexpr uint16_t MIN_PERCENT = 0;
constexpr uint16_t MAX_PERCENT = 100;

enum class WatermarkWarningType {
    NO_WARN = 0,
    HIGH_WATERMARK,
    LOW_WATERMARK,
    CLEAR_BORROW
};

struct Notify {
    int percent{};
    int highWaterMark{};
    int lowWaterMark{};
    std::string nodeId{};
    unsigned int socketId{};
    unsigned int numaId{};
    uint64_t memTotal{};
    uint64_t memUsed{};
    uint64_t memFree{};
    bool waterNotify = true;
    bool oomEventFlag = false;

    std::string ToJson() const
    {
        std::ostringstream oss;
        oss << "{"
            << "\"percent\":" << percent << ","
            << "\"highWaterMark\":" << highWaterMark << ","
            << "\"lowWaterMark\":" << lowWaterMark << ","
            << "\"nodeId\":\"" << nodeId << "\","
            << "\"socketId\":" << socketId << ","
            << "\"numaId\":" << numaId << ","
            << "\"memTotal\":" << memTotal << ","
            << "\"memUsed\":" << memUsed << ","
            << "\"memFree\":" << memFree << ","
            << "\"waterNotify\":" << (waterNotify ? "true" : "false") << ","
            << "\"oomEventFlag\":" << (oomEventFlag ? "true" : "false") << "}";
        return oss.str();
    }
};

class MemHandler {
public:
    static MemHandler &GetInstance();
    VmResult Init();
    void Terminate();
    static VmResult CheckNumaWaterLine();
    static VmResult TransNotify(const std::string &notifyMessage, Notify &notify);
    static VmResult GetBorrowedSizeMap(const std::vector<uint16_t> &remoteNumaIds,
                                       std::map<uint16_t, uint64_t> &numaBorrowedSizeMap);
    static VmResult GetMemoryBorrowInfo(std::unordered_map<unsigned int, unsigned int> &borrowInfo);

    static uint64_t SizeByte2Mb(uint64_t size)
    {
        return size >> MOVE20_BIT;
    }

    static uint64_t SizeMb2Byte(uint64_t size)
    {
        if (size > (std::numeric_limits<uint64_t>::max() >> MOVE20_BIT) / MOVE2_BIT) {
            return std::numeric_limits<uint64_t>::max() / MOVE2_BIT;
        }
        return size << MOVE20_BIT;
    }

private:
    static std::mutex timerTaskMutex;
    static std::string timerName;
    static std::string ToString(WatermarkWarningType warning);
    static VmResult NotifyVm(WatermarkWarningType warningType, const Notify &notify);

    static void NotifyReturnMem(const NumaCpuInfo &numaCpuInfo);
    static bool IsVmExists(const NumaCpuInfo &numaCpuInfo, const HostVmDomainInfo &hostVmDomainInfo);
    static WatermarkWarningType WaterNotifyEvent(const NumaCpuInfo &numaCpuInfo, const size_t &borrowInfoSize,
                                                 Notify &notify);
};
} // namespace vm
#endif // MEM_HANDLER_H
