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

#ifndef UBS_ENGINE_UBSE_MEM_SCHEDULER_SOCKET_INFO_H
#define UBS_ENGINE_UBSE_MEM_SCHEDULER_SOCKET_INFO_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ubse_logger.h"
#include "ubse_math_util.h"
#include "ubse_mem_scheduler_numa_info.h"
#include "ubse_node_controller.h"
#include "../ubse_mem_types.h"

namespace ubse::mem::scheduler {

struct RawPortInfo {
    PortId portId;
    std::string remoteSlotId;
    ChipId remoteChipId;
};

class SchedulerSocketInfo {
public:
    explicit SchedulerSocketInfo(const SocketId socketId) : socketId_(socketId) {}
    void Init(const UbseNumaInfo& ubseNumaInfo);
    [[nodiscard]] SocketInfo GetSocketInfo() const;
    void UpdateLinkInfo(const nodeController::UbseCpuInfo& cpuInfo);
    void SetChipId(const ChipId chipId)
    {
        chipId_ = chipId;
    }
    [[nodiscard]] const std::vector<RawPortInfo>& GetRawPortInfos() const
    {
        return rawPortInfos_;
    }
    [[nodiscard]] std::set<std::pair<NodeId, SocketId>> ResolveRawPorts(
        const std::map<NodeId, std::map<ChipId, SocketId>>& socketToChip) const;
    [[nodiscard]] std::set<PortId> GetPorts() const
    {
        return ports_;
    }
    [[nodiscard]] SchedulerNumaInfo* GetNumaInfo(NumaId numaId) const;
    [[nodiscard]] const std::map<NumaId, std::unique_ptr<SchedulerNumaInfo>>& GetAllNumaInfos() const
    {
        return numaInfoMap_;
    }
    [[nodiscard]] uint64_t GetLentSize() const
    {
        uint64_t total = 0;
        for (const auto& [_, numaInfo] : numaInfoMap_) {
            uint64_t result = 0;
            if (ubse::utils::SafeAdd(total, numaInfo->GetMemLentSize(), result)) {
                total = result;
            }
            if (ubse::utils::SafeAdd(total, numaInfo->GetMemSharedSize(), result)) {
                total = result;
            }
        }
        return total;
    }
    [[nodiscard]] uint64_t GetFreeSize() const
    {
        uint64_t total = 0;
        for (const auto& [_, numaInfo] : numaInfoMap_) {
            uint64_t result = 0;
            if (ubse::utils::SafeAdd(total, numaInfo->GetMemFreeSize(), result)) {
                total = result;
            }
        }
        return total;
    }
    [[nodiscard]] uint64_t GetMaxLentSize() const
    {
        return maxLentSize_;
    }
    void SetMaxLentSize(uint64_t maxLentSize)
    {
        maxLentSize_ = maxLentSize;
    }
    [[nodiscard]] uint64_t GetLentCount() const
    {
        return lentCount_;
    }
    void AddLentCount(uint64_t count)
    {
        uint64_t result = 0;
        if (ubse::utils::SafeAdd(lentCount_, count, result)) {
            lentCount_ = result;
        }
    }
    void SubLentCount(uint64_t count)
    {
        uint64_t result = 0;
        if (ubse::utils::SafeSub(lentCount_, count, result)) {
            lentCount_ = result;
        }
    }
    [[nodiscard]] uint64_t GetTotalMemoryUsedSize() const
    {
        uint64_t total = 0;
        for (const auto& [_, numaInfo] : numaInfoMap_) {
            uint64_t result = 0;
            if (ubse::utils::SafeAdd(total, numaInfo->GetMemUsedSize(), result)) {
                total = result;
            }
        }
        return total;
    }
    [[nodiscard]] uint64_t GetTotalMemorySize() const
    {
        uint64_t total = 0;
        for (const auto& [_, numaInfo] : numaInfoMap_) {
            uint64_t result = 0;
            if (ubse::utils::SafeAdd(total, numaInfo->GetMemTotalSize(), result)) {
                total = result;
            }
        }
        return total;
    }
    [[nodiscard]] uint64_t GetAvailableLendSize(uint64_t waterLine, uint64_t blockSize) const
    {
        uint64_t total = 0;
        for (const auto& [_, numaInfo] : numaInfoMap_) {
            uint64_t result = 0;
            if (ubse::utils::SafeAdd(total, numaInfo->GetAvailableLendSize(waterLine, blockSize), result)) {
                total = result;
            }
        }
        return total;
    }
    [[nodiscard]] uint32_t GetMaxExportTimes() const
    {
        return maxExportTimes_;
    }
    void SetMaxExportTimes(uint32_t maxExportTimes)
    {
        maxExportTimes_ = maxExportTimes;
    }

private:
    SocketId socketId_{0};
    ChipId chipId_{0};
    std::map<NumaId, std::unique_ptr<SchedulerNumaInfo>> numaInfoMap_;
    uint64_t maxLentSize_{0};
    uint64_t lentCount_{0};
    uint32_t maxExportTimes_{0};
    std::set<PortId> ports_;
    std::vector<RawPortInfo> rawPortInfos_;
};

} // namespace ubse::mem::scheduler

#endif // UBS_ENGINE_UBSE_MEM_SCHEDULER_SOCKET_INFO_H
