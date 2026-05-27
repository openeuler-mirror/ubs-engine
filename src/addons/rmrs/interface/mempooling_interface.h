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

#ifndef MEMPOOL_OUT_INTERFACE_H
#define MEMPOOL_OUT_INTERFACE_H

#include <bits/types.h>
#include <cstring>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "exporter.h"
#include "mp_smap_helper.h"
#include "rmrs_resource_query.h"

extern "C" {
namespace mempooling::outinterface {
using namespace mempooling::smap;

struct SrcMemoryBorrowParam {
    SrcMemoryBorrowParam() = default;

    std::string srcNid{};   // 借入方节点Id
    int16_t srcSocketId{};  // 借入方socket Id
    int16_t srcNumaId{};    // 借入方numa Id 当前限制为1
    uid_t uid{0};           // 借入方用户uid
    std::string username{}; // 借入方用户名

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "srcNid=" << srcNid << ",";
        oss << "srcSocketId=" << srcSocketId << ",";
        oss << "srcNumaId=" << srcNumaId;
        oss << "uid=" << uid << ",";
        oss << "username=" << username << ".";
        oss << "}";
        return oss.str();
    }
};

struct BatchSrcMemoryBorrowParam {
    BatchSrcMemoryBorrowParam() = default;

    std::string srcNid{};             // 借入方节点Id
    uint16_t srcNumaNum{1};           // 借入方NUMA数量
    std::vector<int16_t> srcNumaId{}; // 借入方NUMA Id数组
    uid_t uid{0};                     // 借入方用户uid
    std::string username{};           // 借入方用户名

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "srcNid=" << srcNid << ",";
        oss << "srcNumaNum=" << srcNumaNum << ",";
        oss << "srcNumaId=[";
        for (size_t i = 0; i < srcNumaId.size(); ++i) {
            oss << srcNumaId[i];
            if (i < srcNumaId.size() - 1) {
                oss << ",";
            }
        }
        oss << "],";
        oss << "uid=" << uid << ",";
        oss << "username=" << username;
        oss << "}";
        return oss.str();
    }
};

enum class BorrowStrategy : uint8_t {
    AVERAGE = 0 // 平均分配策略
};

struct DestMemoryBorrowParam {
    DestMemoryBorrowParam() = default;

    std::string destNid{};           // 借出方节点Id
    uint16_t destSocketId{};         // 借出方socket Id
    uint16_t destNumaNum{1};         // 当前限制为1
    std::vector<int> destNumaId{};   // 借出方numaId
    std::vector<uint64_t> memSize{}; // 借用大小, 单位kB

    std::string ToString() const
    {
        size_t count = 0;
        size_t total = 0;
        std::ostringstream oss;
        oss << "{";
        oss << "destNid:" << destNid << ",";
        oss << "destSocketId:" << destSocketId << ",";
        oss << "destNumaId:[";
        total = destNumaId.size();
        for (const auto& destNumaIdItem : destNumaId) {
            oss << destNumaIdItem;
            if (++count < total) {
                oss << ",";
            }
        }
        oss << "],";
        oss << "memSize[";
        count = 0;
        total = memSize.size();
        for (const auto& memSizeItem : memSize) {
            oss << memSizeItem;
            if (++count < total) {
                oss << ",";
            }
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

struct MemBorrowExecuteResult {
    MemBorrowExecuteResult() = default;

    std::vector<std::string> borrowIds{};   // 内存描述符数组
    std::vector<uint16_t> presentNumaIds{}; // 呈现的远端Numa数组

    bool operator==(const MemBorrowExecuteResult& memBorrowExecuteResult) const
    {
        if (this->borrowIds == memBorrowExecuteResult.borrowIds &&
            this->presentNumaIds == memBorrowExecuteResult.presentNumaIds) {
            return true;
        }
        return false;
    }

    std::string ToString() const
    {
        size_t count = 0;
        size_t total = 0;
        std::ostringstream oss;
        oss << "borrowIds:[";
        total = borrowIds.size();
        for (const auto& borrowId : borrowIds) {
            oss << borrowId;
            if (++count < total) {
                oss << ",";
            }
        }
        oss << "],";
        oss << "presentNumaIds[";
        count = 0;
        total = presentNumaIds.size();
        for (const auto& presentNumaId : presentNumaIds) {
            oss << presentNumaId;
            if (++count < total) {
                oss << ",";
            }
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

struct MemBorrowStrategyResult {
    MemBorrowStrategyResult() = default;

    SrcMemoryBorrowParam srcParam{};
    uint64_t borrowSize{}; // 内存借用大小
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

struct VMPresetParam {
    VMPresetParam() = default;

    pid_t pid{};      // vm对应pid
    uint16_t ratio{}; // 迁出最大比例
};

struct VMMigrateOutParam {
    VMMigrateOutParam() = default;

    pid_t pid{};
    uint64_t memSize{};   // 决策迁出大小
    uint16_t desNumaId{}; // 迁移远端numa
};

struct MigrateStrategyResult {
    MigrateStrategyResult() = default;

    uint32_t result{};
    std::vector<VMMigrateOutParam> vmInfoList{};
    uint64_t waitingTime{}; // 单位ms
};

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
    uint64_t hugePageTotal{}; // 该numa上pageSize类型大页数量
    uint64_t hugePageFree{};  // 该numa上pageSize类型空闲的大页数量
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

    std::string nodeId{};   // 节点Id
    std::string hostName{}; // 节点HostName
    int16_t numaId{};       // numaId
    int16_t socketId{};     // 该numa绑定cpu映射socketId
    bool isLocal{};         // 是否是本地numa  0：非本地  1：本地
    uint64_t memTotal{};    // 该numa节点内存总量(包含)，系统文件采集，kb
    uint64_t memFree{};     // 该numa上空闲内存，系统文件采集，kb
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
    time_t timestamp{};
    NumaMetaData metaData{}; // numa元信息

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{metaData:[" << metaData.ToString() << "],";
        oss << "timestamp:" << timestamp << "}";
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
        oss << R"("highWaterMark":)" << highWaterMark << R"(})";
        return oss.str();
    }
};

struct NumaQuota {
    uint32_t numaId{};
    uint32_t quota{}; // 单位 KB

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{numaId=" << numaId << ",quota=" << quota << "KB}";
        return oss.str();
    }
};

struct PageSwapPair {
    std::vector<NumaQuota> localNumas{};
    std::vector<NumaQuota> remoteNumas{};

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{localNumas=[";
        for (size_t i = 0; i < localNumas.size(); ++i) {
            oss << localNumas[i].ToString();
            if (i < localNumas.size() - 1) {
                oss << ",";
            }
        }
        oss << "],remoteNumas=[";
        for (size_t i = 0; i < remoteNumas.size(); ++i) {
            oss << remoteNumas[i].ToString();
            if (i < remoteNumas.size() - 1) {
                oss << ",";
            }
        }
        oss << "]}";
        return oss.str();
    }
};

struct PidInfo {
    pid_t pid{};
    uint64_t localUsedMem{};
    std::vector<uint16_t> localNumaIds{};
    uint64_t remoteUsedMem{};
    uint16_t remoteNumaId{};

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{pid:" << pid << ",";
        oss << "localUsedMem:" << localUsedMem << "BYTE,";
        oss << "localNumaIds:[";
        for (size_t i = 0; i < localNumaIds.size(); ++i) {
            oss << localNumaIds[i];
            if (i != localNumaIds.size() - 1) {
                oss << ", ";
            }
        }
        oss << "],remoteUsedMem:" << remoteUsedMem << "BYTE,";
        oss << "remoteNumaId:" << remoteNumaId << "}";
        return oss.str();
    }
};

/**
 * @brief 反亲和性配置
 * @param nodeAntiAffinityMap 反亲和性节点映射
 * @return  0为成功, 非0为异常
 */
uint32_t UBSRMRSUpdateAntiNode(const std::map<std::string, std::vector<std::string>>& nodeAntiMap);

/**
 * @brief 内存借用策略
 * @param outSrcParam 借入节点信息
 * @param borrowSize 需要借入的内存大小(单位kB)
 * @return  0为成功, 非0为异常
 */
uint32_t UBSRMRSMemBorrowStrategy(const SrcMemoryBorrowParam& outSrcParam, const uint64_t& borrowSize,
                                  MemBorrowStrategyResult& outBorrowStrategyResult);

/**
 * @brief 批量内存借用策略
 * @param outSrcParam 批量借入节点信息
 * @param borrowSize 需要借入的内存大小(单位kB)
 * @param outBorrowStrategyResult 借用策略结果数组
 * @param borrowStrategy 借用策略类型
 * @return  0为成功, 非0为异常
 */
uint32_t UBSRMRSBatchBorrowStrategy(const BatchSrcMemoryBorrowParam& outSrcParam, const uint64_t& borrowSize,
                                    std::vector<MemBorrowStrategyResult>& outBorrowStrategyResult,
                                    BorrowStrategy borrowStrategy);
/**
 * @brief 内存借用执行
 * @param outSrcParam 借入节点信息
 * @param outDestParam 决策结果
 * @param outBorrowExecuteResult 执行结果（内存描述符数组，呈现的远端Numa数组）
 * @return  0为成功, 非0为异常
 */
uint32_t UBSRMRSMemBorrowExecute(const SrcMemoryBorrowParam& outSrcParam,
                                 const std::vector<DestMemoryBorrowParam>& outDestParam,
                                 MemBorrowExecuteResult& outBorrowExecuteResult);
/**
 * @brief 内存迁出策略
 * @param borrowInNode 内存借入节点
 * @param outVmInfoList 预设迁出最大比例（虚拟机迁出最大比例{进程id，可迁出最大比例}）
 * @param borrowSize 匀出本地内存大小
 * @param outMigrateStrategyResult 内存迁出策略结果
 * @return  0为成功, 非0为异常
 */
uint32_t UBSRMRSMigrateStrategy(const std::string& borrowInNode, const std::vector<VMPresetParam>& outVmInfoList,
                                const uint64_t& borrowSize, MigrateStrategyResult& outMigrateStrategyResult);

/**
 * @brief 内存迁出执行
 * @param borrowInNode 内存借入节点
 * @param outVmInfoList 虚拟机信息列表{进程id，实际迁出比例，迁出远端numa}
 * @param waitingTime 等待时间(ms)
 * @param borrowIdList 内存描述符列表
 * @return  0为成功, 非0为异常
 */
uint32_t UBSRMRSMigrateExecute(const std::string& borrowInNode, const std::vector<VMMigrateOutParam>& outVmInfoList,
                               const std::uint64_t& waitingTime, const std::vector<std::string>& borrowIds);

/**
 * @brief 内存归还策略与执行
 * @param nodeId 内存借入节点
 * @return 返回错误码：
 *         0   成功
 *         1   失败通用错误码
 *         2   迁移失败-迁移过程中虚机被删除
 *         3   迁移失败通用错误码
 *         4   内存资源删除失败
 */
uint32_t UBSRMRSMemFree(const std::string& nodeId);
/**
 * @brief 内存归还执行
 * @param borrowId 内存借入borrowId
 * @return 返回错误码：
 *         0   成功
 *         1   失败通用错误码
 *         2   迁移失败-迁移过程中虚机被删除
 *         3   迁移失败通用错误码
 *         4   内存资源删除失败
 */
uint32_t UBSRMRSMemFreeWithMigrate(const std::string& borrowId);

/**
 * @brief 借用内存回滚
 * @param borrowInNode 回滚节点id
 * @param borrowIds 借用记录id
 * @return  0为成功, 非0为异常
 */
uint32_t UBSRMRSMemBorrowRollback(const std::string& borrowInNode, const std::vector<std::string>& borrowIds);

/**
 * @brief 查询本节点所有虚机信息
 * @param outVmDomainInfos 虚机字段信息列表
 * @return  0为成功, 非0为异常
 */
uint32_t UBSRMRSGetVmInfoListOnNode(std::vector<VmDomainInfo>& outVmDomainInfos);

/**
 * @brief 查询本节点所有NUMA信息
 * @param outNumaInfos  NUMA字段信息列表
 * @return  0为成功, 非0为异常
 */
uint32_t UBSRMRSGetNumaInfoListOnNode(std::vector<NumaInfo>& outNumaInfos);

/**
 * @brief 超分内存借用
 * @param srcParam 借入节点信息
 * @param borrowSizes 决策结果
 * @param result 执行结果（内存描述符数组，呈现的远端Numa数组）
 * @return  0为成功, 非0为异常
 */
uint32_t UBSRMRSMemBorrow(const SrcMemoryBorrowParam& srcParam, const std::vector<uint64_t>& borrowSizes,
                          const WaterMark& waterMark, MemBorrowExecuteResult& result);

/**
 * @brief 超分内存分配
 * @param srcParam 借入节点信息
 * @param vmPresetParams 需要迁移虚拟机内存迁移参数
 * @param result 执行结果（内存描述符数组，呈现的远端Numa数组
 * @return  0为成功, 非0为异常
 */
uint32_t UBSRMRSMemMigrate(const SrcMemoryBorrowParam& srcParam, const std::vector<VMPresetParam>& vmPresetParams,
                           const MemBorrowExecuteResult& result);

/**
 * @brief 超分归还请求
 * @param srcParam 借入节点信息
 * @param borrowIds 内存描述符数组
 * @param pids 借入numa上虚拟机进程列表
 * @return  0为成功, 非0为异常
 */
uint32_t UBSRMRSMemReturn(const SrcMemoryBorrowParam& srcParam, const std::vector<std::string>& borrowIds,
                          const std::vector<pid_t>& pids);

/**
 * @brief 容器numa信息采集
 * @param srcParam 借入节点信息
 * @param pids 容器pid列表
 * @param pidInfos 存放容器信息采集结果
 * @return  0为成功, 非0为异常
 */
uint32_t UBSRMRSPidNumaInfoCollect(const SrcMemoryBorrowParam& srcParam, const std::vector<pid_t>& pids,
                                   std::vector<PidInfo>& pidInfos);

uint32_t UBSRMRSSetWaterMark(const WaterMark& waterMark);

/**
 * @brief 设置超分/碎片场景
 * @param runMode 场景类型 (0:超分场景; 1:内存碎片场景)
 * @return 0为成功, SMAP未初始化返回21，参数错误返回22，其他错误情况返回1
 */
uint32_t UBSRMRSSetRunMode(const int& runMode);

/**
 * @brief 通知SMAP添加进程扫描，并设置扫描周期参数
 * @param pidVec 需要添加进程扫描的进程数组
 * @param scanTimeVec 扫描周期数组，50毫秒的倍数，最小值50毫秒，最大值为200毫秒
 * @param scanType 标志位，0：HAM_SCAN，供HAM使用。1：NORMAL_SCAN。2：STATISTIC_SCAN, 此模式可以统计特定时长的访存频次信息
 * @param durationVec 统计周期数组，有效值{1~300}，单位s，在scanType=2时使用，默认值为1s
 * @return 0为成功, SMAP未初始化返回-1, 参数错误返回-22, 内核态内存申请失败返回-9, 用户态内存申请失败返回-12, OSTurbo通信失败返回17
 */
int UBSRMRSSmapAddProcessTracking(const std::vector<pid_t>& pidVec, const std::vector<uint32_t>& scanTimeVec,
                                  int scanType, const std::optional<std::vector<uint32_t>>& durationVec = std::nullopt);

/**
 * @brief 查询虚机冷热信息
 * @param pid 进程PID号
 * @param dataVec 存放冷热信息的数组
 * @param lengthIn 指示data数组的大小
 * @param lengthOut 返回实际写入data数组的大小
 * @param dataSource 标识数据来源，0表示迁移进程的冷热信息，1表示统计扫描进程的冷热信息
 * @return 0为成功, SMAP未初始化返回-1, 参数错误返回-22, 没有达到统计周期返回-11, OSTurbo通信失败返回17
 */
int UBSRMRSSmapQueryFreq(const pid_t& pid, std::vector<uint16_t>& dataVec, const uint32_t& lengthIn,
                         uint32_t& lengthOut, const int& dataSource);

/**
 * @brief 通知SMAP移除进程扫描
 * @param pidVec PID数组
 * @param flags 保留字段, 默认值为0
 * @return 0为成功, SMAP未初始化返回-1，参数错误返回-22, OSTurbo通信失败返回17
 */
int UBSRMRSSmapRemoveProcessTracking(const std::vector<pid_t>& pidVec, int flags = 0);

/**
 * @brief 启用/禁用PID对应虚机的冷热迁移和迁回
 * @param pidVec PID数组
 * @param enable 0-禁用 1-启用。
 * @param flags 保留字段, 默认值为0
 * @return 0为成功, SMAP未初始化返回-1，参数错误返回-22，超时返回-110,  OSTurbo通信失败返回17
 */
int UBSRMRSSmapEnableProcessMigrate(std::vector<pid_t> pidVec, int enable, int flags = 0);

/**
 * @brief 分组启用进程冷热迁移
 * @param pid 进程PID
 * @param pageSwapPairs 页交换配对数组（每个PageSwapPair映射为一个MigrationGroup）
 * @return  0为成功, 非0为异常
 */
uint32_t UBSRMRSSmapEnableProcessMigrateGrouped(pid_t pid, const std::vector<PageSwapPair>& pageSwapPairs);

/* *
 * @brief   设置进程迁移到远端NUMA，异步调用接口
 *
 * @param items    [IN] 迁移进程信息，包含迁移进程、远端NUMA和迁移比例
 * @param pidType  [IN] 进程类型，目前支持4KB和2MB进程类型，int配置类型：0-进程（4K）1-虚拟机（2M）
 * @return int  0：操作成功；非0：操作失败
 */
int UBSRMRSMigrateOut(const std::vector<MigrateOutPayload>& items, int pidType);

/* *
 * @brief   移除进程的冷热页迁移
 *
 * @param remoteNumaId   [IN] 需移除pids对应的远端numa
 * @param pids   [IN] 移除的进程信息，包含进程的PID
 * @param pidType  [IN] 进程类型，目前支持4KB和2MB进程类型，int配置类型：0-进程（4K）1-虚拟机（2M）
 * @return int  0：操作成功；非0：操作失败
 */
int UBSRMRSRemove(const uint16_t remoteNumaId, const std::vector<pid_t>& pids, int pidType);

/* *
 * @brief   迁移指定进程远端内存到远端内存
 *
 * @param pids     [IN] 移除的进程信息，包含进程的PID
 * @param srcNid   [IN] 源远端NUMA
 * @param destNid  [IN] 目标远端NUMA
 * @return int  0：操作成功；非0：操作失败
 */
int RemoteNumaMigrate(const std::vector<pid_t>& pids, int srcNid, int destNid);

class ApiConcurrencyManager {
public:
    // 获取单例实例
    static ApiConcurrencyManager& getInstance()
    {
        static ApiConcurrencyManager instance;
        return instance;
    }

    // 删除拷贝构造函数和赋值操作符
    ApiConcurrencyManager(const ApiConcurrencyManager&) = delete;
    ApiConcurrencyManager& operator=(const ApiConcurrencyManager&) = delete;

    // 原有的业务方法
    bool TryEnterOtherFunc();
    void ExitOtherFunc();
    bool TryEnterMemReturnFunc();
    void ExitMemReturnFunc();

private:
    // 私有构造函数
    ApiConcurrencyManager() = default;

    std::mutex m_mtx;
    bool m_isRunningOther = false;
    int m_runningMemReturnCount = 0;
};

} // namespace mempooling::outinterface
}

#endif // MEMPOOL_OUT_INTERFACE_H