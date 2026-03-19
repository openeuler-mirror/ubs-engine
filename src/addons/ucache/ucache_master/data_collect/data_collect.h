/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UCACHE_DATA_COLLECT_H
#define UCACHE_DATA_COLLECT_H

#include <map>
#include <vector>
#include <string>
#include <queue>
#include <iostream>
#include "deserialize.h"

namespace ucache {

namespace data_collect {

using namespace ucache::deserialize;

enum class PhyNodeStat {
    ACTIVE, // 活跃
    FAULT, // 故障
    RECOVER, // 恢复中
};

struct MemInfo {
    uint64_t total;
    uint64_t available;
    uint64_t used;
    uint64_t pagecache;
};

struct BorrowStrategyRawData {
    std::string nodeId;
    int pagecacheAppNums;                      // pagecache紧缺型app数量
    uint64_t freeMemMin;                         // 节点空闲内存最小值
    MemInfo localMemInfo;                      // 节点本地内存信息
    std::map<int, MemInfo> remoteNumaMemInfo;  // 节点远端numa内存信息
};

struct NodeMemBorrowInfo {
    uint64_t totalSize;               // in bytes
    std::string srcNodeId;
    std::string destNodeId;
    std::map<int, std::map<std::string, uint64_t>> numaNodeBorrowSize;   // 每个借出方numa node借出的内存，由策略执行模块维护
    int dstNumaId;                              // 借入方生成的远端numa node，由策略执行模块维护
};

struct NodeMemoryInfo {
    uint64_t totalMemory;
    uint64_t usedMemory;
    uint64_t freeMemory;
    uint64_t pageCacheMemory;
    double borrowedUsageRate;
    double memoryShortage;
    uint64_t pageCacheAppNums;
    uint64_t minFreeKbytes;
};

struct CgroupInfo {
    uint64_t pageCacheIn;     // pagecache 生成速度，单位KB/s
    uint64_t ioReadBandwidth;      // pagecache 生成速度，单位KB/s
};

//    Key: DockerId (string)
using DockerRawMap = std::map<std::string, CgroupInfos>;
using DockerResultMap = std::map<std::string, CgroupInfo>;

//    Key: NodeId (string)
using NodeRawMap = std::map<std::string, DockerRawMap>;
using NodeResultMap = std::map<std::string, DockerResultMap>;

class DataCollect {
public:
    static std::map<std::string, PhyNodeStat> phyNodeStatMap;
    // 策略执行框架调用
    static uint32_t CollectData();

    // 获取和设置借用策略用数据
    static void GetBorrowStrategyRawData(std::vector<BorrowStrategyRawData> &rawData);
    static void GetLoanableTotalBorrowMemMap(std::map<std::string, std::map<int, uint64_t>> &rawMap);
    static void GetPhysicalTopo(std::map<std::string, std::vector<std::string>> &topo);
    static void GetNumaSocketMap(std::map<std::string, std::map<int, int>> &socketMap);
    static void GetlendMemMap(std::map<std::string, std::vector<NodeMemBorrowInfo>> &lendMap);
    static void SetBorrowStrategyRawData(std::vector<BorrowStrategyRawData> &rawData);
    static void SetLoanableTotalBorrowMemMap(std::map<std::string, std::map<int, uint64_t>> &rawMap);
    static void SetPhysicalTopo(std::map<std::string, std::vector<std::string>> &topo);
    static void SetNumaSocketMap(std::map<std::string, std::map<int, int>> &socketMap);
    static void SetlendMemMap(std::map<std::string, std::vector<NodeMemBorrowInfo>> &lendMap);
    
    // 获取和设置迁移策略用数据
    static void GetNodeMemoryInfo(std::map<std::string, NodeMemoryInfo>& nodes);
    static void SetNodeMemoryInfo(std::map<std::string, NodeMemoryInfo>& nodes);

    // 查询生成内存借用账本
    static uint32_t GenerateborrowMemMap();
    static uint32_t GeneratelendMemMap();

    // 获取和设置借用和迁移策略共用数据
    static void GetborrowMemMap(std::map<std::string, std::vector<NodeMemBorrowInfo>> &borrowMap);
    static void SetborrowMemMap(std::map<std::string, std::vector<NodeMemBorrowInfo>> &borrowMap);

    // 获取和设置应用瓶颈识别分类用数据
    static void GetCgroupInfo(std::map<std::string, std::map<std::string, CgroupInfo>> &cgInfos);
    static void SetCgroupInfo(std::map<std::string, std::map<std::string, CgroupInfo>> &cgInfos);

    // 打印cgoupInfo信息
    static std::string PrintCgroupInfo(const std::map<std::string, std::map<std::string, CgroupInfo>>& cgroupInfo);

private:
    // 获取resource_export上报的数据
    static uint32_t GetResourceExportData();

    // 设置借用策略用数据
    static uint32_t GenerateBorrowStrategyRawData();
    static uint32_t GenerateLoanableTotalBorrowMemMap();
    static uint32_t GeneratePhysicalTopo();
    static uint32_t GenerateNumaSocketMap();
    static uint32_t GenerateBorrow();
    static void GenerateLendMap(const BorrowMemInfo &borrowMemInfo, const std::string &lendId,
                            std::map<std::string, std::vector<NodeMemBorrowInfo>> &MemMap);
    static void GenerateBorrowMap(const BorrowMemInfo &borrowMemInfo, const std::string &borrowId,
                            std::map<std::string, std::vector<NodeMemBorrowInfo>> &MemMap);
    
    // 生成迁移策略用数据
    static uint32_t GenerateNodeMemoryInfo();
    
    // 生成应用瓶颈识别分类数据
    static uint32_t GenerateCgroupInfo();

    // 借用策略数据
    static std::vector<BorrowStrategyRawData> borrowStrategyRawData;
    static std::map<std::string, std::map<int, uint64_t>> loanableTotalBorrowMemMap;
    static std::map<std::string, std::vector<std::string>> physicalTopo;
    static std::map<std::string, std::map<int, int>> numaSocketMap;
    static std::map<std::string, std::vector<NodeMemBorrowInfo>> lendMemMap;
    static std::map<std::string, std::vector<NodeMemBorrowInfo>> borrowMemMap;

    // 迁移策略数据
    static std::map<std::string, NodeMemoryInfo> nodeMemInfos;

    // 应用瓶颈识别分类数据
    static std::map<std::string, std::map<std::string, CgroupInfo>> cgroupInfo;

    // resource_export获取的数据
    static std::queue<NodeRawMap> cgroupInfosQueue;
    static std::map<std::string, std::map<std::string, NodeInfo>> nodeInfos;
    static std::map<std::string, uint64_t> memWaterMarkInfos;
    static std::queue<std::map<std::string, uint64_t>> timeStampsCgroup;
    static std::queue<std::map<std::string, uint64_t>> timeStampsNode;
};

} // end namespace data_collect

} // end namespace ucache

#endif // end macro UCACHE_DATA_COLLECT_H
