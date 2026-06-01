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

#ifndef FAULT_MEMID_MODULE_H
#define FAULT_MEMID_MODULE_H

#include <iostream>
#include <string>
#include <vector>
#include "exporter.h"
#include "mempool_borrow_module.h"
#include "mempool_migrate_module.h"
#include "mp_error.h"
#include "mp_json_util.h"

namespace mempooling {
using namespace mempooling::migrate;

constexpr size_t VECTOR_REMOTE_NUMA_SIZE = 2; // 规定接收memid故障处理远端迁移远端的数组大小为2
const uint64_t m_miniUnitMemSize = 131072;
const uint64_t m_maxUnitMemSize = 4194304;

// 存储回溯状态的结构体
struct CombinationState {
    uint64_t currentSum = 0;               //  当前虚拟机迁出内存的总和
    std::vector<pid_t> currentCombination; //  当前虚拟机pid组合
    std::vector<pid_t> result;             //  最接近迁出内存pid结果
    uint64_t minDiff = UINT64_MAX;         //  当前组合和最小差值
};

struct BorrowInNodeData {
    std::string borrowInNid;
    uint64_t memId;
    std::string borrowId;
    uid_t uid{0};
    std::string username{};
    int16_t borrowNumaId{0};
    int16_t borrowSocketId{0};
};

struct MemBorrowStrategyParam {
    std::string destPreNid;
    uint16_t destSocketId;
};

// 发送memId故障所在numa节点信息
struct CurNumaInfoMF {
    uint16_t remoteNumaId;    // 发送memId故障所在numa节点
    uint64_t memBorrowIdSize; // 故障memid对应借用内存大小
    std::string borrowId;     // 故障memid对应借用name
    std::string destPreNid;   // 故障memid是从那个物理节点借用
    uint16_t destSocketId;    // 故障memid是从那个SocketId借用

    void SetValues(uint16_t rNumaId, uint64_t mBorrowIdSize, const std::string& bId, const std::string& dPreNid,
                   uint16_t dSocketId)
    {
        remoteNumaId = rNumaId;
        memBorrowIdSize = mBorrowIdSize;
        borrowId = bId;
        destPreNid = dPreNid;
        destSocketId = dSocketId;
    }
};

// 不同节点故障处理执行，序列化和反序列化
struct FMNotSameNidParam {
    std::vector<uint16_t> remoteNumas;
    uint64_t totalNeedBorrowMem;
    std::vector<pid_t> pids;
    std::string borrowId;
    std::string borrowIdNew;
};

// 相同节点故障处理执行，序列化和反序列化
struct FMSameNidParam {
    uint64_t remoteNumaHuge;
    uint64_t memBorrowIdSize;
    std::string borrowId;
    std::string borrowIdNew;
};

// 不相同节点故障处理，查询虚拟机信息，发送参数，序列化和反序列化
struct FMVmInfoParam {
    uint16_t remoteNumaId;
    uint64_t memBorrowIdSize;
};

// 不相同节点故障处理，查询虚拟机信息，接收结果，序列化和反序列化
struct FMVmInfoResult {
    std::vector<pid_t> pids;
    uint64_t totalNeedBorrowMem;
};

struct NotSameNidExecuteParam {
    std::string importNodeId;    // 内存借入节点
    uint16_t remoteNumaHuge;     // 需要分大页的远端numa
    uint64_t totalNeedBorrowMem; // 总共需要借用的内存
    uint16_t remoteNumaId;       // 虚拟机所在远端numa
    std::string borrowId;        // borrowID
};

// 即将uce故障的内存参数
struct UCEMemoryParam {
    uint64_t memBorrowIdSize; // 内存大小 kb
    std::string borrowId;     // 即将UCE故障内存对应的borrowId
};

struct NotSameNidMemoryParam {
    std::string borrowId;        // 需要归还内存的borrowId
    uint64_t totalNeedBorrowMem; // 需要借用内存的大小 kb
};

uint32_t MemIdFaultNotSameNidRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
void MemIdFaultNotSameNidResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
uint32_t MemIdFaultSameNidRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
void MemIdFaultSameNidResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
uint32_t MemIdFaultNotSameNidVmInfoRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
void MemIdFaultNotSameNidVmInfoResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);

class FaultMemIdCollect {
public:
    static FaultMemIdCollect& Instance()
    {
        static FaultMemIdCollect instance;
        return instance;
    }
    MpResult GetRemoteNumaVms(uint16_t remoteNumaId, std::vector<VmNumaInfo>& allVmNumaInfoInfoList,
                              std::map<pid_t, VmNumaInfo>& vmNumaInfoMap);
    MpResult GetSocketIdOfNUMA(std::string borrowInNid, SrcMemoryBorrowParam& srcParam);
    MpResult IsBorrowIdOfCurNid(BorrowInNodeData& borrowInNodeData, uint64_t& memBorrowIdSize, uint16_t& remoteNumaId,
                                std::string& destPreNid, uint16_t& destSocketId);
    MpResult NotSameNidVmInfoRpc(std::string importNodeId, uint16_t remoteNumaId, uint64_t memBorrowIdSize,
                                 FMVmInfoResult& fMVmInfoResult);
    MpResult NotSameNidVmInfo(uint16_t remoteNumaId, uint64_t memBorrowIdSize, std::vector<pid_t>& pids,
                              uint64_t& totalNeedBorrowMem);
};

class FaultMemIdExecute {
public:
    static FaultMemIdExecute& Instance()
    {
        static FaultMemIdExecute instance;
        return instance;
    }
    MpResult RollBackHugepages(uint64_t remoteNumeId, uint64_t borrowMemSize);
    MpResult EchoHugepages(uint64_t remoteNumeId, uint64_t borrowMemSize);
    MpResult NotSameNidExecute(std::vector<uint16_t> remoteNumas, uint64_t totalNeedBorrowMem, std::vector<pid_t>& pids,
                               std::string borrowId, bool isForce);
    MpResult NotSameNidExecuteRpc(NotSameNidExecuteParam& param, std::vector<pid_t>& pids, std::string borrowIdNew);
    MpResult SameNidExecuteRpc(std::string importNodeId, uint64_t remoteNumaHuge, uint64_t memBorrowIdSize,
                               std::string borrowId, std::string borrowIdNew);
    MpResult SameNidExecute(uint64_t remoteNumaHuge, uint64_t memBorrowIdSize, std::string borrowId,
                            std::string borrowIdNew);
    MpResult VmsMigrateOtherRemoteNuma(std::vector<pid_t>& pids, uint16_t remoteNumaId, uint16_t remoteNumaHuge,
                                       std::string borrowId, bool isForce);
};

class FaultMemIdStrategy {
public:
    static FaultMemIdStrategy& Instance()
    {
        static FaultMemIdStrategy instance;
        return instance;
    }
    MpResult AdjustNeedBorrowMem(uint64_t& needBorrowMem);
    bool IsMemBorrowNotWipeTheEdge(const std::vector<pid_t>& pids, const std::vector<VmNumaInfo>& allVmNumaInfoInfoList,
                                   uint64_t& totalNeedBorrowMem);
    MpResult ApplyMemBorrowStrategyMultipleUB(const mempooling::SrcMemoryBorrowParam& srcParam,
                                              const std::vector<uint64_t>& borrowSizes,
                                              const MemBorrowStrategyParam& memBorrowStrategyParam,
                                              MemBorrowStrategyMultiResult& borrowStrategyMultiResult,
                                              bool& isSameDestNid);
    void PrintBorrowStrategyMultiResult(const MemBorrowStrategyMultiResult& borrowStrategyMultiResult,
                                        const bool& isSameDestNid);
    MpResult MemBorrPostprocess(std::string borrowId,
                                const mempooling::MemBorrowExecuteResult& borrowExecuteResultNotSameNid,
                                uint64_t totalNeedBorrowMem);
};

class FaultMemIdModule {
public:
    static FaultMemIdModule& Instance()
    {
        static FaultMemIdModule instance;
        return instance;
    }

    MpResult Init();

    MpResult MemIdFaultManage(std::string borrowInNid, uint64_t memId, bool isForce, bool byNodeFault);

    static bool compareVmNumaInfo(const VmNumaInfo& a, const VmNumaInfo& b);

    MpResult FindClosestVmForMemAlloc(std::vector<VmNumaInfo>& allVmNumaInfoInfoList, uint64_t memSizeSingle,
                                      std::vector<pid_t>& pids, uint64_t& totalNeedBorrowMem);

    MpResult ClosestVmVector(const std::vector<VmNumaInfo>& allVmNumaInfoInfoList, uint64_t memSizeSingle,
                             std::vector<pid_t>& pids, uint64_t& totalNeedBorrowMem);

    MpResult BorrowFromSameNid(std::string borrowInNid, const mempooling::SrcMemoryBorrowParam& srcParam,
                               const MemBorrowStrategyMultiResult& borrowStrategyMultiResult,
                               const UCEMemoryParam& memParam, bool isForce);

    MpResult BorrowFromNotSameNid(std::string borrowInNid, const CurNumaInfoMF& curNumaInfoMF,
                                  const mempooling::SrcMemoryBorrowParam& srcParam, bool byNodeFault, bool isForce);

    MpResult NotSameMemBoorNidExecute(const mempooling::SrcMemoryBorrowParam& srcParam,
                                      const MemBorrowStrategyMultiResult& borrowStrategyMultiResultNotSameNid,
                                      std::string& borrowIdNew, uint64_t totalNeedBorrowMem, uint16_t& remoteNumaHuge);

    MpResult GetSocketIdOfNUMA(std::string borrowInNid, SrcMemoryBorrowParam& srcParam, std::string destPreNid,
                               uint16_t destSocketId);

    MpResult NotSameNidDeleteUpdate(std::string borrowId, std::string borrowIdNew);

private:
    FaultMemIdModule() = default;
    ~FaultMemIdModule() = default;
    FaultMemIdModule(const FaultMemIdModule&) = delete;
    FaultMemIdModule& operator=(const FaultMemIdModule&) = delete;

    void findCombination(const std::vector<VmNumaInfo>& allVmNumaInfoInfoList, uint64_t memSizeSingle, int start,
                         CombinationState& state);
};

} // namespace mempooling
#endif // FAULT_MEMID_MODULE_H