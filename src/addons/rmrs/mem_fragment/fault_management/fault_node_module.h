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

#ifndef FAULT_NODE_MODULE_H
#define FAULT_NODE_MODULE_H

#include <iostream>
#include <string>
#include <vector>
#include "ubse_logger.h"
#include "exporter.h"
#include "mem_manager.h"
#include "mempool_borrow_module.h"
#include "mp_error.h"
#include "mp_memory_info.h"
#include "mp_module.h"
#include "mp_smap_helper.h"
#include "mp_smap_module.h"

namespace mempooling {
using namespace ubse::log;
using namespace mempooling::smap;

enum class NodeType
{
    BORROW_IN,
    BORROW_OUT,
    NO_RECORD,
    ABNORMAL
};

struct BorrowExecuteParam {
    // 借入节点
    std::string borrowNodeId{};
    // 借入节点的那个numa
    uint16_t borrowNumaId{};
    // 接入节点socketId
    uint16_t borrowSocketId{};
    // 借出节点
    std::string lentNodeId{};
    // 借出节点的那个numa
    uint16_t lentNumaId{};
    // 借出节点那个socket
    uint16_t lentSocketId{};
    // 借用内存大小
    uint64_t memSize{};
    // 老的borrowId
    std::string oldBorrowId;
    // 老的numaId
    uint16_t oldNumaId;
    // 老的memId
    std::vector<uint64_t> memIdList;
    // 新的numaId
    uint16_t newNumaId;
    // 新的borrowId
    std::string newBorrowId;
    uid_t uid{0};
    std::string username{};

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "borrowNodeId=" << borrowNodeId << ",";
        oss << "borrowNumaId=" << borrowNumaId << ",";
        oss << "borrowSocketId=" << borrowSocketId << ",";
        oss << "lentNodeId=" << lentNodeId << ",";
        oss << "lentNumaId=" << lentNumaId << ",";
        oss << "memSize=" << memSize << ",";
        oss << "oldBorrowId=" << oldBorrowId << ",";
        oss << "[";
        for (uint64_t memIdItem : memIdList) {
            oss << "memIdItem=" << memIdItem << ",";
        }
        oss << "]";
        oss << "oldNumaId=" << oldNumaId << ",";
        oss << "newNumaId=" << newNumaId << ",";
        oss << "newBorrowId=" << newBorrowId << ",";
        oss << "uid=" << uid << ",";
        oss << "username=" << username;
        oss << "}";
        return oss.str();
    }
};

struct OnceBorrowRecord {
    std::string borrowId;
    uint64_t borrowSize;
    // 借入方memId
    std::vector<uint64_t> memIdList;
    // 借入方remotenumaId
    uint16_t numaId;
    // 借入方localnumaId
    uint16_t localNumaId;
    uid_t uid{0};
    std::string username{};
};

struct NodeBorrowRecord {
    std::string nodeId{};
    uint16_t borrowSocketId{};
    uint64_t borrowMemSize{};
    std::vector<OnceBorrowRecord> onceBorrowRecordList;
};

struct MigrateBorrowRecord {
    std::vector<uint64_t> memIdList;
    // byte
    uint64_t memSize;
};

struct ForwardMemIdParam {
    std::string importNodeId;
    std::vector<uint64_t> memIdList;
    uid_t uid{0};
    std::string username{};
};

struct NumaReplaceReturnMsg {
    // 源 远端NUMAID
    int srcNid;
    // 目的 远端NUMAID
    int destNid;
    std::vector<MigrateBorrowRecord> migrateBorrowRecordList;
    std::string faultNuma;
    std::vector<BorrowExecuteParam> BorrowExecuteParamVec;
};

enum class BorrowStrategyType
{
    NUMA_LEVEL_STRATEGY,      // 采用NUMA级别借用决策
    BORROW_ID_LEVEL_STRATEGY, // 采用borrowId级别借用决策
    STRATEGY_FAILED           // 决策失败
};

struct FaultNumaVmInfo {
    pid_t pid;
    uint16_t localNumaId;
    uint16_t remoteNumaId;
    uint64_t remoteNumaUsedMem;
};

struct NumaLevelDecision {
    std::vector<std::string> oldNames; // 旧的borrowId集合
    uint16_t borrowNumaId;             // 借入方numaId
    uint16_t borrowSocketId;           // 借入方socketId
    std::string lentNodeId;            // 借出方nodeId
    uint16_t lentSocketId;             // 决策出的借出方socketId
    uint64_t borrowSize;               // 决策出的借用内存大小
    uint16_t lentNumaId;               // 决策出的借出方numaId
    uint64_t lentMemSize;              // 决策出的借出方numaId上的借出内存大小(单位：KB)
    std::map<uint16_t, uint64_t> lentNumaIdAndSizeMap; // 决策出的借出方numaId -> 借出内存大小(单位：KB)
    bool isReturnDirectly = false;                     // 是否直接归还（无需借用迁移）
    std::vector<pid_t> pids;                           // 需逃生的虚机pid集合
    uid_t uid{0};           // 发起借用方运行用户的uid，后续资源管理权限都由此用户管理
    std::string username{}; // 发起借用方运行用户的名称，后续资源管理权限都由此用户管理
};

struct BorrowIdLevelDecision {
    std::string oldName;     // 旧的borrowId
    uint16_t borrowSocketId; // 借入方socketId
    uint16_t borrowNumaId;   // 借入方numaId
    uint64_t needBorrowSize; // 决策出的需要借用内存大小
    std::vector<pid_t> pids; // 需逃生的虚机pid集合
    std::string lentNodeId;  // 决策出的借出方nodeId
    uint16_t lentSocketId;   // 决策出的借出方socketId
    uint16_t lentNumaId;     // 决策出的借出方numaId
    uint64_t lentMemSize;    // 决策出的借出方numaId上的借出内存大小(单位：KB)
    std::map<uint16_t, uint64_t> lentNumaIdAndSizeMap; // 决策出的借出方numaId -> 借出内存大小(单位：KB)
    bool isReturnDirectly = false;                     // 是否直接归还（无需借用迁移）
    uid_t uid{0};           // 发起借用方运行用户的uid，后续资源管理权限都由此用户管理
    std::string username{}; // 发起借用方运行用户的名称，后续资源管理权限都由此用户管理
};

struct BorrowGroupResult {
    std::string borrowNodeId;             // 该故障numa对应的nodeId
    uint16_t borrowSocketId;              // 该故障numa对应的socketId
    uint16_t remoteNumaId;                // 该故障numa对应的远端numaId
    uint64_t totalSize;                   // 该故障numa上的总借用内存大小
    uint64_t totalUsedSize;               // 该故障numa上的已用内存大小
    std::vector<BorrowRecord> records;    // 该故障numa上的账本集合，已按size降序
    std::vector<FaultNumaVmInfo> vmInfos; // 该故障numa上的虚机信息集合，按占用大小降序
    BorrowStrategyType strategyType =
        BorrowStrategyType::STRATEGY_FAILED; // 该故障numa上的借用决策（NUMA级别/borrowId级别/决策失败）
    NumaLevelDecision numaDecision;          // NUMA级别决策结果
    std::vector<BorrowIdLevelDecision> borrowIdDecisions; // BorrowId级别决策结果（每个borrowRecord对应一个）
};

// 集群快照项（增加 totalBlocks 用于快速判断，派生自 numaCanLentMap）
struct ClusterSnapshotItem {
    std::string nodeId;
    uint16_t socketId;
    uint64_t freeMemSize;                        // 可借出内存（理论未对齐大小）
    uint64_t canLentMemSize;                     // 实际可借出内存（按blockSize对齐后的总大小）
    std::vector<uint16_t> numaIds;               // 该socket下的所有numaId
    std::map<uint16_t, uint64_t> numaCanLentMap; // numaId -> 可借block数量
    uint64_t totalBlocks;                        // 该 socket 可借block总数（派生，加速判断）
};

struct AllocResult {
    ClusterSnapshotItem* selected = nullptr;
    std::vector<std::pair<uint16_t, uint64_t>> allocatedNumas;
};

class FaultNodeModule {
public:
    static FaultNodeModule& Instance()
    {
        static FaultNodeModule instance;
        return instance;
    }

    MpResult Init();
    MpResult DetermineNodeTypeOverCommit(const std::string nodeId, NodeType& nodeType);
    MpResult DetermineNodeTypeFragment(const std::string nodeId, NodeType& nodeType);
    MpResult FragmentHandleFault(std::string nodeId);
    bool CheckUBTurboIsAliveRpc(std::string nodeId);
    MpResult DetermineNodeType(const std::string nodeId, NodeType& nodeType);
    MpResult ProcessBorrowOutNodeFault(const std::string nodeId, bool forceDeleteMem);

    MpResult GetBorrowNodeInfo(std::string nodeId, std::vector<BorrowRecord>& borrowRecords);
    MpResult GetBorrowAbleNodeIdList(std::string curDealNodeId, std::vector<std::string>& borrowAbleNodeIdList);
    MpResult GetBorrowAbleNodeInfoSortByMemSize(std::vector<std::string> borrowAbleNodeIdList,
                                                std::vector<NodeMemoryInfoWithReservedMem>& ableNodeMemInfoList);
    void MergeBorrowRecords(std::vector<BorrowRecord> borrowRecordList,
                            std::vector<NodeBorrowRecord>& nodeBorrowRecordList);
    MpResult NodeMayBorrowFromOtherNode(NodeBorrowRecord nodeBorrowRecord,
                                        std::vector<NodeMemoryInfoWithReservedMem>& ableNodeMemInfoList,
                                        std::vector<BorrowExecuteParam>& borrowExecuteParamCollectList);
    MpResult MayBorrowFromOtherNode(std::string curDealNodeId, std::vector<BorrowRecord> borrowRecords,
                                    std::vector<BorrowExecuteParam>& borrowExecuteParamCollectList,
                                    std::vector<ForwardMemIdParam>& forwardMemIdParamList);
    MpResult FillBorrowExecuteParam(std::vector<BorrowExecuteParam>& borrowExecuteParamCollectList);
    bool DealBorrowResult(std::vector<BorrowExecuteParam>& borrowExecuteParamList,
                          std::vector<BorrowExecuteParam>& successExecuteParamCollectList,
                          std::vector<MemBorrowExecuteResult> resultList);
    void ExecuteBorrow(std::vector<BorrowExecuteParam>& borrowExecuteParamCollectList,
                       std::vector<BorrowExecuteParam>& successExecuteParamCollectList,
                       std::vector<ForwardMemIdParam>& forwardMemIdParamList);
    MpResult ExecuteNumaReplaceAndReturn(std::vector<BorrowExecuteParam>& borrowExecuteParamCollectList, bool failFlag,
                                         bool forceDeleteMem);
    bool MayBorrowFromNuma(NodeBorrowRecord nodeBorrowRecord, NodeMemoryInfoWithReservedMem& originNodeMemInfoItem,
                           NodeMemoryInfoWithReservedMem nodeMemInfoItem,
                           std::vector<BorrowExecuteParam>& borrowExecuteParamCollectList);
    MpResult ForwardMemIdFaultDeal(std::vector<ForwardMemIdParam> forwardMemIdParamList, bool forceDeleteMem);
    bool QueryNumaAllPid(uint16_t numaId, std::vector<pid_t>& pidList);
    bool AllocateHugePage(uint16_t numaId, uint64_t hugePageMemSize);
    bool SwitchMigrateForNumaVm(std::vector<pid_t> pidList, int enable);
    bool GenerateMigrateNumaMsgList(NumaReplaceReturnMsg rpcMsg, std::vector<MigrateNumaMsg>& msgList);
    MpResult DealRes(NumaReplaceReturnMsg msg);
    bool ExecMigrateRemoteNumaToNuma(NumaReplaceReturnMsg rpcMsg, std::vector<MigrateNumaMsg> msgList);
    void DoExecuteBorrow(std::vector<BorrowExecuteParam>& successExecuteParamCollectList,
                         std::pair<std::string, std::vector<BorrowExecuteParam>> nodeBorrowExecuteParam,
                         std::vector<ForwardMemIdParam>& forwardMemIdParamList);

    MpResult ProcessBorrowOutNodeFaultParallel(const std::string nodeId, bool forceDeleteMem);
    MpResult FaultHandleInfosCollect(const std::string& faultNodeId, std::vector<BorrowGroupResult>& borrowGroups,
                                     std::vector<ClusterSnapshotItem>& baseSnapshot);
    std::vector<BorrowGroupResult> GroupBorrowRecordsByNuma(const std::vector<BorrowRecord>& records);
    MpResult GetVmOccupancyForGroup(const std::string& borrowNodeId, uint16_t remoteNumaId,
                                    std::vector<FaultNumaVmInfo>& vmInfos);
    MpResult GetBaseClusterSnapshot(const std::string& faultNodeId, std::vector<ClusterSnapshotItem>& clusterInfos);
    MpResult FaultHandleBorrowStrategy(std::vector<BorrowGroupResult>& borrowGroups,
                                       std::vector<ClusterSnapshotItem>& baseSnapshot);
    void GenerateNumaLevelDecision(std::vector<BorrowGroupResult>& borrowGroups,
                                   std::vector<ClusterSnapshotItem>& baseSnapshot, bool mustSamePlane);
    void GenerateBorrowIdLevelDecision(std::vector<BorrowGroupResult>& borrowGroups,
                                       std::vector<ClusterSnapshotItem>& baseSnapshot, bool mustSamePlane);
    std::vector<ClusterSnapshotItem*> FilterSnapshotByBorrowNode(const std::string& borrowNodeId,
                                                                 uint16_t borrowSocketId,
                                                                 std::vector<ClusterSnapshotItem>& clusterInfos);
    void NumaLevelDecisionFill(BorrowGroupResult& group, ClusterSnapshotItem& best, uint64_t needBlocks,
                               std::vector<std::pair<uint16_t, uint64_t>>& allocatedNumas);
    bool TryBestFitAllocate(const BorrowGroupResult& group, const std::vector<ClusterSnapshotItem*>& candidates,
                            uint64_t needBlocks, bool samePlaneOnly, AllocResult& result);
    void RemoveMigratedPidsFromVmInfos(std::vector<FaultNumaVmInfo>& vmInfos, const std::vector<pid_t>& pids);
    MpResult BorrowIdLevelExecute(const BorrowGroupResult& group, BorrowIdLevelDecision decision);
    MpResult SendNumaLevelExecuteRpc(std::string& nodeId, BorrowGroupResult& group, MpResult& outResult);
    MpResult SendBorrowIdExecuteRpc(std::string& nodeId, BorrowGroupResult& group, MpResult& outResult);
    MpResult FaultHandleExecuteParallel(std::vector<BorrowGroupResult>& borrowGroups);
    MpResult NumaLevelExecute(const BorrowGroupResult& group, NumaLevelDecision decision,
                              std::map<std::string, MemBorrowExecuteResult>& tmpRedirectionMap);
    MpResult FaultHandleMigrate(uint16_t presentNumaId, uint16_t faultNumaId,
                                std::vector<pid_t>& pids, uint64_t borrowMemSize);
    MpResult NumaLevelMemBorrow(const BorrowGroupResult& group, NumaLevelDecision decision,
                                std::map<std::string, MemBorrowExecuteResult>& tmpRedirectionMap,
                                uint16_t& presentNumaId);
    static uint64_t GetBlockSizeKB();

private:
    FaultNodeModule() = default;
    ~FaultNodeModule() = default;
    FaultNodeModule(const FaultNodeModule&) = delete;
    FaultNodeModule& operator=(const FaultNodeModule&) = delete;
    uint16_t faultHandleCurRound = 0;
    static uint64_t sBlockSizeKb;                          // 缓存的块大小（KB）
    static void SetBlockSizeKB(const std::string& nodeId); // 根据节点设置块大小
};

// RPC Handler
uint32_t CheckUBTurboIsAliveHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
void CheckUBTurboIsAliveResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
void NodeNumaReplaceReturnHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
void NodeNumaReplaceReturnResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
void GetPidListAndHugePageMemSize(const NumaReplaceReturnMsg& rpcMsg, std::vector<pid_t>& destPidList,
                                  uint64_t& hugePageMemSize);
uint32_t NumaLevelExecuteHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
uint32_t BorrowIdLevelExecuteHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
void NumaLevelExecuteResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
void BorrowIdLevelExecuteResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);

class MpFaultNodeSubModule : public MpSubModule {
public:
    MpResult Init() override
    {
        // 注册节点级故障numa迁移以及归还消息处理器
        UbseComEndpoint endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_FAULT_NODE_NUMA_REPLACE_RETURN};
        auto ret = UbseRegRpcService(endpoint, NodeNumaReplaceReturnHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MSG] NodeNumaReplaceReturn reg failed res: " << ret << ".";
            return ret;
        }

        // 注册ubturbo探活消息
        endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_CHECK_UBTURBO_IS_ALIVE};
        ret = UbseRegRpcService(endpoint, CheckUBTurboIsAliveHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MSG] CheckUBTurboIsAliveHandler reg failed res: " << ret << ".";
            return ret;
        }

        // 注册 NUMA 级别执行 handler
        endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_NUMA_LEVEL_EXECUTE};
        ret = UbseRegRpcService(endpoint, NumaLevelExecuteHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MSG] NumaLevelExecuteHandler reg failed, ret=" << ret;
            return ret;
        }

        // 注册 BorrowId 级别执行 handler
        endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_BORROW_ID_LEVEL_EXECUTE};
        ret = UbseRegRpcService(endpoint, BorrowIdLevelExecuteHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MSG] BorrowIdLevelExecuteHandler reg failed, ret=" << ret;
            return ret;
        }

        return MEM_POOLING_OK;
    }
    void DeInit() override
    {
        return;
    }
    const std::string Name() override
    {
        return "FaultNode";
    }
};

} // namespace mempooling
#endif // FAULT_NODE_MODULE_H