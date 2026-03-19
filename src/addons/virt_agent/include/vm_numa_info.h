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

#ifndef VM_NUMA_INFO_H
#define VM_NUMA_INFO_H

#include <vector>
#include <string>
#include <sstream>

namespace vm {
struct NumaCpuInfo {
    NumaCpuInfo() = default;

    uint32_t id{};
    uint32_t logicId{};
    int16_t socketId{}; // key 1
    int16_t logicSocketId{};
    uint16_t numaCpuCounts{};      // Number of CPUs on a single NUMA node
    uint16_t numaVmCpuAllocated{}; // Total allocated VM vCPU capacity on a NUMA node
    uint64_t numaVmMemTotal{};     // Total memory of VMs on a NUMA node,from mem
    uint64_t numaVmMemAllocated{}; // Total allocated VM memory capacity on a NUMA node
    std::string nodeId{};
    std::string hostName{};             // key 2
    int16_t numaId{};                   // key 3
    bool isLocal{};                     // Indicates whether this is a local NUMA node, 0: non-local, 1: local
    std::vector<uint16_t> freeCPUIds{}; // List of CPUs on this NUMA node not bound to any VM
    std::vector<uint16_t> cpuIds{};     // List of CPUs on this NUMA node
    std::string status{};
    uint64_t memTotal{};     // Total memory
    uint64_t memUsed{};      // Used memory size
    uint64_t memFree{};      // Free memory size
    uint64_t nrHugePage{};   // Total number of huge pages
    uint64_t freeHugePage{}; // Number of free huge pages

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "id:" << id << ",";
        oss << "logicId:" << logicId << ",";
        oss << "socketId:" << socketId << ",";
        oss << "logicSocketId:" << logicSocketId << ",";
        oss << "numaCpuCounts:" << numaCpuCounts << ",";
        oss << "numaVmCpuAllocated:" << numaVmCpuAllocated << ",";
        oss << "numaVmMemTotal:" << numaVmMemTotal << ",";
        oss << "numaVmMemAllocated:" << numaVmMemAllocated << ",";
        oss << "memTotal:" << memTotal << ",";
        oss << "memUsed:" << memUsed << ",";
        oss << "memFree:" << memFree << ",";
        oss << "nrHugePage:" << nrHugePage << ",";
        oss << "freeHugePage:" << freeHugePage << ",";
        oss << "nodeId:" << nodeId << ",";
        oss << "numaId:" << numaId << ",";
        oss << "freeCPUIds:[";
        for (const auto &cpuId : freeCPUIds) {
            oss << cpuId << ", ";
        }
        oss << "],";
        oss << "cpuIds:[";
        for (const auto &cpuId : cpuIds) {
            oss << cpuId << ", ";
        }
        oss << "]";
        oss << "status:" << status << ",";
        oss << "}";
        return oss.str();
    }
};

struct HostNumaCpuInfo {
    HostNumaCpuInfo() = default;
    std::string nodeId{};
    std::string hostName{};
    time_t timestamp{};
    std::string type = "metric";
    std::vector<NumaCpuInfo> numaCpuInfos{};

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "nodeId:" << nodeId << ",";
        oss << "hostName:" << hostName << ",";
        oss << "timestamp:" << timestamp << ",";
        oss << "numaCpuInfos:[";
        for (const auto &numaCpuInfo : numaCpuInfos) {
            oss << numaCpuInfo.toString() << ",";
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};
} // vm

#endif // VM_NUMA_INFO_H
