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
#include "mp_error.h"
#include "mp_memory_info.h"
#include "mempool_borrow_module.h"
#include "mem_manager.h"
#include "exporter.h"
#include "mem_manager.h"
#include "mp_smap_helper.h"
#include "mp_smap_module.h"
#include "mp_module.h"

namespace mempooling {
using namespace ubse::log;
using namespace mempooling::smap;

enum class NodeType {
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

class FaultNodeModule {
public:
    static FaultNodeModule &Instance()
    {
        static FaultNodeModule instance;
        return instance;
    }

    MpResult Init();
    MpResult DetermineNodeTypeOverCommit(const std::string nodeId, NodeType &nodeType);
    MpResult DetermineNodeTypeFragment(const std::string nodeId, NodeType &nodeType);
    MpResult FragmentHandleFault(std::string nodeId);
    bool CheckUBTurboIsAliveRpc();
    MpResult DetermineNodeType(const std::string nodeId, NodeType &nodeType);
    MpResult ProcessBorrowOutNodeFault(const std::string nodeId, bool forceDeleteMem);

    MpResult GetBorrowNodeInfo(std::string nodeId, std::vector<BorrowRecord> &borrowRecords);
    MpResult GetBorrowAbleNodeIdList(std::string curDealNodeId, std::vector<std::string> &borrowAbleNodeIdList);
    MpResult GetBorrowAbleNodeInfoSortByMemSize(std::vector<std::string> borrowAbleNodeIdList,
        std::vector<NodeMemoryInfoWithReservedMem> &ableNodeMemInfoList);
    void MergeBorrowRecords(std::vector<BorrowRecord> borrowRecordList,
        std::vector<NodeBorrowRecord> &nodeBorrowRecordList);
    MpResult NodeMayBorrowFromOtherNode(NodeBorrowRecord nodeBorrowRecord,
        std::vector<NodeMemoryInfoWithReservedMem> &ableNodeMemInfoList,
        std::vector<BorrowExecuteParam> &borrowExecuteParamCollectList);
    MpResult MayBorrowFromOtherNode(std::string curDealNodeId, std::vector<BorrowRecord> borrowRecords,
        std::vector<BorrowExecuteParam> &borrowExecuteParamCollectList,
        std::vector<ForwardMemIdParam> &forwardMemIdParamList);
    MpResult FillBorrowExecuteParam(std::vector<BorrowExecuteParam> &borrowExecuteParamCollectList);
    bool DealBorrowResult(std::vector<BorrowExecuteParam> &borrowExecuteParamList,
        std::vector<BorrowExecuteParam> &successExecuteParamCollectList,
        std::vector<MemBorrowExecuteResult> resultList);
    void ExecuteBorrow(std::vector<BorrowExecuteParam> &borrowExecuteParamCollectList,
        std::vector<BorrowExecuteParam> &successExecuteParamCollectList,
        std::vector<ForwardMemIdParam> &forwardMemIdParamList);
    MpResult ExecuteNumaReplaceAndReturn(std::vector<BorrowExecuteParam> &borrowExecuteParamCollectList, bool failFlag,
                                         bool forceDeleteMem);
    bool MayBorrowFromNuma(NodeBorrowRecord nodeBorrowRecord, NodeMemoryInfoWithReservedMem &originNodeMemInfoItem,
        NodeMemoryInfoWithReservedMem nodeMemInfoItem,
        std::vector<BorrowExecuteParam> &borrowExecuteParamCollectList);
    MpResult ForwardMemIdFaultDeal(std::vector<ForwardMemIdParam> forwardMemIdParamList, bool forceDeleteMem);
    bool QueryNumaAllPid(uint16_t numaId, std::vector<pid_t> &pidList);
    bool AllocateHugePage(uint16_t numaId, uint64_t hugePageMemSize);
    bool SwitchMigrateForNumaVm(std::vector<pid_t> pidList, int enable);
    bool GenerateMigrateNumaMsgList(NumaReplaceReturnMsg rpcMsg, std::vector<MigrateNumaMsg> &msgList);
    MpResult DealRes(NumaReplaceReturnMsg msg);
    bool ExecMigrateRemoteNumaToNuma(NumaReplaceReturnMsg rpcMsg, std::vector<MigrateNumaMsg> msgList);
    void DoExecuteBorrow(std::vector<BorrowExecuteParam> &successExecuteParamCollectList,
        std::pair<std::string, std::vector<BorrowExecuteParam>> nodeBorrowExecuteParam,
        std::vector<ForwardMemIdParam> &forwardMemIdParamList);
    
    // RPC Handler
    void CheckUBTurboIsAliveHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);
    void CheckUBTurboIsAliveResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode);

private:
    FaultNodeModule() = default;
    ~FaultNodeModule() = default;
    FaultNodeModule(const FaultNodeModule &) = delete;
    FaultNodeModule &operator=(const FaultNodeModule &) = delete;
    uint16_t faultHandleCurRound = 0;
};

void NodeNumaReplaceReturnHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);
void NodeNumaReplaceReturnResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode);
void GetPidListAndHugePageMemSize(const NumaReplaceReturnMsg &rpcMsg, std::vector<pid_t> &destPidList,
                                  uint64_t &hugePageMemSize);

class MpFaultNodeSubModule : public MpSubModule {
public:
    MpResult Init() override
    {
        // 注册节点级故障numa迁移以及归还消息处理器
        UbseComEndpoint endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_FAULT_NODE_NUMA_REPLACE_RETURN};
        auto ret = UbseRegRpcService(endpoint, NodeNumaReplaceReturnHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) <<
                "[MSG] NodeNumaReplaceReturn reg failed res: " << ret << ".";
            return ret;
        }

        // 注册ubturbo探活消息
        UbseComEndpoint endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_CHECK_UBTURBO_IS_ALIVE};
        auto ret = UbseRegRpcService(endpoint, FaultNodeModule::Instance().CheckUBTurboIsAliveHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) <<
                "[MSG] CheckUBTurboIsAliveHandler reg failed res: " << ret << ".";
            return ret;
        }

        return MEM_POOLING_OK;
    }
    void DeInit() override
    {
        return ;
    }
    const std::string Name() override
    {
        return "FaultNode";
    }
};

} // namespace mempooling
#endif // FAULT_NODE_MODULE_H