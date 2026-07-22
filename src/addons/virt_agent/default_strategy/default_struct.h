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
#ifndef DEFAULT_STRUCT_H
#define DEFAULT_STRUCT_H
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace default_strategy {
using uint8_t = __uint8_t;
using uint16_t = __uint16_t;
using uint32_t = __uint32_t;
using uint64_t = __uint64_t;
using time_t = __time_t;
using DsResult = uint32_t;
const uint32_t DS_OK = 0;                         // success
const uint32_t DS_ERROR = 1;                      // error
const uint32_t DS_ERROR_NULLPTR = 2;              // null pointer
const uint32_t DS_ERROR_NULLPTR_EMPTY_VECTOR = 3; // empty vector
const uint32_t DS_ERROR_INVAL = 4;                // invalid argument

const uint32_t DS_ERR_LOG_INIT = 5;        // strategy log initialization error
const uint32_t DS_ERR_RET_MEM_LINE = 6;    // memory waterline exceeds the return threshold
const uint32_t DS_ERR_BOR_MEM_RETMOTE = 7; // remote node of borrowed memory not found in GlobalNumaInfoMap
const uint32_t DS_ERR_SET_INVAL = 8;       // invalid strategy parameter setting

enum class StrategyTip : int
{
    NOPE = 0,                  // no status code
    RET_BORROW_LEGER_EMPTY,    // memory return hint: borrow ledger is empty
    RET_BAN_RET_TOO_LARGE_MEM, // memory return hint: return is forbidden as it would cause high memory waterline
                               // (>(return waterline + alarm waterline) / 2)
    BOR_BIG_FREE_REMOTE_MEM,   // memory borrow hint: free remote memory is larger than the expected borrow amount
    BOR_NOT_ENOUGH_CREDIT,     // memory borrow hint: borrowed memory reaches the node's borrowing quota
};

// log level of default strategy
enum class StrategyLogLevel : uint32_t
{
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    CRIT = 4
};

enum class NumaMigrateStatus : int
{
    NORMAL = 0,
    MIGRATEIN,
    MIGRATEOUT,
    MIGRATINGIN,
    MIGRATINGOUT
};

enum class ConnectionState : int
{
    UP = 0,
    DOWN,
    NONE
};

enum class VmMigrateStatus : int
{
    MIGRATEABLE = 0,
    MIGRATING,
    MIGRATEUNABLE
};

enum class EscapeActionType : int
{
    BORROW = 0,
    RETURN,
    NOPE
};

enum class NumaStatus : int
{
    NORMAL = 0,
    FAULT,
    UNKNOWN,
};

struct VMNodeLocInfo {
    std::string hostName{};
    std::string hostId{};
    int16_t socketId{};
    int16_t numaId{};

    bool operator<(const VMNodeLocInfo& a) const
    {
        if (hostId != a.hostId) {
            return hostId < a.hostId;
        } else if (socketId != a.socketId) {
            return socketId < a.socketId;
        } else if (numaId != a.numaId) {
            return numaId < a.numaId;
        } else {
            return false;
        }
    }

    bool operator==(const VMNodeLocInfo& a) const
    {
        return (hostId == a.hostId && socketId == a.socketId && numaId == a.numaId);
    }

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "[\"" << hostId << "\"," << socketId << "," << numaId << "]";
        return oss.str();
    }
};

struct NumaInfoCollected {
    uint16_t numaId{};                  // real numaId
    uint16_t numaCpuCounts{};           // from Collected
    std::vector<uint16_t> cpuIds{};     // sample
    uint64_t numaMemTotal{};            // from Collected
    uint64_t numaMemUsed{};             // from Collected
    uint16_t numaCpuUsedCounts{};       // sample
    uint64_t numaVMMemAllocated{};      // sample
    std::vector<uint16_t> idleCpuIds{}; // sample
};

struct NumaInfoToKeep {
    NumaMigrateStatus numaMigrateStatus = NumaMigrateStatus::NORMAL; // Maintained and updated by Mgr: NORMAL MIGRATEIN,
                                                                     // MIGRATEOUT, MIGRATINGIN, MIGRATINGOUT
    time_t numaMigrateLastTime = 0;                                  // Maintained and updated by Mgr, after migrating
};

struct GlobalNumaInfo
    : NumaInfoCollected
    , NumaInfoToKeep {
    uint64_t numaMemBorrow{}; // from mem
    uint64_t numaMemLend{};   // from mem
    VMNodeLocInfo numaLoc{};
    NumaStatus numaStatus = NumaStatus::NORMAL; // numa status
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << R"("numaLoc": )" << numaLoc.ToString() << ",";
        oss << R"("numaMemTotal": )" << numaMemTotal << ",";
        oss << R"("numaMemUsed": )" << numaMemUsed << ",";
        oss << R"("numaVMMemAllocated": )" << numaVMMemAllocated << ",";
        oss << R"("numaMemBorrow": )" << numaMemBorrow << ",";
        oss << R"("numaMigrateLastTime": )" << numaMigrateLastTime << ",";
        if (numaMigrateStatus == NumaMigrateStatus::NORMAL) {
            oss << R"("numaMigrateStatus": "NORMAL",)";
        } else if (numaMigrateStatus == NumaMigrateStatus::MIGRATEIN) {
            oss << R"("numaMigrateStatus": "MIGRATEIN",)";
        } else if (numaMigrateStatus == NumaMigrateStatus::MIGRATEOUT) {
            oss << R"("numaMigrateStatus": "MIGRATEOUT",)";
        } else if (numaMigrateStatus == NumaMigrateStatus::MIGRATINGIN) {
            oss << R"("numaMigrateStatus": "MIGRATINGIN",)";
        } else if (numaMigrateStatus == NumaMigrateStatus::MIGRATINGOUT) {
            oss << R"("numaMigrateStatus": "MIGRATINGOUT",)";
        }
        if (numaStatus == NumaStatus::NORMAL) {
            oss << R"("numaStatus": "NORMAL")";
        } else if (numaStatus == NumaStatus::FAULT) {
            oss << R"("numaStatus": "FAULT")";
        } else if (numaStatus == NumaStatus::UNKNOWN) {
            oss << R"("numaStatus": "UNKNOWN")";
        }
        oss << "}";
        return oss.str();
    }
};

struct VmDomainNumaMemInfo {
    int16_t numaId{};
    uint64_t pageSize{};
    int64_t usedMem{};
    int16_t socketId{};
    bool isLocal{};

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "numaId:" << numaId << ",";
        oss << "pageSize:" << pageSize << ",";
        oss << "socketId:" << socketId << ",";
        oss << "usedMem:" << usedMem << ",";
        oss << "isLocal:" << isLocal;
        oss << "}";
        return oss.str();
    }
};

// vm basic info collected
struct VMBasicInfoCollected {
    std::string uuid{};
    uint64_t pid{};
    std::string name{};
    std::string nodeId{};
    std::string hostName{};
    uint64_t maxMem{};
    time_t vmCreateTime = 0;
    std::unordered_map<int16_t, VmDomainNumaMemInfo> numaMemInfo{}; // key is numaId
};

// VM information maintained by ubsemanager
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

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"({"uuid":")" << uuid << R"(",)";
        oss << R"("maxMem":)" << maxMem << R"(,)";
        oss << R"("remoteUsedMem":)" << remoteUsedMem << R"(,)";
        oss << R"("vmCreateTime":)" << vmCreateTime << R"(,)";
        oss << R"("numaMemInfo":[)";
        for (const auto& item : numaMemInfo) {
            oss << item.second.ToString() << ",";
        }
        oss << R"(],)";
        oss << R"("vmMigrateInTime":)" << vmMigrateInTime << R"(,)";
        oss << R"("vmMigrateCount":)" << vmMigrateCount << R"(,)";
        oss << R"("vmMigrateStatus":")" << GetVmMigrateStatus() << R"("})";
        return oss.str();
    }

    std::string GetVmMigrateStatus() const
    {
        switch (vmMigrateStatus) {
            case VmMigrateStatus::MIGRATEABLE:
                return R"(MIGRATEABLE)";
            case VmMigrateStatus::MIGRATEUNABLE:
                return R"(MIGRATEUNABLE)";
            case VmMigrateStatus::MIGRATING:
                return R"(MIGRATING)";
            default:
                return R"(UNKNOWN)";
        }
    }
};

using NodeLocInfo = struct TagUbseNodeLocInfo {
    std::string hostId{};
    int16_t socketId{};
    int16_t numaId{};
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "\"" << hostId << "\"," << socketId << "," << numaId;
        return oss.str();
    }
};

using BorrowItem = struct TagUbseBorrowItem {
    uint16_t exportLocNum{};                  // Number of NUMA nodes providing memory
    std::vector<NodeLocInfo> exportLocInfo{}; // Locations of NUMA nodes providing memory, For a single borrow
                                              // operation, at most two NUMA nodes from one physical node P can be used
    std::vector<uint64_t> requestSize{};      // Amount of memory provided by each NUMA node
    uint64_t exportMemId{};                   // Memory ID generated by OBMM on the exporting node
    uint64_t importMemId{};                   // Memory ID generated by OBMM on the importing (local) node
    std::string name{};                       // Borrow ledger entry name

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"({"exportLocNum":)" << exportLocNum << R"(,)";
        oss << R"("exportLocInfo":[)";
        for (size_t i = 0; i < exportLocNum; ++i) {
            if (i >= exportLocInfo.size()) {
                break;
            }
            if (i != 0) {
                oss << R"(,)";
            }
            oss << exportLocInfo[i].ToString();
        }
        oss << R"(],)";
        oss << R"("requestSize":[)";
        for (size_t i = 0; i < exportLocNum; ++i) {
            if (i >= requestSize.size()) {
                break;
            }
            if (i != 0) {
                oss << R"(,)";
            }
            oss << requestSize[i];
        }
        oss << R"(],)";
        oss << R"("exportMemId":)" << exportMemId << R"(,)";
        oss << R"("importMemId":)" << importMemId << R"(,)";
        oss << R"("name":")" << name << R"("})";
        return oss.str();
    }
};

using BorrowItemInfo = struct TagUbseBorrowItemInfo {
    std::vector<BorrowItem> borrowItem{};

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"("borrowItem":[)";
        for (size_t i = 0; i < borrowItem.size(); ++i) {
            if (i != 0) {
                oss << R"(,)";
            }
            oss << borrowItem[i].ToString();
        }
        oss << R"(]})";
        return oss.str();
    };
};

using LendItem = struct TagUbseLendItem {
    NodeLocInfo importNumaLoc{};              // Location of the borrowing NUMA node
    uint16_t exportLocNum{};                  // Number of NUMA nodes actually lending memory, For a single borrow
                                              // operation, at most two NUMA nodes can jointly provide memory
    std::vector<NodeLocInfo> exportLocInfo{}; // Locations of NUMA nodes providing memory
    uint64_t exportMemId{};                   // Export result generated by OBMM on the local node
    uint64_t importMemId{};                   // Import result generated by OBMM on the borrowing node
    std::vector<uint64_t> requestSize{};      // Amount of memory provided by each NUMA node during lending
};

using LendItemInfo = struct TagUbseLendItemInfo {
    int lendItemNum{}; // Actual number of lending ledger entries for this NUMA node
    std::vector<LendItem> lendItem{};
};

struct AlarmNumaInfo {
    int oomEventFlag{};
    bool isMemReturn{};                                          // Whether memory is returned
    VMNodeLocInfo numaLoc{};                                     // Location of the alarm NUMA node
    std::unordered_map<std::string, VMBasicInfo> vmBasicInfos{}; // VM information on the alarm node
    BorrowItemInfo borrowItemInfo{};                             // Borrow table
    LendItemInfo lendItemInfo{};                                 // Lend table, not used in the current strategy

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"("AlarmNumaInfo":)";
        oss << R"({"isMemReturn":)" << std::boolalpha << isMemReturn << R"(,)";
        oss << R"("oomEventFlag":)" << oomEventFlag << R"(,)";
        oss << R"("numaLoc":)" << numaLoc.ToString() << R"(,)";
        oss << R"("vmBasicInfos":{)";
        size_t count = 0;
        for (auto& vmBasicInfo : vmBasicInfos) {
            if (count != 0) {
                oss << R"(,)";
            }
            oss << R"(")" << vmBasicInfo.first << R"(":)" << vmBasicInfo.second.ToString();
            count++;
        }
        oss << R"(},)";
        oss << R"("borrowItemInfo":)" << borrowItemInfo.ToString();
        oss << R"(})";
        return oss.str();
    }
};

using GlobalNumaInfoMap = std::map<VMNodeLocInfo, GlobalNumaInfo>;

struct StrategyTipInfo {
    static std::string StrategyTipToString(const StrategyTip& strategyTip)
    {
        std::ostringstream oss;
        if (strategyTip == StrategyTip::NOPE) {
            oss << R"("NOPE")";
        } else if (strategyTip == StrategyTip::RET_BORROW_LEGER_EMPTY) {
            oss << R"("RET_BORROW_LEGER_EMPTY")";
        } else if (strategyTip == StrategyTip::RET_BAN_RET_TOO_LARGE_MEM) {
            oss << R"("RET_BAN_RET_TOO_LARGE_MEM")";
        } else if (strategyTip == StrategyTip::BOR_BIG_FREE_REMOTE_MEM) {
            oss << R"("BOR_BIG_FREE_REMOTE_MEM")";
        } else if (strategyTip == StrategyTip::BOR_NOT_ENOUGH_CREDIT) {
            oss << R"("BOR_NOT_ENOUGH_CREDIT")";
        }
        return oss.str();
    }

    static std::string StrategyTips(const std::vector<StrategyTip>& strategyTips)
    {
        std::ostringstream oss;
        oss << R"("strategyTip": [)";
        if (strategyTips.empty()) {
            oss << "],";
        } else {
            for (auto it = strategyTips.begin(); it != strategyTips.end(); it++) {
                if (it == strategyTips.end() - 1) {
                    oss << StrategyTipToString(*it);
                } else {
                    oss << StrategyTipToString(*it) << ",";
                }
            }
            oss << "],";
        }
        return oss.str();
    }
};

struct EscapeAction {
    EscapeActionType actionType = EscapeActionType::NOPE;
    std::vector<StrategyTip> strategyTips{};
    VMNodeLocInfo curNodeLoc{};
    std::vector<std::string> returnMemNames{};
    std::vector<size_t> borrowSizes{};
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        GetEscapeActionType(oss);
        oss << ", " << StrategyTipInfo::StrategyTips(strategyTips);
        oss << R"( "returnMemNames": [)";
        for (auto it = returnMemNames.begin(); it != returnMemNames.end(); it++) {
            if (it == returnMemNames.end() - 1) {
                oss << R"(")" << *it << R"(")";
            } else {
                oss << R"(")" << *it << R"(",)";
            }
        }
        oss << R"(], "borrowSizes": [)";
        for (auto it = borrowSizes.begin(); it != borrowSizes.end(); it++) {
            if (it == borrowSizes.end() - 1) {
                oss << *it;
            } else {
                oss << *it << ",";
            }
        }
        oss << "]}";
        return oss.str();
    }
    void GetEscapeActionType(std::ostringstream& oss) const
    {
        oss << R"("escapeActionType": )";
        if (actionType == EscapeActionType::BORROW) {
            oss << R"("BORROW")";
        } else if (actionType == EscapeActionType::RETURN) {
            oss << R"("RETURN")";
        } else if (actionType == EscapeActionType::NOPE) {
            oss << R"("NOPE")";
        }
    }
};
} // namespace default_strategy
#endif // DEFAULT_STRUCT_H
