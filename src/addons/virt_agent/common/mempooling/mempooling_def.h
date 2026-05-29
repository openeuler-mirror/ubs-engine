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

#ifndef MEMPOOLING_DEF_H
#define MEMPOOLING_DEF_H

#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "vm_vector_util.h"

namespace vm::mempooling {
struct SrcMemoryBorrowParam {
    SrcMemoryBorrowParam() = default;

    std::string srcNid{};   // node Id
    int16_t srcSocketId{};  // socket Id
    int16_t srcNumaId{};    // numa Id
    uid_t uid{0};           // UID of the user who initiates the borrowing.
    std::string username{}; // userName of the user who initiates the borrowing.

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "srcNid:" << srcNid << ",";
        oss << "srcSocketId:" << srcSocketId << ",";
        oss << "srcNumaId:" << srcNumaId << ",";
        oss << "uid:" << uid << ",";
        oss << "username:" << username;
        oss << "}";
        return oss.str();
    }
};

struct DestMemoryBorrowParam {
    DestMemoryBorrowParam() = default;

    std::string destNid{};           // Lender node ID
    uint16_t destSocketId{};         // Lender socket ID
    uint16_t destNumaNum{1};         // Currently limited to 1
    std::vector<int> destNumaId{};   // Lender NUMA IDs
    std::vector<uint64_t> memSize{}; // Borrowed memory size, in kB

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "destNid:" << destNid << ",";
        oss << "destSocketId:" << destSocketId << ",";
        oss << "destNumaId:[";
        oss << VectorUtil::VectorToString(destNumaId, ",");
        oss << "],";
        oss << "memSize[";
        oss << VectorUtil::VectorToString(memSize, ",");
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

struct MemBorrowExecuteResult {
    MemBorrowExecuteResult() = default;

    std::vector<std::string> borrowIds{};
    std::vector<uint16_t> presentNumaIds{};

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "borrowIds:[";
        oss << VectorUtil::VectorToString(borrowIds, ",");
        oss << "],";
        oss << "presentNumaIds[";
        oss << VectorUtil::VectorToString(presentNumaIds, ",");
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

struct MemBorrowStrategyResult {
    MemBorrowStrategyResult() = default;

    SrcMemoryBorrowParam srcParam{};
    uint64_t borrowSize{};
    std::vector<DestMemoryBorrowParam> destParam{};

    std::string ToString() const
    {
        size_t count = 0;
        size_t total = 0;
        std::ostringstream oss;
        oss << "{";
        oss << "srcParam:[" << srcParam.ToString() << "],";
        oss << "borrowSize:" << borrowSize << ",";
        oss << "destParam:[";
        total = destParam.size();
        for (const auto& destParamItem : destParam) {
            oss << destParamItem.ToString();
            if (++count < total) {
                oss << ",";
            }
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

struct PidInfo {
    pid_t pid{}; // PID corresponding to the VM
    uint64_t localUsedMem{};
    std::vector<uint16_t> localNumaIds{};
    uint64_t remoteUsedMem{};
    uint16_t remoteNumaId{}; // Remote NUMA ID, valid only when remoteUsedMem > 0

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{"
            << "\"pid\":" << pid << ","
            << "\"localMemSize\":" << localUsedMem << ","
            << "\"remoteMemSize\":" << remoteUsedMem << ",";

        oss << "\"localNumaIds\":[";
        oss << VectorUtil::VectorToString(localNumaIds, ",");
        oss << "]"
            << "}";

        return oss.str();
    }
};

struct VMPresetParam {
    VMPresetParam() = default;

    pid_t pid{};      // PID corresponding to the VM
    uint16_t ratio{}; // Maximum migration-out ratio

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "pid:" << pid << ",";
        oss << "ratio:" << ratio << ",";
        oss << "}";
        return oss.str();
    }
};

struct VMMigrateOutParam {
    VMMigrateOutParam() = default;

    pid_t pid{};
    uint64_t memSize{};   // Decided migration-out memory size
    uint16_t desNumaId{}; // Destination remote NUMA ID

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "pid:" << pid << ",";
        oss << "memSize:" << memSize << ",";
        oss << "desNumaId:" << desNumaId;
        oss << "}";
        return oss.str();
    }
};

struct MigrateStrategyResult {
    MigrateStrategyResult() = default;

    uint32_t result{};
    std::vector<VMMigrateOutParam> vmInfoList{};
    uint64_t waitingTime{}; // unit: ms

    std::string ToString() const
    {
        size_t count = 0;
        size_t total = 0;
        std::ostringstream oss;
        oss << "{";
        oss << "result:" << result << ",";
        oss << "vmInfoList:[";
        total = vmInfoList.size();
        for (const auto& vmInfo : vmInfoList) {
            oss << vmInfo.ToString();
            if (++count < total) {
                oss << ",";
            }
        }
        oss << "],";
        oss << "waitingTime:" << waitingTime;
        oss << "}";
        return oss.str();
    }
};

struct VmMetaData {
    VmMetaData() = default;

    std::string nodeId{};   // Physical node ID (from control-plane configuration file)
    std::string hostName{}; // Physical node host name (from VM XML definition)
    std::string uuid{};     // VM UUID (from VM XML definition)
    std::string name{};     // VM name (from VM XML definition)
    std::string state{};    // VM state
    time_t vmCreateTime{};  // VM creation time (collected from libvirt)
    uint64_t maxMem{};      // Requested VM memory (from VM XML definition), in KBytes
    pid_t pid{};            // VM process PID (provided by the operating system)

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "nodeId:" << nodeId << ",";
        oss << "hostName:" << hostName << ",";
        oss << "uuid:" << uuid << ",";
        oss << "name:" << name << ",";
        oss << "vmCreateTime:" << vmCreateTime << ",";
        oss << "maxMem:" << maxMem << ",";
        oss << "state:" << state << ",";
        oss << "pid:" << pid;
        oss << "}";
        return oss.str();
    }
};

struct VmDomainNumaInfo {
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

struct VmDomainInfo {
    VmDomainInfo() = default;

    VmMetaData metaData{};
    std::unordered_map<int16_t, VmDomainNumaInfo> numaInfo{}; // key is numaId
    time_t timestamp{};

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "metaData:[" << metaData.ToString() << "],";
        oss << "numaInfo:[";
        for (const auto& item : numaInfo) {
            oss << item.second.ToString() << ",";
        }
        oss << "],";
        oss << "timestamp:" << timestamp;
        oss << "}";
        return oss.str();
    }
};

struct NumaPageData {
    uint64_t pageSize{};
    uint64_t hugePageTotal{};
    uint64_t hugePageFree{};
};

struct NumaMetaData {
    NumaMetaData() = default;

    std::string nodeId{};
    std::string hostName{}; // Node host name
    int16_t numaId{};       // numaId
    int16_t socketId{};     // Socket ID mapped to CPUs bound to this NUMA
    bool isLocal{};         // Whether this is a local NUMA (0: non-local, 1: local)
    uint64_t memTotal{};    // Total memory of this NUMA node (inclusive), collected from system files, in kB
    uint64_t memFree{};     // Free memory on this NUMA node, collected from system files, in kB
    std::unordered_map<uint64_t, NumaPageData> numaPageInfo{}; // key is pageType, unit kB

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "nodeId:" << nodeId << ",";
        oss << "hostName:" << hostName << ",";
        oss << "numaId:" << numaId << ",";
        oss << "isLocal:" << isLocal << ",";
        oss << "memTotal:" << memTotal << ",";
        oss << "memFree:" << memFree << ",";
        oss << "socketId:" << socketId;
        oss << "numaPageInfo:";
        oss << "[";
        bool first = true;
        for (const auto& [pageType, pageData] : numaPageInfo) {
            if (!first)
                oss << ",";
            oss << pageType << ":";
            oss << "{pageSize:" << pageData.pageSize << ","
                << "hugePageTotal:" << pageData.hugePageTotal << ","
                << "hugePageFree:" << pageData.hugePageFree << "}";
            first = false;
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

struct NumaInfo {
    NumaInfo() = default;

    time_t timestamp{};
    NumaMetaData metaData{}; // numa meta data

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "numaMetaInfo:[" << metaData.ToString() << "],";
        oss << "timestamp:" << timestamp;
        oss << "}";
        return oss.str();
    }
};

struct WaterMark {
    WaterMark() = default;

    uint16_t highWaterMark = 85;
    uint16_t lowWaterMark = 80;

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"({"highWaterMark":)" << highWaterMark << R"(,)";
        oss << R"("lowWaterMark":)" << lowWaterMark << R"(})";
        return oss.str();
    }
};

struct BatchSrcMemoryBorrowParam {
    BatchSrcMemoryBorrowParam() = default;

    std::string srcNid{};
    uint16_t srcNumaNum{};
    std::vector<int16_t> srcNumaId{};
    uid_t uid{};
    std::string username{};

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"({"srcNid":)" << srcNid << R"(,)";
        oss << R"("srcNumaNum":)" << srcNumaNum << R"(,)";
        oss << R"("srcNumaId":[)";
        for (const auto& srcNuma : srcNumaId) {
            oss << srcNuma << R"(,)";
        }
        oss << R"(],)";
        oss << R"("uid":)" << uid << R"(,)";
        oss << R"("username":)" << username << R"(})";
        return oss.str();
    }
};

enum class BorrowStrategy : uint8_t
{
    AVERAGE
};

struct NumaQuota {
    NumaQuota() = default;

    uint32_t numaId;
    uint32_t quota; // KB

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"({"numaId":)" << numaId << R"(,)";
        oss << R"("quota":)" << quota << R"(})";
        return oss.str();
    }
};

struct PageSwapPair {
    PageSwapPair() = default;

    std::vector<NumaQuota> localNumas{};
    std::vector<NumaQuota> remoteNumas{};

    [[nodiscard]] std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"({"localNumas":[)";
        for (auto localNuma : localNumas) {
            oss << localNuma.ToString() << R"(,)";
        }
        oss << R"(],)"
            << R"("remoteNumas":[)";
        for (auto remoteNuma : remoteNumas) {
            oss << remoteNuma.ToString() << R"(,)";
        }
        oss << R"(]})";
        return oss.str();
    }
};
} // namespace vm::mempooling

#endif // MEMPOOLING_DEF_H
