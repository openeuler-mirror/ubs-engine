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

#ifndef VM_INFO_H
#define VM_INFO_H

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "mempooling_def.h"

namespace vm {
enum class VmEventType
{
    UNKNOWN = 1,
    START,
    SHUTDOWN,
    MIGRATION,
    MIGRATION_START,
    MIGRATION_END,
    OOM,
};
using MemNumaInfo = std::unordered_map<int16_t, mempooling::VmDomainNumaInfo>;
struct VmDomainInfo {
    VmDomainInfo() = default;

    std::string uuid{};
    std::string name{};
    std::string state{};
    time_t vmCreateTime{}; // VM creation time
    uint64_t maxMem{};     // Requested memory size of the VM
    uint64_t usedMem{};    // Memory currently used by the VM (proc)
    uint64_t remoteUsedMem{};
    std::string nodeId{};
    std::string hostName{};    // Host ID of the node (from config file)
    int64_t pid{};             // VM process PID
    time_t timestamp{};        // Collection timestamp
    MemNumaInfo numaMemInfo{}; // key is numaId

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "uuid:" << uuid << ",";
        oss << "name:" << name << ",";
        oss << "state:" << state << ",";
        oss << "vmCreateTime:" << vmCreateTime << ",";
        oss << "maxMem:" << maxMem << ",";
        oss << "usedMem:" << usedMem << ",";
        oss << "remoteUsedMem:" << remoteUsedMem << ",";
        oss << "hostName:" << hostName << ",";
        oss << "pid:" << pid << ",";
        oss << "numaMemInfo:[";
        for (const auto& item : numaMemInfo) {
            oss << item.second.ToString() << ",";
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

struct HostVmDomainInfo {
    HostVmDomainInfo() = default;

    std::string nodeId{};
    std::string hostName{};
    std::string type = "metric";
    std::vector<vm::VmDomainInfo> vmDomainInfos{};

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "nodeId:" << nodeId << ",";
        oss << "hostName:" << hostName << ",";
        oss << "vmDomainInfos:[";
        for (const auto& vmDomainInfo : vmDomainInfos) {
            oss << vmDomainInfo.toString() << ",";
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};
} // namespace vm

#endif // VM_INFO_H
