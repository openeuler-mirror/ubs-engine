/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef EXPORT_TYPE_H
#define EXPORT_TYPE_H

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace mempooling::exportV2 {

struct VmMetaData {
    VmMetaData() = default;

    std::string nodeId{};   // 物理节点id-管控面配置文件
    std::string hostName{}; // 物理节点hostName-虚机xml定义文件
    std::string uuid{};     // 虚拟机uuid-虚机xml定义文件
    std::string name{};     // 虚拟机name-虚机xml定义文件
    std::string state{};    // 虚拟机状态-虚机xml定义文件
    time_t vmCreateTime{};  // 虚机创建时间-libvirt采集
    uint64_t maxMem{};      // 虚机申请内存-虚机xml定义文件，单位KBytes
    pid_t pid{};            // 虚机进程PID-操作系统提供

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
        oss << "pid:" << pid << ",";
        oss << "}";
        return oss.str();
    }
};

struct VmDomainNumaInfo {
    int16_t numaId{};    // numaId
    uint64_t pageSize{}; // 虚机在指定numaId上页大小-默认2mBytes大页，即2048KBytes
    int64_t usedMem{};   // 对应numaId使用内存，单位KB
    int16_t socketId{};  // 如果是远端numaId，填充-1，否则为真实值>=0
    bool isLocal{};      // 是否是本地numa  0：非本地  1：本地
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "numaId:" << numaId << ",";
        oss << "pageSize:" << pageSize << ",";
        oss << "usedMem:" << usedMem << ",";
        oss << "socketId:" << socketId << ",";
        oss << "isLocal:" << isLocal;
        oss << "}";
        return oss.str();
    }
};

struct VmDomainInfo {
    VmDomainInfo() = default;

    VmMetaData metaData{};                                    // 元信息
    std::unordered_map<int16_t, VmDomainNumaInfo> numaInfo{}; // 虚机使用numa信息,key为numaId
    time_t timestamp{};                                       // 时间戳

    std::string ToString() const
    {
        size_t count = 0;
        size_t total = 0;
        std::ostringstream oss;
        oss << "{";
        oss << "metaData:[" << metaData.ToString() << "],";
        oss << "numaInfo:[";
        bool first = true;
        for (const auto& it : numaInfo) {
            if (!first) {
                oss << ",";
            }
            first = false;
            oss << "{numaIdKey:" << it.first << ",value:" << it.second.ToString() << "}";
        }
        oss << "],";
        oss << "timestamp:" << timestamp;
        oss << "}";
        return oss.str();
    }
};

struct NumaPageData {
    uint64_t pageSize{};      // 该numaId上页类型
    uint64_t hugePageTotal{}; // 该numa上pageSize类型页数量
    uint64_t hugePageFree{};  // 该numa上pageSize类型空闲的页数量

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{pageSize:" << pageSize << ",hugePageTotal:" << hugePageTotal << ",hugePageFree:" << hugePageFree
            << "}";
        return oss.str();
    }
};

struct NumaMetaData {
    NumaMetaData() = default;

    std::string nodeId{};                                    // 节点Id
    std::string hostName{};                                  // 节点HostName
    int16_t numaId{};                                        // numaId
    int16_t socketId{};                                      // 该numa绑定cpu映射socketId
    bool isLocal;                                            // 是否是本地numa  0：非本地  1：本地
    uint64_t memTotal{};                                     // 该numa节点内存总量(包含)，系统文件采集，kb
    uint64_t memFree{};                                      // 该numa上空闲内存，系统文件采集，kb
    std::unordered_map<uint64_t, NumaPageData> numaPageInfo; // 该numa上页信息，key为该numaId上页类型
    std::string ToString() const
    {
        size_t count = 0;
        size_t total = 0;
        std::ostringstream oss;
        oss << "{nodeId:" << nodeId << ",";
        oss << "hostName:" << hostName << ",";
        oss << "numaId:" << numaId << ",";
        oss << "isLocal:" << isLocal << ",";
        oss << "socketId:" << socketId << ",";
        oss << "memTotal:" << memTotal << ",";
        oss << "memFree:" << memFree << ",";
        oss << "numaPageInfo:[";
        bool first = true;
        for (const auto& it : numaPageInfo) {
            if (!first) {
                oss << ",";
            }
            first = false;
            oss << "{pageSizeKey:" << it.first << ",value:" << it.second.ToString() << "}";
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

struct NumaInfo {
    NumaInfo() = default;
    time_t timestamp{};      // 时间戳
    NumaMetaData metaData{}; // numa元信息

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{metaData:[" << metaData.ToString() << "],";
        oss << "timestamp:" << timestamp << "}";
        return oss.str();
    }
};

struct NumaInfoWithReservedMem : public NumaInfo {
    uint64_t reservedMem{}; // 新增字段
    uint64_t borrowableMem{};
    uint64_t lentMem{};
    uint64_t sharedMem{};

    NumaInfoWithReservedMem() = default;

    NumaInfoWithReservedMem(const NumaInfo& base, uint64_t reserved, uint64_t borrowable, uint64_t lent,
                            uint64_t shared)
        : NumaInfo(base),
          reservedMem(reserved),
          borrowableMem(borrowable),
          lentMem(lent),
          sharedMem(shared)
    {
    }

    // 调试输出
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{metaData:[" << metaData.ToString() << "], "
            << "timestamp:" << timestamp << ", "
            << "reservedMem:" << reservedMem << ", "
            << "borrowableMem:" << borrowableMem << ", "
            << "lentMem:" << lentMem << ", "
            << "sharedMem:" << sharedMem << "}";
        return oss.str();
    }
};

} // namespace mempooling::exportV2
#endif