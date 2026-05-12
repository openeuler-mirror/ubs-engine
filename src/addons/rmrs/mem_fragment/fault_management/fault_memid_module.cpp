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

#include "fault_memid_module.h"
#include <algorithm>

#include "exporter.h"
#include "fault_node_module.h"
#include "mem_borrow_executor.h"
#include "mem_manager.h"
#include "mp_smap_helper.h"
#include "rmrs_serialize.h"
#include "ubse_com.h"
#include "ubse_logger.h"

namespace mempooling {
using namespace ubse::log;
using namespace ubse::mem::controller;
using namespace ubse::com;
using namespace mempooling::smap;
using namespace rmrs::serialize;

constexpr int SMAP_OK = 0;
constexpr int SMAP_ERROR = 1;
constexpr auto DELETERE_KIND = "MEM";
constexpr size_t BORROW_RESULT_SIZE = 1;
constexpr uint64_t KB_TO_BYTES = 1024;
constexpr size_t ECHO_HUGE_PAGE_NUM = 1;
constexpr int SMAP_MIGRATE_DISABLE = 0;
constexpr int SMAP_MIGRATE_ENABLE = 1;
constexpr int SMAP_MIGRATE_FLAGS = 0;
constexpr size_t RESP_LENGTH = 2;

#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)

MpResult FaultMemIdModule::BorrowFromSameNid(std::string borrowInNid, const mempooling::SrcMemoryBorrowParam &srcParam,
                                             const MemBorrowStrategyMultiResult &borrowStrategyMultiResult,
                                             const UCEMemoryParam &memParam, bool isForce)
{
    LOG_DEBUG << "[FaultManager][MemId] MemIdFaultManage is same dest nid.";
    uint64_t memBorrowIdSize = memParam.memBorrowIdSize;
    std::string borrowId = memParam.borrowId;
    MemBorrowExecuteResult borrowExecuteResult;
    MpResult ret = MempoolBorrowModule::Instance().MemBorrowExecute(srcParam, borrowStrategyMultiResult.destParam,
                                                                    borrowExecuteResult);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] MemBorrow execute failed.";
        return MEM_POOLING_ERROR;
    } else if (borrowExecuteResult.presentNumaId.size() != BORROW_RESULT_SIZE) {
        LOG_ERROR << "[FaultManager][MemId] MemBorrow execute failed.";
        LOG_DEBUG << "[FaultManager][MemId] BorrowExecute error, presentNumaId size is not 1, actual size="
                  << borrowExecuteResult.presentNumaId.size() << ".";
        return MEM_POOLING_ERROR;
    } else if (borrowExecuteResult.borrowIds.size() != BORROW_RESULT_SIZE) {
        LOG_ERROR << "[FaultManager][MemId] MemBorrow execute failed.";
        LOG_DEBUG << "[FaultManager][MemId] BorrowExecute Error, borrowIds size is not 1, actual size="
                  << borrowExecuteResult.borrowIds.size() << ".";
        return MEM_POOLING_ERROR;
    }
    LOG_INFO << "[FaultManager][MemId] MemBorrow execute success!";
    LOG_DEBUG << "[FaultManager][MemId] Param borrowId=" << borrowExecuteResult.borrowIds[0] << ".";

    // 通过RPC发送到从节点执行操作
    std::string importNodeId = borrowInNid;
    uint64_t remoteNumaHuge = borrowExecuteResult.presentNumaId[0];
    MpResult retRpc = FaultMemIdExecute::Instance().SameNidExecuteRpc(importNodeId, remoteNumaHuge, memBorrowIdSize,
                                                                      borrowId, borrowExecuteResult.borrowIds[0]);
    if (retRpc != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Same nid execute rpc same nid failed.";
        return MEM_POOLING_ERROR;
    }
    LOG_INFO << "[FaultManager][MemId] Success: mem borrow, huge-page, fault del.";
    return MEM_POOLING_OK;
}

// 从相同节点，借用内存执行RPC函数，发送到从节点执行，需要传入执行的节点Nid
MpResult FaultMemIdExecute::SameNidExecuteRpc(std::string importNodeId, uint64_t remoteNumaHuge,
                                              uint64_t memBorrowIdSize, std::string borrowId, std::string borrowIdNew)
{
    LOG_DEBUG << "[FaultManager][MemId] Master invoke the agent SameNidExecuteRpc. AgentNodeId=" << importNodeId << ".";
    UbseComEndpoint endpoint_fm_same = {
        .moduleId = MP_MODULE_CODE, .serviceId = message::OPCODE_FM_SAME, .address = importNodeId};
    FMSameNidParam fMSameNidParam = {remoteNumaHuge, memBorrowIdSize, borrowId, borrowIdNew};

    RmrsOutStream builder;
    builder << fMSameNidParam;
    UbseByteBuffer reqData = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    uint32_t ret = 0;
    uint32_t retRpc = UbseRpcSend(endpoint_fm_same, reqData, &ret, MemIdFaultSameNidResHandler);
    delete[] reqData.data;
    reqData.data = nullptr;
    if (retRpc != MEM_POOLING_OK || ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Fault same nid execute failed.";
        return ret;
    }
    LOG_INFO << "[FaultManager][MemId] Fault same nid execute success.";
    return MEM_POOLING_OK;
}

// 具体的执行函数，从相同节点借用内存之后，rpc到内存借入节点分大页，删除故障内存
MpResult FaultMemIdExecute::SameNidExecute(uint64_t remoteNumaHuge, uint64_t memBorrowIdSize, std::string borrowId,
                                           std::string borrowIdNew)
{
    // 修改remoteNumaId大页
    std::vector<uint64_t> remoteNumaIds = {remoteNumaHuge};
    std::vector<uint64_t> borrowSizes = {memBorrowIdSize * KB_TO_BYTES}; // 内存单位从kb转换为b

    LOG_DEBUG << "[FaultManager][MemId] Param remoteNumaIds=" << remoteNumaIds[0] << ".";
    LOG_DEBUG << "[FaultManager][MemId] Param borrowSizes= " << borrowSizes[0] << ".";

    MpResult retAllocate = MpSmapHelper::GetInstance().AllocateHugePages(remoteNumaIds, borrowSizes);
    if (retAllocate != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] AllocateHugePages failed.";
        return MEM_POOLING_ERROR;
    }
    LOG_INFO << "[FaultManager][MemId] AllocateHugePages success.";

    MpResult ret = MemBorrowExecutor::Instance().MemFreeWithOps(borrowId, false, true);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Mem free by executor failed.";
        LOG_DEBUG << "[FaultManager][MemId] Mem free by executor failed, borrowId=" << borrowId << ".";
        return MEM_POOLING_ERROR;
    }

    // 更新标识映射符号
    MpResult retBorrId = BorrowIdRedirection::Instance().Update(borrowId, borrowIdNew);
    if (retBorrId != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Update borrowId redirection failed.";
    }

    LOG_DEBUG << "[FaultManager][MemId] Rack delete resource success, with borrowId=" << borrowId << ".";
    LOG_INFO << "[FaultManager][MemId] AgentNode uccess: huge-page, fault del, update borrowId.";
    return MEM_POOLING_OK;
}

// 分配大页
MpResult FaultMemIdExecute::EchoHugepages(uint64_t remoteNumeId, uint64_t borrowMemSize)
{
    // 修改remoteNumaId大页 入参：远端numaId, 新借用内存大小kb
    std::vector<uint64_t> remoteNumaIdsNotSameNid = {remoteNumeId};
    std::vector<uint64_t> borrowSizesNotSameNid = {borrowMemSize * KB_TO_BYTES}; // 内存单位从kb转换为b

    if (remoteNumaIdsNotSameNid.size() == ECHO_HUGE_PAGE_NUM && borrowSizesNotSameNid.size() == ECHO_HUGE_PAGE_NUM) {
        LOG_DEBUG << "[FaultManager][MemId] Param remoteNumaId=" << remoteNumaIdsNotSameNid[0] << ".";
        LOG_DEBUG << "[FaultManager][MemId] Param borrowSize=" << borrowSizesNotSameNid[0] << " KB.";
    } else {
        LOG_ERROR << "[FaultManager][MemId] Param invalid.";
    }

    MpResult retAllocate =
        MpSmapHelper::GetInstance().AllocateHugePages(remoteNumaIdsNotSameNid, borrowSizesNotSameNid);
    if (retAllocate != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Allocate hugePages not same nid failed.";
        return MEM_POOLING_ERROR;
    }
    LOG_INFO << "[FaultManager][MemId] Allocate HugePages not same nid success.";
    return MEM_POOLING_OK;
}

// 分支二：查询远端nuam虚拟机信息
MpResult FaultMemIdCollect::NotSameNidVmInfoRpc(std::string importNodeId, uint16_t remoteNumaId,
                                                uint64_t memBorrowIdSize, FMVmInfoResult &fMVmInfoResult)
{
    LOG_DEBUG << "[FaultManager][MemId] Master invoke the agent NotSameNidVmInfoRpc. AgentNodeId=" << importNodeId
              << ".";
    LOG_DEBUG << "[FaultManager][MemId] Param, importNodeId=" << importNodeId << ", remoteNumaId=" << remoteNumaId
              << ", memBorrowIdSize=" << memBorrowIdSize << " KB.";

    UbseComEndpoint endpoint_fm_vm = {
        .moduleId = MP_MODULE_CODE, .serviceId = message::OPCODE_FM_VM, .address = importNodeId};
    FMVmInfoParam fMVmInfoParam = {remoteNumaId, memBorrowIdSize};

    RmrsOutStream builder;
    builder << fMVmInfoParam;
    UbseByteBuffer reqData = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    uint32_t retRpc = UbseRpcSend(endpoint_fm_vm, reqData, &fMVmInfoResult, MemIdFaultNotSameNidVmInfoResHandler);
    delete[] reqData.data;
    reqData.data = nullptr;
    if (retRpc != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Failed to query VM information. Rpc send failed.";
        return MEM_POOLING_ERROR;
    }
    if (fMVmInfoResult.totalNeedBorrowMem == 0) {
        LOG_ERROR << "[FaultManager][MemId] Failed to query VM information. The size of memory to borrow is zero.";
        return MEM_POOLING_ERROR;
    }
    LOG_INFO << "[FaultManager][MemId] Fault not same nid VmInfo success.";
    return MEM_POOLING_OK;
}

MpResult FaultMemIdCollect::NotSameNidVmInfo(uint16_t remoteNumaId, uint64_t memBorrowIdSize, std::vector<pid_t> &pids,
                                             uint64_t &totalNeedBorrowMem)
{
    LOG_DEBUG << "[FaultManager][MemId] Entry NotSameNidVmInfo.";
    LOG_DEBUG << "[FaultManager][MemId] Input param, remote NumaId=" << remoteNumaId
              << ", memBorrowIdSize=" << memBorrowIdSize << ".";

    std::vector<VmNumaInfo> allVmNumaInfoInfoList;
    std::map<pid_t, VmNumaInfo> vmNumaInfoMap;
    MpResult ret = GetRemoteNumaVms(remoteNumaId, allVmNumaInfoInfoList, vmNumaInfoMap);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Get VmInfo(Immediately) failed.";
        totalNeedBorrowMem = 0;
        return MEM_POOLING_ERROR;
    }

    // 判断虚拟机占用远端内存是否大于memId对应的内存，选取最接近memId内存的虚拟机，借用到相关的内存
    ret = FaultMemIdModule::Instance().FindClosestVmForMemAlloc(allVmNumaInfoInfoList, memBorrowIdSize, pids,
                                                                totalNeedBorrowMem);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Find closest VM for Mem Alloc failed.";
        totalNeedBorrowMem = 0;
        return MEM_POOLING_ERROR;
    }
    LOG_DEBUG << "[FaultManager][MemId] OutParam, totalNeedBorrowMem=" << totalNeedBorrowMem << " KB.";
    for (size_t i = 0; i < pids.size(); i++) {
        LOG_DEBUG << "[FaultManager][MemId] OutParam, Index=" << i << ", pid=" << pids[i] << ".";
    }
    LOG_DEBUG << "[FaultManager][MemId] Exit NotSameNidVmInfo.";
    return MEM_POOLING_OK;
}

// 分支二：从不同的物理节点借用内存
MpResult FaultMemIdModule::BorrowFromNotSameNid(std::string borrowInNid, const CurNumaInfoMF &curNumaInfoMF,
                                                const mempooling::SrcMemoryBorrowParam &srcParam, bool byNodeFault,
                                                bool isForce)
{
    // 如果借不来相同邻居节点的内存,采用以单个memId归还
    FMVmInfoResult fMVmInfoResult;

    // 分支二：查询远端nuam虚拟机信息
    MpResult retVmInfo = FaultMemIdCollect::Instance().NotSameNidVmInfoRpc(
        borrowInNid, curNumaInfoMF.remoteNumaId, curNumaInfoMF.memBorrowIdSize, fMVmInfoResult);
    if (retVmInfo != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Not same nid VmInfo rpc failed.";
        return MEM_POOLING_ERROR;
    }

    std::vector<pid_t> pids = fMVmInfoResult.pids;
    uint64_t totalNeedBorrowMem = fMVmInfoResult.totalNeedBorrowMem;

    // 重新执行内存借用策略
    std::vector<uint64_t> borrowSizes = {totalNeedBorrowMem};
    MemBorrowStrategyMultiResult borrowStrategyMultiResultNotSameNid;
    borrowStrategyMultiResultNotSameNid.byNodeFault = byNodeFault;
    bool isSameDestNid = true;
    MpResult ret;
    MemBorrowStrategyParam param = {curNumaInfoMF.destPreNid, curNumaInfoMF.destSocketId};
    ret = FaultMemIdStrategy::Instance().ApplyMemBorrowStrategyMultipleUB(
        srcParam, borrowSizes, param, borrowStrategyMultiResultNotSameNid, isSameDestNid);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Apply memBorrow strategy multiple failed.";
        return MEM_POOLING_ERROR;
    }
    uint16_t remoteNumaHuge = 0;
    std::string borrowIdNew;
    MpResult retMem = FaultMemIdModule::NotSameMemBoorNidExecute(srcParam, borrowStrategyMultiResultNotSameNid,
                                                                 borrowIdNew, totalNeedBorrowMem, remoteNumaHuge);
    if (retMem != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Not same nid mem borrow execute failed.";
        return MEM_POOLING_ERROR;
    }

    // 通过RPC发送到从节点执行操作
    std::string importNodeId = borrowInNid;
    NotSameNidExecuteParam executeParam = {importNodeId, remoteNumaHuge, totalNeedBorrowMem, curNumaInfoMF.remoteNumaId,
                                           curNumaInfoMF.borrowId};
    MpResult retRpc = FaultMemIdExecute::Instance().NotSameNidExecuteRpc(executeParam, pids, borrowIdNew);
    if (retRpc != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Not same nid execute rpc failed.";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

MpResult FaultMemIdModule::NotSameMemBoorNidExecute(
    const mempooling::SrcMemoryBorrowParam &srcParam,
    const MemBorrowStrategyMultiResult &borrowStrategyMultiResultNotSameNid, std::string &borrowIdNew,
    uint64_t totalNeedBorrowMem, uint16_t &remoteNumaHuge)
{
    MemBorrowExecuteResult borrowExecuteResultNotSameNid;
    MpResult ret = MempoolBorrowModule::Instance().MemBorrowExecute(
        srcParam, borrowStrategyMultiResultNotSameNid.destParam, borrowExecuteResultNotSameNid);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Mem borrow not same nid failed.";
        return MEM_POOLING_ERROR;
    } else if (borrowExecuteResultNotSameNid.presentNumaId.size() != 1 ||
               borrowExecuteResultNotSameNid.borrowIds.size() != 1) {
        LOG_ERROR << "[FaultManager][MemId] Error, borrow result param error.";
        return MEM_POOLING_ERROR;
    }

    borrowIdNew = borrowExecuteResultNotSameNid.borrowIds[0];
    remoteNumaHuge = borrowExecuteResultNotSameNid.presentNumaId[0];
    return MEM_POOLING_OK;
}

MpResult FaultMemIdExecute::NotSameNidExecuteRpc(NotSameNidExecuteParam &param, std::vector<pid_t> &pids,
                                                 std::string borrowIdNew)
{
    std::string importNodeId = param.importNodeId;          // 内存借入节点
    uint16_t remoteNumaHuge = param.remoteNumaHuge;         // 需要分大页的远端numa
    uint64_t totalNeedBorrowMem = param.totalNeedBorrowMem; // 总共需要借用的内存
    uint16_t remoteNumaId = param.remoteNumaId;             // 虚拟机所在远端numa
    std::string borrowId = param.borrowId;                  // borrowID
    std::vector<uint16_t> remoteNumas = {remoteNumaHuge, remoteNumaId};

    LOG_DEBUG << "[FaultManager][MemId] Master invoke agent NotSameNidExecuteRpc. AgentNode=" << importNodeId << ".";
    UbseComEndpoint endpoint_fm_not = {
        .moduleId = MP_MODULE_CODE, .serviceId = message::OPCODE_FM_NOT, .address = importNodeId};
    FMNotSameNidParam fMNotSameNidParam = {remoteNumas, totalNeedBorrowMem, pids, borrowId, borrowIdNew};

    RmrsOutStream builder;
    builder << fMNotSameNidParam;
    UbseByteBuffer reqData = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    uint32_t ret;
    uint32_t retRpc = UbseRpcSend(endpoint_fm_not, reqData, &ret, MemIdFaultNotSameNidResHandler);
    delete[] reqData.data;
    reqData.data = nullptr;
    if (retRpc != MEM_POOLING_OK || ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Fault not same nid execute failed.";
        return ret;
    }
    LOG_INFO << "[FaultManager][MemId] Fault not same nid execute success.";
    return MEM_POOLING_OK;
}

MpResult FaultMemIdExecute::NotSameNidExecute(std::vector<uint16_t> remoteNumas, uint64_t totalNeedBorrowMem,
                                              std::vector<pid_t> &pids, std::string borrowId, bool isForce)
{
    LOG_DEBUG << "[FaultManager][MemId] Entry NotSameNidExecute.";
    if (remoteNumas.size() != VECTOR_REMOTE_NUMA_SIZE) {
        LOG_ERROR << "[FaultManager][MemId] Input remote numa param invaild, with size=" << remoteNumas.size() << ".";
        return MEM_POOLING_ERROR;
    }
    uint16_t remoteNumaHuge = remoteNumas[0];
    uint16_t remoteNumaId = remoteNumas[1];
    // 修改remoteNumaId大页
    MpResult resEcho = EchoHugepages(remoteNumaHuge, totalNeedBorrowMem);
    if (resEcho != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Echo Hugepages failed.";
        return MEM_POOLING_ERROR;
    }

    // 关闭pid远端冷热流动，pid跨远端numa迁移, pid远端冷热流动开启
    MpResult resRemote = VmsMigrateOtherRemoteNuma(pids, remoteNumaId, remoteNumaHuge, borrowId, isForce);
    if (resRemote != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] VM migrate to other remote NUMA(not same nid) failed.";
        return MEM_POOLING_ERROR;
    }
    LOG_DEBUG << "[FaultManager][MemId] Exit NotSameNidExecute.";
    return MEM_POOLING_OK;
}

// pid级别迁移到其他远端numa
MpResult FaultMemIdExecute::VmsMigrateOtherRemoteNuma(std::vector<pid_t> &pids, uint16_t remoteNumaId,
                                                      uint16_t remoteNumaHuge, std::string borrowId, bool isForce)
{
    if (pids.size() != 0) {
        // 关闭pid级别冷热迁移
        int retSmap = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), SMAP_MIGRATE_DISABLE,
                                                                   SMAP_MIGRATE_FLAGS);
        if (retSmap != SMAP_OK) {
            LOG_ERROR << "[FaultManager][MemId] Smap Enable Process(disable) failed, ErrorCode=" << retSmap << ".";
            return MEM_POOLING_ERROR;
        }
    }

    if (pids.size() != 0) {
        // pid级别远端迁移
        MpResult retRemote =
            MpSmapHelper::SmapMigratePidRemoteNumaHelper(pids.data(), pids.size(), remoteNumaId, remoteNumaHuge);
        if (retRemote != MEM_POOLING_OK) {
            LOG_ERROR << "[FaultManager][MemId] Smap migrate pid remote NUMA failed ret=" << retRemote << ".";
            return MEM_POOLING_ERROR;
        }
        LOG_INFO << "[FaultManager][MemId] Smap migrate pid remote NUMA(not same nid) success.";
        for (size_t i = 0; i < pids.size(); i++) {
            LOG_DEBUG << "[FaultManager][MemId] Index=" << i << ", pid=" << pids[i] << ".";
        }
        LOG_DEBUG << "[FaultManager][MemId] Param srcNid=" << remoteNumaId << ", destNid=" << remoteNumaHuge << ".";
    }

    if (pids.size() != 0) {
        // 开启pid级别冷热流动
        int retSmap02 = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), SMAP_MIGRATE_ENABLE,
                                                                     SMAP_MIGRATE_FLAGS);
        if (retSmap02 != 0) {
            LOG_ERROR << "[FaultManager][MemId] Smap Enable process(enable) failed, ErrorCode=" << retSmap02 << ".";
            return MEM_POOLING_ERROR;
        }
    }
    LOG_INFO << "[FaultManager][MemId] VM migrate other remote NUMA success.";
    return MEM_POOLING_OK;
}

MpResult FaultMemIdModule::NotSameNidDeleteUpdate(std::string borrowId, std::string borrowIdNew)
{
    auto ret = MemBorrowExecutor::Instance().MemFreeWithOps(borrowId, false, true);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Rack delete resource(not same nid) failed.";
        return MEM_POOLING_ERROR;
    } else {
        LOG_INFO << "[FaultManager][MemId] Rack delete resource(not same nid) success.";
    }

    // 更新标识映射符号
    MpResult resID = BorrowIdRedirection::Instance().Update(borrowId, borrowIdNew);
    if (resID != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Update borrowId redirection failed.";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

// 查询numa节点的socket
MpResult FaultMemIdModule::GetSocketIdOfNUMA(std::string borrowInNid, SrcMemoryBorrowParam &srcParam,
                                             std::string destPreNid, uint16_t destSocketId)
{
    LOG_DEBUG << "[FaultManager][MemId] Input param, borrowInNid=" << borrowInNid << ", destPreNid=" << destPreNid
              << ", destSocketId=" << destSocketId << ".";

    srcParam.srcNid = borrowInNid; // 内存借入节点物理机
    // 以下是新代码
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    uint32_t retTopo = UbseMemGetTopologyInfo(nodeTopology);
    if (retTopo != 0) {
        LOG_ERROR << "[FaultManager][MemId] Get topo from rack failed!";
        return MEM_POOLING_ERROR;
    }

    // 遍历nodeTopology，找到故障borrowId对于内存借出节点和内存借出socketId同平面的故障节点id的socket
    std::string key = destPreNid + "-" + std::to_string(destSocketId);
    for (const auto &pair : nodeTopology) {
        const std::string &host = pair.first;
        const auto &memNodeList = pair.second;
        if (host != key) {
            continue;
        }
        LOG_DEBUG << "[FaultManager][MemId] Host=" << host << ", Total MemNodeData count=" << memNodeList.size() << ".";
        for (size_t i = 0; i < memNodeList.size(); ++i) {
            const auto &memNode = memNodeList[i];
            LOG_DEBUG << "[FaultManager][MemId] Index=" << i << ", NodeId=" << memNode.nodeId
                      << ", socketId=" << memNode.socket.socketId << ".";
            if (memNode.nodeId != borrowInNid) {
                continue;
            }
            int value = std::stoi(memNode.socket.socketId);
            if (value < INT16_MIN || value > INT16_MAX) {
                LOG_ERROR << "[FaultManager][MemId] Value of nodeId is out of int16_t range.";
                return MEM_POOLING_ERROR;
            }
            srcParam.srcSocketId = static_cast<int16_t>(value);

            if (memNode.socket.numas.size() == 0) {
                LOG_ERROR << "[FaultManager][MemId] Param error, memNode socket numas size is zero.";
                return MEM_POOLING_ERROR;
            }
            int valueNuma = std::stoi(memNode.socket.numas[0].numaId);
            if (valueNuma < INT16_MIN || valueNuma > INT16_MAX) {
                LOG_ERROR << "[FaultManager][MemId] Value of numaId is out of int16_t range.";
                return MEM_POOLING_ERROR;
            }
            srcParam.srcNumaId = static_cast<int16_t>(valueNuma);

            LOG_DEBUG << "[FaultManager][MemId] OUt param srcNumaId=" << srcParam.srcNumaId
                      << ", srcSocketId=" << srcParam.srcSocketId << ".";
            return MEM_POOLING_OK;
        }
    }
    LOG_ERROR << "[FaultManager][MemId] Get SocketId Of NUMA failed.";
    return MEM_POOLING_ERROR;
}

// memId级别故障处理主函数
MpResult FaultMemIdModule::MemIdFaultManage(std::string borrowInNid, uint64_t memId, bool isForce, bool byNodeFault)
{
    BorrowInNodeData borrowInNodeData{borrowInNid, memId};
    uint64_t memBorrowIdSize; // borrowId对应的内存大小 kb
    uint16_t remoteNumaId;    // borrowId在那个远端numa上面
    std::string destPreNid;   // borrowId的借入内存是那个物理机
    uint16_t destSocketId;    // borrowId的借入内存是物理机那个SocketId

    // 查询内存账本，校验输入进来的borrowId是否都是本节点的
    MpResult ret = FaultMemIdCollect::Instance().IsBorrowIdOfCurNid(borrowInNodeData, memBorrowIdSize, remoteNumaId,
                                                                    destPreNid, destSocketId);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Failed to found memid on cur node.";
        return MEM_POOLING_ERROR;
    }
    // 当前节点的memId 对应的borrowId
    std::string borrowId = borrowInNodeData.borrowId;

    // 调用MemBorrowStrategyMultiple，查询是否可以向邻居节点借来相同的borrowId对应的内存
    SrcMemoryBorrowParam srcParam;
    srcParam.uid = borrowInNodeData.uid;
    srcParam.username = borrowInNodeData.username;
    srcParam.srcSocketId = borrowInNodeData.borrowSocketId;
    srcParam.srcNumaId = borrowInNodeData.borrowNumaId;
    std::vector<uint64_t> borrowSizes = {memBorrowIdSize};
    MemBorrowStrategyMultiResult borrowStrategyMultiResult;
    borrowStrategyMultiResult.byNodeFault = byNodeFault;
    bool isSameDestNid = true;

    LOG_DEBUG << "[FaultManager][MemId] Param byNodeFault=" << borrowStrategyMultiResult.byNodeFault << ".";
    MemBorrowStrategyParam param = {destPreNid, destSocketId};
    ret = FaultMemIdStrategy::Instance().ApplyMemBorrowStrategyMultipleUB(srcParam, borrowSizes, param,
                                                                          borrowStrategyMultiResult, isSameDestNid);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] MemBorrow Strategy failed.";
        return MEM_POOLING_ERROR;
    }

    // 判断是否可以从同一个节点借来
    UCEMemoryParam memParam = {memBorrowIdSize, borrowId};
    if (isSameDestNid) {
        // 如果可以借来相同邻居节点的内存
        if (BorrowFromSameNid(borrowInNid, srcParam, borrowStrategyMultiResult, memParam, isForce) != MEM_POOLING_OK) {
            LOG_ERROR << "[FaultManager][MemId] Borrow from same nid failed.";
            return MEM_POOLING_ERROR;
        }
    } else {
        CurNumaInfoMF curNumaInfoMf;
        curNumaInfoMf.SetValues(remoteNumaId, memBorrowIdSize, borrowId, destPreNid, destSocketId);
        if (BorrowFromNotSameNid(borrowInNid, curNumaInfoMf, srcParam, byNodeFault, isForce) != MEM_POOLING_OK) {
            LOG_ERROR << "[FaultManager][MemId] Borrow from not same nid failed.";
            return MEM_POOLING_ERROR;
        }
    }
    return MEM_POOLING_OK;
}

// 1.获取当前远端numa上面的虚拟机
MpResult FaultMemIdCollect::GetRemoteNumaVms(uint16_t remoteNumaId, std::vector<VmNumaInfo> &allVmNumaInfoInfoList,
                                             std::map<pid_t, VmNumaInfo> &vmNumaInfoMap)
{
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    MpResult ret = mempooling::exportV2::Exporter::GetVmInfoImmediately(vmDomainInfos);
    if (MEM_POOLING_OK != ret) {
        LOG_ERROR << "[FaultManager][MemId] Get VmInfo(Immediately) failed.";
        return MEM_POOLING_ERROR;
    }

    if (vmDomainInfos.empty()) {
        LOG_DEBUG << "[FaultManager][MemId] CurRemoteNode vm empty, numaId=" << remoteNumaId << ".";
        return MEM_POOLING_OK;
    }

    for (const auto &vmDomainInfo : vmDomainInfos) {
        VmNumaInfo info;
        info.pid = vmDomainInfo.metaData.pid;
        for (const auto &[numaId, numaInfo] : vmDomainInfo.numaInfo) {
            if (numaInfo.isLocal) {
                // 本地 NUMA
                info.localNumaId = numaInfo.numaId;
                info.localUsedMem = numaInfo.usedMem;
            } else {
                // 远端 NUMA
                info.remoteNumaId = numaInfo.numaId;
                info.remoteUsedMem = numaInfo.usedMem;
            }
        }

        LOG_DEBUG << "[FaultManager][MemId] VmNumaInfo pid=" << info.pid << ", localNumaId=" << info.localNumaId
                  << ", remoteNumaId=" << info.remoteNumaId << ", localUsedMem=" << info.localUsedMem
                  << ", remoteUsedMem=" << info.remoteUsedMem << ".";

        if (info.remoteUsedMem > 0 && info.remoteNumaId == remoteNumaId) {
            allVmNumaInfoInfoList.push_back(info);
            vmNumaInfoMap[info.pid] = info;
            LOG_DEBUG << "[FaultManager][MemId] Add pid=" << info.pid << ", localNumaId=" << info.localNumaId
                      << ", remoteNumaId=" << info.remoteNumaId << ", localUsedMem=" << info.localUsedMem
                      << ", remoteUsedMem=" << info.remoteUsedMem << ".";
        }
    }
    LOG_INFO << "[FaultManager][MemId] Get remote NUMA VM info success.";
    return MEM_POOLING_OK;
}

bool FaultMemIdModule::compareVmNumaInfo(const VmNumaInfo &a, const VmNumaInfo &b)
{
    // 根据虚机占用远端内存大小，从小到大排序
    if (a.remoteUsedMem != b.remoteUsedMem) {
        return a.remoteUsedMem < b.remoteUsedMem;
    }
    return a.pid < b.pid;
}

MpResult FaultMemIdModule::FindClosestVmForMemAlloc(std::vector<VmNumaInfo> &allVmNumaInfoInfoList,
                                                    uint64_t memSizeSingle, std::vector<pid_t> &pids,
                                                    uint64_t &totalNeedBorrowMem)
{
    // 初始化出参
    pids.clear();
    totalNeedBorrowMem = 0;
    // 虚拟机占用远端内存排序
    std::sort(allVmNumaInfoInfoList.begin(), allVmNumaInfoInfoList.end(), compareVmNumaInfo);

    // 累加计算所有虚拟机占用远端内存大小
    uint64_t totalRemoteMem = 0;
    for (VmNumaInfo vmNumaInfo : allVmNumaInfoInfoList) {
        totalRemoteMem += vmNumaInfo.remoteUsedMem;
    }

    // 选择迁出哪些虚拟机，和对应借用的内存
    // 虚拟机占用远端numa内存 >= memId内存
    // 如果虚拟机累计迁出的内存正好等于128M整数倍的时候，需要借用的内存多128M，同时借用之后的内存不应该大于4G
    if (totalRemoteMem > memSizeSingle) {
        LOG_DEBUG << "[FaultManager][MemId] VMtotalRemoteMem=" << totalRemoteMem
                  << " is greater than memSizeSingle=" << memSizeSingle << ".";

        // 找到内存最接近memSizeSingle的虚拟机列表，调整内存大小（如果选取的虚拟机内存最大超过4G会返回错误码）
        MpResult res = ClosestVmVector(allVmNumaInfoInfoList, memSizeSingle, pids, totalNeedBorrowMem);
        if (res != MEM_POOLING_OK) {
            LOG_ERROR << "[FaultManager][MemId] Get closest VM Vector failed.";
            pids.clear();
            totalNeedBorrowMem = 0;
            return MEM_POOLING_ERROR;
        }
    } else if (totalRemoteMem == 0) {
        LOG_DEBUG << "[FaultManager][MemId] VMtotalRemoteMem=" << totalRemoteMem << ".";

        // 只要借用128M内存，而且不需要虚拟机pid级别跨numa迁移
        totalNeedBorrowMem = m_miniUnitMemSize;
    } else {
        // 调整借用内存大小
        totalNeedBorrowMem = totalRemoteMem;
        MpResult res = FaultMemIdStrategy::Instance().AdjustNeedBorrowMem(totalNeedBorrowMem);
        if (res != MEM_POOLING_OK) {
            LOG_ERROR << "[FaultManager][MemId] AdjustNeedBorrowMem failed.";
            pids.clear();
            totalNeedBorrowMem = 0;
            return MEM_POOLING_ERROR;
        }

        for (size_t i = 0; i < allVmNumaInfoInfoList.size(); i++) {
            pids.push_back(allVmNumaInfoInfoList[i].pid);
        }
    }

    // 选取决策借用内存和即将发生UCE故障的内存的最大的那个
    totalNeedBorrowMem = std::max(totalNeedBorrowMem, memSizeSingle);
    LOG_DEBUG << "[FaultManager][MemId] totalNeedBorrowMem=" << totalNeedBorrowMem
              << ", memSizeSingle=" << memSizeSingle << ".";
    return MEM_POOLING_OK;
}

// 判断需要借用的内存量不要擦边界，不擦边界返回true，擦边界返回false
bool FaultMemIdStrategy::IsMemBorrowNotWipeTheEdge(const std::vector<pid_t> &pids,
                                                   const std::vector<VmNumaInfo> &allVmNumaInfoInfoList,
                                                   uint64_t &totalNeedBorrowMem)
{
    // 如果调整后的总借用大小大于m_maxMigrateMem，返回false
    if (totalNeedBorrowMem > m_maxUnitMemSize) {
        LOG_ERROR << "[FaultManager][MemId] Borrow size exceeds 4GB per-request limit, with borrowSize="
                  << totalNeedBorrowMem << " KB.";
        return false;
    }
    LOG_DEBUG << "[FaultManager][MemId] Adjust totalNeedBorrowMem=" << totalNeedBorrowMem << ".";
    return true;
}

MpResult FaultMemIdStrategy::ApplyMemBorrowStrategyMultipleUB(const mempooling::SrcMemoryBorrowParam &srcParam,
                                                              const std::vector<uint64_t> &borrowSizes,
                                                              const MemBorrowStrategyParam &memBorrowStrategyParam,
                                                              MemBorrowStrategyMultiResult &borrowStrategyMultiResult,
                                                              bool &isSameDestNid)
{
    std::string destPreNid = memBorrowStrategyParam.destPreNid;
    uint16_t destSocketId = memBorrowStrategyParam.destSocketId;
    if (borrowSizes.size() != BORROW_RESULT_SIZE) {
        LOG_ERROR << "[FaultManager][MemId] The borrowSizes(should be 1) is error, size is " << borrowSizes.size()
                  << ".";
        return MEM_POOLING_ERROR;
    }
    LOG_DEBUG << "[FaultManager][MemId] Param srcNid=" << srcParam.srcNid << ", srcSocketId=" << srcParam.srcSocketId
              << ", srcNumaId=" << srcParam.srcNumaId << ", borrowSizes=" << borrowSizes[0]
              << ", destPreNid=" << destPreNid << ", destSocketId=" << destSocketId << ".";

    MpResult ret = MempoolBorrowModule::Instance().MemBorrowStrategyMultipleUB(srcParam, borrowSizes, destPreNid,
                                                                               destSocketId, borrowStrategyMultiResult);

    if (borrowStrategyMultiResult.destParam.size() != BORROW_RESULT_SIZE) {
        LOG_ERROR << "[FaultManager][MemId] The result of borrow strategy is error, size is "
                  << borrowStrategyMultiResult.destParam.size() << ".";
        PrintBorrowStrategyMultiResult(borrowStrategyMultiResult, isSameDestNid);
        return MEM_POOLING_ERROR;
    } else if (borrowStrategyMultiResult.destParam[0].destNid != destPreNid) {
        LOG_DEBUG << "[FaultManager][MemId] Not same nid, nid of result="
                  << borrowStrategyMultiResult.destParam[0].destNid << " is not equal to destPreNid=" << destPreNid
                  << ".";
        isSameDestNid = false;
    } else if (borrowStrategyMultiResult.destParam[0].destSocketId != destSocketId) {
        LOG_DEBUG << "[FaultManager][MemId] Not same socketId, socketId of result="
                  << borrowStrategyMultiResult.destParam[0].destSocketId
                  << " is not equal to destSocketId=" << destSocketId << ".";
        isSameDestNid = false;
    }

    PrintBorrowStrategyMultiResult(borrowStrategyMultiResult, isSameDestNid);

    return MEM_POOLING_OK;
}

void FaultMemIdStrategy::PrintBorrowStrategyMultiResult(const MemBorrowStrategyMultiResult &borrowStrategyMultiResult,
                                                        const bool &isSameDestNid)
{
    LOG_DEBUG << "[FaultManager][MemId] Result destParam size=" << borrowStrategyMultiResult.destParam.size() << ".";
    for (size_t i = 0; i < borrowStrategyMultiResult.destParam.size(); i++) {
        if (borrowStrategyMultiResult.destParam[i].destNumaId.size() == 0 ||
            borrowStrategyMultiResult.destParam[i].memSize.size() == 0) {
            LOG_ERROR << "[FaultManager][MemId] borrowStrategyMultiResult.destParam is empty.";
            return;
        }

        LOG_DEBUG << "[FaultManager][MemId] Index=" << i << ", borrowSizes=" << borrowStrategyMultiResult.borrowSizes[i]
                  << ", destNid=" << borrowStrategyMultiResult.destParam[i].destNid
                  << ", destSocketId=" << borrowStrategyMultiResult.destParam[i].destSocketId
                  << ", destNumaNum=" << borrowStrategyMultiResult.destParam[i].destNumaNum
                  << ", destNumaId[0]=" << (borrowStrategyMultiResult.destParam[i].destNumaId[0])
                  << ", memSize[0]=" << (borrowStrategyMultiResult.destParam[i].memSize[0]) << ", isSameDestNid="
                  << isSameDestNid << ".";
    }
}

// 选取最接近且大于借用内存的memSize
MpResult FaultMemIdStrategy::AdjustNeedBorrowMem(uint64_t &needBorrowMem)
{
    if (needBorrowMem == 0) {
        needBorrowMem = m_miniUnitMemSize;
        return MEM_POOLING_OK;
    }

    // 计算最接近 needBorrowMem 的 m_miniUnitMemSize 的倍数
    uint64_t adjustedMem = ((needBorrowMem + m_miniUnitMemSize - 1) / m_miniUnitMemSize) * m_miniUnitMemSize;
    needBorrowMem = adjustedMem;

    LOG_DEBUG << "[FaultManager][MemId] Result needBorrowMem=" << needBorrowMem << ".";
    return MEM_POOLING_OK;
}

// 辅助函数：回溯搜索组合
void FaultMemIdModule::findCombination(const std::vector<VmNumaInfo> &allVmNumaInfoInfoList, uint64_t memSizeSingle,
                                       int start, CombinationState &state)
{
    // 当前和大于等于目标时，计算差值并更新最优解
    if (state.currentSum >= memSizeSingle) {
        uint64_t diff = state.currentSum - memSizeSingle;
        if (diff < state.minDiff || (diff == state.minDiff && state.currentCombination.size() < state.result.size())) {
            state.minDiff = diff;
            state.result = state.currentCombination;
        }
        return;
    }

    // 遍历数组，递归进行组合搜索
    for (size_t i = static_cast<size_t>(start); i < allVmNumaInfoInfoList.size(); ++i) {
        state.currentCombination.push_back(allVmNumaInfoInfoList[i].pid);
        state.currentSum += allVmNumaInfoInfoList[i].remoteUsedMem;
        findCombination(allVmNumaInfoInfoList, memSizeSingle, i + 1, state);
        state.currentCombination.pop_back();
        state.currentSum -= allVmNumaInfoInfoList[i].remoteUsedMem;
    }
}

// 选取虚拟机远端占用内存累加最接近memId内存大小的
MpResult FaultMemIdModule::ClosestVmVector(const std::vector<VmNumaInfo> &allVmNumaInfoInfoList, uint64_t memSizeSingle,
                                           std::vector<pid_t> &pids, uint64_t &totalNeedBorrowMem)
{
    CombinationState state;
    findCombination(allVmNumaInfoInfoList, memSizeSingle, 0, state);

    pids = state.result;

    for (pid_t pid : pids) {
        LOG_DEBUG << "[FaultManager][MemId] Add pid=" << pid << ".";
    }

    for (pid_t pid : pids) {
        for (VmNumaInfo vmNumaInfo : allVmNumaInfoInfoList) {
            if (vmNumaInfo.pid == pid) {
                totalNeedBorrowMem += vmNumaInfo.remoteUsedMem;
                break;
            }
        }
    }

    LOG_DEBUG << "[FaultManager][MemId] Original totalNeedBorrowMem=" << totalNeedBorrowMem << ".";

    // 调整借用内存大小
    MpResult res = FaultMemIdStrategy::Instance().AdjustNeedBorrowMem(totalNeedBorrowMem);
    if (res != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] AdjustNeedBorrowMem failed.";
        return MEM_POOLING_ERROR;
    }

    LOG_DEBUG << "[FaultManager][MemId] Later totalNeedBorrowMem=" << totalNeedBorrowMem << ".";

    return MEM_POOLING_OK;
}

MpResult FaultMemIdCollect::IsBorrowIdOfCurNid(BorrowInNodeData &borrowInNodeData, uint64_t &memBorrowIdSize,
                                               uint16_t &remoteNumaId, std::string &destPreNid, uint16_t &destSocketId)
{
    LOG_DEBUG << "[FaultManager][MemId] IsBorrowIdOfCurNid start.";
    // 查询账本信息
    std::string borrowInNid = borrowInNodeData.borrowInNid;
    uint64_t memId = borrowInNodeData.memId;

    std::vector<BorrowRecord> borrowRecords;
    MpResult ret = BorrowRecordHelper::Instance().GetFragMentFaultBorrowRecords(borrowRecords);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Faild to get borrow records.";
    }

    LOG_DEBUG << "[FaultManager][MemId] Size of borrowRecords=" << borrowRecords.size() << ".";
    // 如果borrowId不在物理机节点borrowInNid里面，返回ERROR
    for (size_t i = 0; i < borrowRecords.size(); i++) {
        if (std::find(borrowRecords[i].borrowMemId.begin(), borrowRecords[i].borrowMemId.end(), memId) !=
            borrowRecords[i].borrowMemId.end()) {
            borrowInNodeData.borrowId = borrowRecords[i].name; // 内存描述符
            borrowInNodeData.uid = borrowRecords[i].uid;
            borrowInNodeData.username = borrowRecords[i].username;
            borrowInNodeData.borrowNumaId = borrowRecords[i].borrowLocalNuma;
            borrowInNodeData.borrowSocketId = borrowRecords[i].borrowSocketId;
            memBorrowIdSize = borrowRecords[i].size;          // 内存借用大小
            remoteNumaId = borrowRecords[i].borrowRemoteNuma; // 借用内存呈现的远端numa
            destPreNid = borrowRecords[i].lentNode;           // 内存借出节点
            destSocketId = borrowRecords[i].lentSocketId;
            LOG_DEBUG << "[FaultManager][MemId] Find memId=" << memId << ", in Nid=" << borrowInNid
                      << ", with borrowId=" << borrowInNodeData.borrowId << ", with uid=" << borrowInNodeData.uid
                      << ", with username=" << borrowInNodeData.username << ", with memBorrowIdSize="
                      << memBorrowIdSize << ", remoteNumaId=" << remoteNumaId << ", destPreNid=" << destPreNid
                      << ", destSocketId=" << destSocketId << ".";
            return MEM_POOLING_OK;
        }
    }

    LOG_ERROR << "[FaultManager][MemId] Faild Find memId=" << memId << ", in Nid=" << borrowInNid << ".";
    return MEM_POOLING_ERROR;
}

MpResult FaultMemIdModule::Init()
{
    return MEM_POOLING_OK;
}

// memid级别故障处理：从不同的nid物理机节点借用内存
uint32_t MemIdFaultNotSameNidRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    LOG_DEBUG << "[FaultManager][MemId] MemIdFaultNotSameNidRecvHandler start.";

    FMNotSameNidParam param;
    RmrsInStream builder(req.data, req.len);
    builder >> param;

    if (param.remoteNumas.size() != VECTOR_REMOTE_NUMA_SIZE) {
        LOG_ERROR << "[FaultManager][MemId] Input remote numa param invaild, with size=" << param.remoteNumas.size()
                  << ".";
        return MEM_POOLING_ERROR;
    }

    LOG_DEBUG << "[FaultManager][MemId] Recv remoteNumaHuge=" << param.remoteNumas[0]
              << ", totalNeedBorrowMem=" << param.totalNeedBorrowMem << ", remoteNumaId=" << param.remoteNumas[1]
              << ", borrowId=" << param.borrowId << ".";
    for (unsigned int i = 0; i < param.pids.size(); i++) {
        LOG_DEBUG << "[FaultManager][MemId] Recv index=" << i << ", pids=" << param.pids[i] << ".";
    }

    MpResult res = FaultMemIdExecute::Instance().NotSameNidExecute(param.remoteNumas, param.totalNeedBorrowMem,
                                                                   param.pids, param.borrowId, false);
    if (MEM_POOLING_OK != res) {
        LOG_DEBUG << "[FaultManager][MemId] Recv NotSameNidExecute res=" << res << ".";
    }

    // 不同节点归还内存和更新标识映射符号
    MpResult retDelete = FaultMemIdModule::Instance().NotSameNidDeleteUpdate(param.borrowId, param.borrowIdNew);
    if (retDelete != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Not same nid delete and upadate failed.";
    }
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    if (MEM_POOLING_OK != res || MEM_POOLING_OK != retDelete) {
        resp.len = RESP_LENGTH;
        resp.data = new (std::nothrow) uint8_t[resp.len];
        if (resp.data == nullptr) {
            LOG_ERROR << "[FaultManager][MemId] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(res);
        resp.data[1] = 0;
    } else {
        resp.len = sizeof(uint8_t);
        resp.data = new (std::nothrow) uint8_t[resp.len];
        if (resp.data == nullptr) {
            LOG_ERROR << "[FaultManager][MemId] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(res);
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

void MemIdFaultNotSameNidResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager][MemId] Ctx or respData is null.";
        return;
    }
    uint32_t *result = static_cast<uint32_t *>(ctx);
    if (resCode != MEM_POOLING_OK || respData.len != MEM_POOLING_ERROR) {
        *result = MEM_POOLING_ERROR;
        LOG_ERROR << "[FaultManager][MemId] ResHandler result error.";
        return;
    }
    LOG_INFO << "[FaultManager][MemId] ResHandler result ok.";
    *result = MEM_POOLING_OK;
}

// memid级别故障处理：相同的nid物理机节点借用内存
uint32_t MemIdFaultSameNidRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    LOG_DEBUG << "[FaultManager][MemId] MemIdFaultSameNidRecvHandler start.";

    FMSameNidParam param;
    RmrsInStream builder(req.data, req.len);
    builder >> param;

    LOG_DEBUG << "[FaultManager][MemId] Recv remoteNumaHuge=" << param.remoteNumaHuge
              << " memBorrowIdSize = " << param.memBorrowIdSize << " borrowId=" << param.borrowId
              << " isForce=" << param.borrowIdNew << ".";

    MpResult res = FaultMemIdExecute::Instance().SameNidExecute(param.remoteNumaHuge, param.memBorrowIdSize,
                                                                param.borrowId, param.borrowIdNew);
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    if (MEM_POOLING_OK != res) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager][MemId] Recv SameNidExecute res:" << res << ".";
        resp.len = RESP_LENGTH;
        resp.data = new (std::nothrow) uint8_t[resp.len];
        if (resp.data == nullptr) {
            LOG_ERROR << "[FaultManager][MemId] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(res);
        resp.data[1] = 0;
    } else {
        resp.len = sizeof(uint8_t);
        resp.data = new (std::nothrow) uint8_t[resp.len];
        if (resp.data == nullptr) {
            LOG_ERROR << "[FaultManager][MemId] Failed to allocate memory, size=" << resp.len << ".";
            return MEM_POOLING_ERROR;
        }
        resp.data[0] = static_cast<uint8_t>(res);
    }
    return res;
}

void MemIdFaultSameNidResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager][MemId] Ctx or respData is null.";
        return;
    }
    uint32_t *result = static_cast<uint32_t *>(ctx);
    if (resCode != MEM_POOLING_OK || respData.len != MEM_POOLING_ERROR) {
        *result = MEM_POOLING_ERROR;
        return;
    }
    *result = MEM_POOLING_OK;
}

// memid级别故障处理：不同的nid物理机节点虚拟机信息查询
uint32_t MemIdFaultNotSameNidVmInfoRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    LOG_INFO << "[FaultManager][MemId] MemIdFaultNotSameNidVmInfoRecvHandler start.";

    FMVmInfoParam param;
    RmrsInStream builderIn(req.data, req.len);
    builderIn >> param;
    std::vector<pid_t> pids;
    uint64_t totalNeedBorrowMem = 0;

    LOG_DEBUG << "[FaultManager][MemId] Recv remoteNumaId=" << param.remoteNumaId
              << ", memBorrowIdSize=" << param.memBorrowIdSize << ".";

    MpResult res = FaultMemIdCollect::Instance().NotSameNidVmInfo(param.remoteNumaId, param.memBorrowIdSize, pids,
                                                                  totalNeedBorrowMem);
    FMVmInfoResult result = {pids, totalNeedBorrowMem};
    RmrsOutStream builder;
    builder << result;
    resp.len = builder.GetSize();
    resp.data = builder.GetBufferPointer();
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    if (MEM_POOLING_OK != res) {
        LOG_ERROR << "[FaultManager][MemId] Recv MigrateStrategy res=" << res << ".";
    }
    return res;
}

void MemIdFaultNotSameNidVmInfoResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager][MemId] Ctx or respData is null.";
        return;
    }
    FMVmInfoResult result;
    FMVmInfoResult *fMVmInfoResult = static_cast<FMVmInfoResult *>(ctx);
    if (resCode != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][MemId] Send error, with ret=" << resCode << ".";
    } else {
        RmrsInStream builder(respData.data, respData.len);
        builder >> result;
    }
    *fMVmInfoResult = result;
}

} // namespace mempooling