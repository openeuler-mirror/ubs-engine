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

#ifndef UBSE_MANAGER_UBSE_VM_STRUCT_H
#define UBSE_MANAGER_UBSE_VM_STRUCT_H

#include <algorithm>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "vm_info.h"
#include "vm_mem.h"
#include "vm_strategy_struct.h"

namespace std {
template <>
struct hash<vm::NodeLocInfo> {
    size_t operator()(const vm::NodeLocInfo& key) const noexcept
    {
        // Use the hash combination method recommended by Huawei BoostKit.
        size_t seed = 0;
        seed ^= std::hash<std::decay_t<decltype(key.hostId)>>{}(key.hostId) + 0x9e3779b9 + (seed << 6) +     // left 6
                (seed >> 2);                                                                                 // right 2
        seed ^= std::hash<std::decay_t<decltype(key.socketId)>>{}(key.socketId) + 0x9e3779b9 + (seed << 6) + // left 6
                (seed >> 2);                                                                                 // right 2
        seed ^= std::hash<std::decay_t<decltype(key.numaId)>>{}(key.numaId) + 0x9e3779b9 + (seed << 6) +     // left 6
                (seed >> 2);                                                                                 // right 2
        return seed;
    }
};
} // namespace std

namespace vm {

enum NumaMigrateStatus
{
    NORMAL = 0,
    MIGRATEIN,
    MIGRATEOUT,
    MIGRATINGIN,
    MIGRATINGOUT
};
enum EscapeActionType
{
    BORROW = 0,
    RETURN,
    NOPE
};

enum ConnectionState
{
    UP,
    DOWN,
    NONE
};

enum class NumaStatus : int
{
    NORMAL = 0,
    FAULT,
    UNKNOWN,
};

struct NumaInfoToKeep {
    NumaMigrateStatus numaMigrateStatus = NORMAL;
    time_t numaMigrateLastTime = 0;
};

inline NumaStatus MapStringToNumaStatus(const std::string& status)
{
    if (status == "normal") {
        return NumaStatus::NORMAL;
    }
    if (status == "fault") {
        return NumaStatus::FAULT;
    }
    return NumaStatus::UNKNOWN;
}

struct GlobalNumaInfo
    : NumaInfoCollected
    , NumaInfoToKeep {
    uint64_t numaMemBorrow = 0; // from mem
    uint64_t numaMemLend = 0;   // from mem
    VMNodeLocInfo numaLoc{};
    NumaStatus numaStatus = NumaStatus::NORMAL; // numa status
    std::string toString() const
    {
        std::ostringstream oss;
        oss << "{" << std::endl;
        oss << "  numaLoc:" << numaLoc.toString() << std::endl;
        oss << "  numaMemTotal:" << numaMemTotal << std::endl;
        oss << "  numaMemUsed:" << numaMemUsed << std::endl;
        oss << "  numaVMMemAllocated:" << numaVMMemAllocated << std::endl;
        oss << "  numaMemBorrow:" << numaMemBorrow << std::endl;
        oss << "  numaMigrateLastTime:" << numaMigrateLastTime << std::endl;
        if (numaMigrateStatus == NumaMigrateStatus::NORMAL) {
            oss << "  numaMigrateStatus:NORMAL" << std::endl;
        } else if (numaMigrateStatus == NumaMigrateStatus::MIGRATEIN) {
            oss << "  numaMigrateStatus:MIGRATEIN" << std::endl;
        } else if (numaMigrateStatus == NumaMigrateStatus::MIGRATEOUT) {
            oss << "  numaMigrateStatus:MIGRATEOUT" << std::endl;
        } else if (numaMigrateStatus == NumaMigrateStatus::MIGRATINGIN) {
            oss << "  numaMigrateStatus:MIGRATINGIN" << std::endl;
        } else if (numaMigrateStatus == NumaMigrateStatus::MIGRATINGOUT) {
            oss << "  numaMigrateStatus:MIGRATINGOUT" << std::endl;
        }
        if (numaStatus == NumaStatus::NORMAL) {
            oss << "  numaStatus:NORMAL}" << std::endl;
        } else if (numaStatus == NumaStatus::FAULT) {
            oss << "  numaStatus:FAULT}" << std::endl;
        } else if (numaStatus == NumaStatus::UNKNOWN) {
            oss << "  numaStatus:UNKNOWN}" << std::endl;
        }
        return oss.str();
    }
};
// VM information collect by the collection module
struct VMBasicInfoCollected {
    std::string uuid{};
    uint64_t pid{};
    std::string name{};
    std::string nodeId{};
    std::string hostName{};
    uint64_t maxMem{};
    time_t vmCreateTime = 0;
    std::unordered_map<int16_t, mempooling::VmDomainNumaInfo> numaMemInfo{}; // key is numaId
};

// VM information maintained by VirtAgent
struct VMBasicInfoToKeep {
    time_t vmMigrateInTime = 0;  // update after migration
    uint16_t vmMigrateCount = 0; // update after migration
    VmMigrateStatus vmMigrateStatus{};
};

struct VMBasicInfo
    : VMBasicInfoCollected
    , VMBasicInfoToKeep {
    uint64_t remoteUsedMem{}; // Total number of used remote memory
    time_t vmSampleTime = 0;  // vm collection time
    std::string toString() const
    {
        std::ostringstream oss;
        oss << "{" << std::endl;
        oss << "  uuid:" << uuid << std::endl;
        oss << "  maxMem:" << maxMem << std::endl;
        oss << "  remoteUsedMem:" << remoteUsedMem << std::endl;
        oss << "  vmCreateTime:" << vmCreateTime << std::endl;
        oss << "  numaMemInfo:[";
        for (const auto& item : numaMemInfo) {
            oss << item.second.ToString() << ",";
        }
        oss << "]";
        oss << "  vmMigrateInTime:" << vmMigrateInTime << std::endl;
        oss << "  vmMigrateCount:" << vmMigrateCount << std::endl;
        if (vmMigrateStatus == VmMigrateStatus::MIGRATEABLE) {
            oss << "  vmMigrateStatus: MIGRATEABLE" << std::endl;
        } else if (vmMigrateStatus == VmMigrateStatus::MIGRATEUNABLE) {
            oss << "  vmMigrateStatus: MIGRATEUNABLE" << std::endl;
        } else if (vmMigrateStatus == VmMigrateStatus::MIGRATING) {
            oss << "  vmMigrateStatus: MIGRATING" << std::endl;
        }
        oss << "}";
        return oss.str();
    }
};

using NumaMemInfoMap = std::unordered_map<int16_t, mempooling::VmDomainNumaInfo>;

struct EscapeAction {
    EscapeActionType actionType = EscapeActionType::NOPE;
    std::vector<StrategyTip> strategyTips{};
    VMNodeLocInfo curNodeLoc{};
    std::vector<std::string> returnMemNames{};
    std::vector<size_t> borrowSizes{};
    std::string ToString() const
    {
        std::ostringstream oss;
        GetEscapeActionType(oss);
        oss << StrategyTipInfo::StrategyTips(strategyTips);
        oss << "\t\"returnMemNames\":[";
        for (auto it = returnMemNames.begin(); it != returnMemNames.end(); it++) {
            if (it == returnMemNames.end() - 1) {
                oss << *it;
            } else {
                oss << *it << ",";
            }
        }
        oss << "],\t\"borrowSizes\":[";
        for (auto it = borrowSizes.begin(); it != borrowSizes.end(); it++) {
            if (it == borrowSizes.end() - 1) {
                oss << *it;
            } else {
                oss << *it << ",";
            }
        }
        oss << "]"
            << "    },";
        return oss.str();
    }
    void GetEscapeActionType(std::ostringstream& oss) const
    {
        oss << "\"escapeActionType\": ";
        if (actionType == EscapeActionType::BORROW) {
            oss << "\"BORROW\",";
        } else if (actionType == EscapeActionType::RETURN) {
            oss << "\"RETURN\",";
        } else if (actionType == EscapeActionType::NOPE) {
            oss << "\"NOPE\",";
        }
    }
};

enum class MemMigrateStatus : uint8_t
{
    READY_TO_MIGRATE = 0,
    MIGRATE_SUCCESS = 1
};

struct BorrowIdStatus {
    BorrowIdStatus() = default;

    std::string borrowId{};
    uint16_t presentNumaId{};
    MemMigrateStatus memMigrateStatus = MemMigrateStatus::READY_TO_MIGRATE;
    VMNodeLocInfo nodeLocInfo{};

    bool operator==(const BorrowIdStatus& borrowIdStatus) const
    {
        if (this->borrowId == borrowIdStatus.borrowId) {
            return true;
        }
        return false;
    }

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"({"borrowId":")" << borrowId << R"(",)";
        oss << R"("presentNumaId":)" << presentNumaId << R"(,)";
        oss << R"("memMigrateStatus":)" << static_cast<uint8_t>(memMigrateStatus) << R"(,)";
        oss << R"("nodeLocInfo":)" << nodeLocInfo.toString() << R"(})";
        return oss.str();
    }
};

using GlobalNumaInfoMap = std::map<VMNodeLocInfo, GlobalNumaInfo>;
using NumaVMInfoMap = std::map<VMNodeLocInfo, std::unordered_map<std::string, VMBasicInfo>>;
using VMInfoToKeepMap = std::unordered_map<std::string, VMBasicInfoToKeep>;
using NumaInfoToKeepMap = std::map<VMNodeLocInfo, NumaInfoToKeep>;
using GlobalBorrowMap = std::unordered_map<std::string, BorrowIdStatus>;

template <typename T>
void SortByNodeId(std::vector<T>& items)
{
    // Define a lambda expression as a comparator
    auto comparator = [](const T& pre, const T& curr) {
        return pre.nodeId < curr.nodeId;
    };
    // Using std::sort for sorting, passing a custom comparator.
    std::sort(items.begin(), items.end(), comparator);
}

struct AlarmNumaInfo {
    int oomEventFlag{};                                          // Is the Out Of Memory alarm
    bool isMemReturn{};                                          // Is the memory return alarm
    VMNodeLocInfo numaLoc{};                                     // Location of the alarm node
    std::unordered_map<std::string, VMBasicInfo> vmBasicInfos{}; // VM information on alarm node
    BorrowItemInfo borrowItemInfo{};                             // Memory borrow information
    LendItemInfo lendItemInfo{};                                 // Memory lend information

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "AlarmNumaInfo: {" << std::endl;
        oss << "  numaLoc: " << numaLoc.toString() << std::endl;
        oss << "  isMemReturn: " << isMemReturn << std::endl;
        oss << "  oomEventFlag: " << oomEventFlag << std::endl;
        oss << "  vmBasicInfos:[" << std::endl;
        for (auto& vmBasicInfo : vmBasicInfos) {
            oss << vmBasicInfo.second.toString() << "," << std::endl;
        }
        oss << "  borrowItemInfo:[" << std::endl;
        oss << "    borrowItem.size():" << borrowItemInfo.borrowItem.size() << std::endl;
        oss << "    BorrowItem:[" << std::endl;
        for (auto& borrowItem : borrowItemInfo.borrowItem) {
            oss << "      importMemId:" << borrowItem.importMemId << ";";
            for (uint16_t i = 0; i < borrowItem.exportLocNum && i < borrowItem.exportLocInfo.size() &&
                                 i < borrowItem.requestSize.size();
                 i++) {
                oss << "[" << borrowItem.exportLocInfo[i].toString() << "," << borrowItem.requestSize[i] << "],";
            }
            oss << std::endl;
        }
        oss << "}";
        return oss.str();
    }
};

struct HugePageInfo {
    uint64_t numaId{};
    uint64_t borrowedSize{};
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << R"("numaId": )" << numaId << R"(, )";
        oss << R"("borrowSize": )" << borrowedSize;
        oss << "}";
        return oss.str();
    }
};
} // namespace vm
#endif // UBSE_MANAGER_UBSE_VM_STRUCT_H