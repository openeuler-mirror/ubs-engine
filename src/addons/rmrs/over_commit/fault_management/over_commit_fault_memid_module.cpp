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

#include "over_commit_fault_memid_module.h"
#include <cstdio>
#include "collect_util.h"
#include "exporter.h"
#include "mem_borrow_executor.h"
#include "mp_json_util.h"
#include "mp_smap_helper.h"
#include "mp_string_util.h"
#include "over_commit_def.h"
#include "over_commit_fault_management_handler.h"
#include "over_commit_storage.h"
#include "over_commit_ucache_strategy.h"
#include "rmrs_resource_query.h"
#include "set_smap_remote_numa_info_send.h"
#include "smap_query_process_send.h"

namespace mempooling {

constexpr auto DELETERE_KIND = "MEM";
constexpr auto HIGH_WATER = 92;
constexpr auto LOW_WATER = 80;
constexpr auto HUGE_PAGE_SIZE = 2048;
constexpr auto GB_TO_BYTE = 1024 * 1024 * 1024;

constexpr auto NUM_TWO = 2;
constexpr auto TAG = "[OverCommit][FaultManagement][MemIdFault] ";

// 内存偏移调整相关常量定义
static constexpr uint64_t BIAS_LIMIT_MAX = 4 * 1024 * 1024;          // 偏移上限，超过则失败（KB）
static constexpr uint64_t BIAS_THRESHOLD_MID = 2 * 1024 * 1024;      // 中等偏移阈值 (KB)
static constexpr uint64_t BIAS_THRESHOLD_SMALL = 1 * 1024 * 1024;    // 小偏移阈值 (KB)

static constexpr uint64_t ADJUST_SIZE_LARGE = 4;       // 大偏移调整大小 (GB)
static constexpr uint64_t ADJUST_SIZE_MID = 2;         // 中等偏移调整大小 (GB)
static constexpr uint64_t ADJUST_SIZE_SMALL = 1;       // 小偏移调整大小 (GB)
static constexpr uint64_t ADJUST_SIZE_NONE = 0;        // 无需调整

using namespace mempooling::smap;
using namespace ubse::com;
using namespace ubse::mem::controller;

const std::string SUB_MODULE_NAME = "[OverCommit][FaultManagement][MemIdFault] ";
#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME

MpResult OverCommitFaultMemIdModule::GetOverCommitScene(const std::string &nodeId)
{
    // 容器还是虚机
    mSceneType = MpConfiguration::GetInstance().GetSceneType();
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG << " SceneType= " << static_cast<uint32_t>(mSceneType) << ".";
    // 单NUMA 和 多NUMA
    if (mSceneType == MpSceneType::CONTAINER_SCENE) {
        auto ret = OverCommitStorage::Instance().GetNumaBindType(nodeId, mBindType);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "GetNumaBindType failed.";
            return MEM_POOLING_ERROR;
        }
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "BindType= " << static_cast<uint32_t>(mBindType) << ".";
    } else {
        mBindType = NumaBindType::BIND_SINGLE;
    }
    return MEM_POOLING_OK;
}

MpResult GetContainerNumaInfoList(outinterface::SrcMemoryBorrowParam oParam,
                                  std::vector<VmNumaInfoWithSocket> &vmNumaInfoWithSocketList, uint16_t remoteNumaId,
                                  NumaBindType bindType)
{
    std::unordered_map<std::uint16_t, std::vector<pid_t>> pidMap;
    std::vector<uint32_t> numaIds{remoteNumaId};
    auto ret = CollectUtil::GetRemoteVmPids(oParam.srcNid, numaIds, pidMap);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "GetRemoteVmPids failed, ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    std::vector<RmrsPidInfo> pidInfos;
    // 函数GetRemoteVmPids可以保证pidMap中一定包含remoteNumaId
    ret = ResourceQuery::HelpGetContainerPidNumaInfo(oParam.srcNid, pidMap[remoteNumaId], pidInfos);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "HelpGetContainerPidNumaInfo failed, ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    VmNumaInfoWithSocket numaInfoWithSocket;
    for (const auto &info : pidInfos) {
        if (bindType == NumaBindType::BIND_SINGLE) {
            if (info.localNumaIds.empty()) {
                UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                    << TAG << "info.localNumaIds is empty, pid=" << info.pid << ".";
                continue;
            }
            // 这里适配采集接口的排序，取第一个numaId（主要的绑定numaId）
            numaInfoWithSocket.localNumaId = info.localNumaIds[0];
            // 获取第一个numaId对应的socketId，若获取不到则为-1
            auto it = std::find_if(info.metaNumaInfos.begin(), info.metaNumaInfos.end(),
                                   [&](const MetaNumaInfo &m) { return m.numaId == numaInfoWithSocket.localNumaId; });

            numaInfoWithSocket.socketId = (it != info.metaNumaInfos.end()) ? it->socketId : -1;
        } else {
            //  在容器场景，绑定多numa状态下，为了算法决策计算，将所有的pid都当做在numa0上
            numaInfoWithSocket.localNumaId = 0;
            // 在容器场景，绑定多numa状态下，这里的值用不到
            numaInfoWithSocket.socketId = 0;
        }
        numaInfoWithSocket.pid = info.pid;
        numaInfoWithSocket.remoteNumaId = remoteNumaId;
        numaInfoWithSocket.remoteUsedMem = info.totalRemoteUsedMem >> BYTE2KB;
        numaInfoWithSocket.localUsedMem = info.totalLocalUsedMem >> BYTE2KB;
        //  在容器场景，绑定多numa状态下，这里的值用不到
        numaInfoWithSocket.localFreeMem = 0;
        (void)vmNumaInfoWithSocketList.emplace_back(numaInfoWithSocket);
    }
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultMemIdModule::GetPidNumaInfo(outinterface::SrcMemoryBorrowParam oParam,
                                                    std::vector<VmNumaInfoWithSocket> &vmNumaInfoWithSocketList,
                                                    uint16_t remoteNumaId)
{
    MpResult ret = MEM_POOLING_ERROR;
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "GetPidNumaInfo start.";
    switch (mSceneType) {
        case MpSceneType::VIRTUAL_SCENE:
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "VIRTUAL_SCENE.";
            ret = GetVmNumaInfoMapRpc(oParam.srcNid, vmNumaInfoWithSocketList, remoteNumaId);
            if (ret != MEM_POOLING_OK) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << TAG << "GetVmNumaInfoMapRpc failed, ret=" << ret << ".";
                return MEM_POOLING_ERROR;
            }
            break;
        case MpSceneType::CONTAINER_SCENE:
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "CONTAINER_SCENE.";
            ret = GetContainerNumaInfoList(oParam, vmNumaInfoWithSocketList, remoteNumaId, mBindType);
            if (ret != MEM_POOLING_OK) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << TAG << "MakeNumaInfoList failed, ret=" << ret << ".";
                return MEM_POOLING_ERROR;
            }
            break;
        default:
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Invalid bind type.";
            return MEM_POOLING_ERROR;
    }

    // 判断远端numa上是否有内存被使用,如果没有则将vmNumaInfoWithSocketList清空
    bool hasRemoteMem = std::any_of(vmNumaInfoWithSocketList.begin(), vmNumaInfoWithSocketList.end(),
                                    [](const VmNumaInfoWithSocket &info) { return info.remoteUsedMem > 0; });
    if (!hasRemoteMem) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "No remote numa memory used, clear vmNumaInfoWithSocketList.";
        vmNumaInfoWithSocketList.clear();
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "GetPidNumaInfo success.";
    return MEM_POOLING_OK;
}

uint64_t GetLocalNumaOnRemoteNumaBorrowSize(const std::string &localNodeId, uint16_t localNumaId, uint16_t remoteNumaId,
                                            NumaBindType bindType)
{
    // 3.1. 创建账本
    std::vector<BorrowRecord> borrowRecords;
    auto ret = BorrowRecordHelper::Instance().CollectBorrowRecordsWithFault(localNodeId, borrowRecords);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "Query borrow record failed. localNodeId=" << localNodeId << ", localNumaId=" << localNumaId
            << "remoteNumaId=" << remoteNumaId << ".";
        return MEM_POOLING_ERROR;
    }
    uint64_t borrowSize{0};
    for (const auto &[name, size, lentNode, lentMemId, lentSocketId, lentNuma, borrowNode, borrowLocalNuma,
                      borrowRemoteNuma, borrowMemId, uid, username, borrowSocketId] : borrowRecords) {
        if (bindType == NumaBindType::BIND_SINGLE) {
            if ((borrowLocalNuma == localNumaId) && (borrowRemoteNuma == remoteNumaId)) {
                borrowSize += size;
            }
        } else {
            // 多numa时，计算整个节点的在远端NUMA上的使用情况
            if (borrowRemoteNuma == remoteNumaId) {
                borrowSize += size;
            }
        }
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG << "Get"
        << "remoteNumaTotalSize=" << borrowSize << "KB, localNumaId=" << localNumaId
        << ", remoteNumaId=" << remoteNumaId << ".";
    return borrowSize;
}

MpResult GetLocalBorrowNumaIdOfMemId(const std::string &localNodeId, int16_t &localNumaId, uint16_t memId)
{
    std::vector<BorrowRecord> borrowRecords;
    auto ret = BorrowRecordHelper::Instance().CollectBorrowRecordsWithFault(localNodeId, borrowRecords);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "Query borrow record failed. localNodeId=" << localNodeId << ", memId=" << memId << ".";
        return MEM_POOLING_ERROR;
    }
    for (const auto &[name, size, lentNode, lentMemId, lentSocketId, lentNuma, borrowNode, borrowLocalNuma,
                      borrowRemoteNuma, borrowMemId, uid, username, borrowSocketId] : borrowRecords) {
        auto it = std::find(borrowMemId.begin(), borrowMemId.end(), memId);
        if (it != borrowMemId.end()) {
            localNumaId = borrowLocalNuma;
        }
    }
    return MEM_POOLING_OK;
}

MpResult MemBorrowExecute(SrcMemoryBorrowParam srcParam, uint64_t borrowSize, WaterMark water,
                          MemBorrowExecuteResult &borrowExecuteResult)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Start borrow mem.";
    NumaBindType bindType;
    MpResult ret = OverCommitStorage::Instance().GetNumaBindType(srcParam.srcNid, bindType);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }
    uint16_t socketId = -1;
    if (bindType == NumaBindType::BIND_SINGLE) {
        MpResult retCode = MemManager::Instance().GetSocketId(srcParam.srcNid, srcParam.srcNumaId, socketId);
        if (retCode != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "GetSocketId failed. ret=" << retCode << ".";
            return retCode;
        } else {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << TAG << "GetSocketId success. socketId=" << srcParam.srcSocketId << ".";
        }
    }
    srcParam.srcSocketId = static_cast<int16_t>(socketId);
    std::vector<uint64_t> borrowSizes{borrowSize * KB}; // KBbyte
    MemBorrowExecuteParam param = {.srcParam=srcParam, .borrowSizes=borrowSizes,
        .highWaterMark=water.highWaterMark, .lowWaterMark=water.lowWaterMark};
    LOG_WARN << "Begin rpc to node" << srcParam.srcNid << " to process fault.";
    ubse::com::UbseComEndpoint endpoint_ms = {.moduleId = MP_MODULE_CODE,
                                              .serviceId = message::OPCODE_OVER_COMMIT_FAULT_HANDLE_MEM_BORROW,
                                              .address = srcParam.srcNid};
    rmrs::serialize::RmrsOutStream builder;
    builder << param;
    UbseByteBuffer reqData = {
        .data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = DefaultFreeFunc};

    FaultHandleMemBorrowResult faultHandleMemBorrowResult;
    ret = UbseRpcSend(endpoint_ms, reqData, &faultHandleMemBorrowResult,
                      mempooling::over_commit::OverCommitFaultManagementHandler::FaultHandleMemBorrowResHandler);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "FaultHandleMemBorrow rpc failed. nodeId=" << srcParam.srcNid << ", rpc_ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    if (faultHandleMemBorrowResult.retCode != MEM_POOLING_OK) {
        LOG_ERROR << "FaultHandleMemBorrow execute failed. nodeId=" << srcParam.srcNid << ".";
        return MEM_POOLING_ERROR;
    }

    borrowExecuteResult.borrowIds = faultHandleMemBorrowResult.borrowIds;
    borrowExecuteResult.presentNumaId = faultHandleMemBorrowResult.presentNumaId;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG << "MemBorrowExecute success" << borrowExecuteResult.ToString() << ".";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultMemIdModule::GetWaterMark(struct WaterMark &waterMark)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Start to get GetWaterMarkFromK8s ";
    auto ret = OverCommitStorage::Instance().GetWaterMark(waterMark.highWaterMark, waterMark.lowWaterMark);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Get GetWaterMark failed use default.";
        waterMark.highWaterMark = HIGH_WATER;
        waterMark.lowWaterMark = LOW_WATER;
    }

    return MEM_POOLING_OK;
}

void GetBothVmNumaInfoMultiScene(std::vector<VmNumaInfoWithSocket> &allVmNumaInfoOnRemoteNuma,
                                 std::vector<VmNumaInfo> &numaInfoOnBoth)
{
    for (const VmNumaInfoWithSocket &vmNumaInfo : allVmNumaInfoOnRemoteNuma) {
        VmNumaInfo info;
        info.pid = vmNumaInfo.pid;
        info.localNumaId = vmNumaInfo.localNumaId;
        info.remoteNumaId = vmNumaInfo.remoteNumaId;
        info.localUsedMem = vmNumaInfo.localUsedMem;
        info.remoteUsedMem = vmNumaInfo.remoteUsedMem;
        numaInfoOnBoth.push_back(info);
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "Add pid=" << vmNumaInfo.pid << " localNumaId=" << vmNumaInfo.localNumaId
            << " remoteNumaId=" << vmNumaInfo.remoteNumaId << " localUsedMem=" << vmNumaInfo.localUsedMem << "KB"
            << " remoteUsedMem=" << vmNumaInfo.remoteUsedMem << "KB.";
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG << "GetBothVmNumaInfoMultiScene size=" << numaInfoOnBoth.size() << ".";
}

MpResult OverCommitFaultMemIdModule::PrepareParamForBorrowMem(outinterface::SrcMemoryBorrowParam &param, uint16_t memId,
                                                              uint16_t preRemoteNumaId,
                                                              std::vector<VmNumaInfo> &allVmNumaInfoOnBoth,
                                                              mempooling::WaterMark &waterMark)
{
    // 获取场景类型
    if (GetOverCommitScene(param.srcNid) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "GetOverCommitScene failed.";
        return MEM_POOLING_ERROR;
    }
    // 获取memID对应的借入numaId
    if (mBindType == NumaBindType::BIND_MULTIPLE) {
        param.srcNumaId = -1;
        param.srcSocketId = -1;
    } else {
        auto innerRet = GetLocalBorrowNumaIdOfMemId(param.srcNid, param.srcNumaId, memId);
        if (innerRet != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "GetLocalBorrowNumaIdOfMemId failed.";
            return MEM_POOLING_ERROR;
        }
    }

    // 获取远端numaId上的PID列表
    std::vector<VmNumaInfoWithSocket> allVmNumaInfoSocOnRemoteNuma;
    auto ret = GetPidNumaInfo(param, allVmNumaInfoSocOnRemoteNuma, preRemoteNumaId);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "GetVmNumaInfoMapRpc failed. ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    if (allVmNumaInfoSocOnRemoteNuma.empty()) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "No VM is using memory on remoteNumaId=" << preRemoteNumaId << ", return directly.";
        return MEM_POOLING_OK;
    }
    // uds获取水线
    ret = GetWaterMark(waterMark);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "GetWaterMark failed.";
        return MEM_POOLING_ERROR;
    }
    // 取交集
    if (mBindType == NumaBindType::BIND_SINGLE) {
        GetBothVmNumaInfo(allVmNumaInfoSocOnRemoteNuma, param.srcNumaId, allVmNumaInfoOnBoth, param.srcSocketId);
    } else {
        GetBothVmNumaInfoMultiScene(allVmNumaInfoSocOnRemoteNuma, allVmNumaInfoOnBoth);
    }

    return MEM_POOLING_OK;
}
void ShowPids(const FMVmInfoResult &fMVmInfoResult, const uint64_t faultMemSize)
{
    std::string pidsStr{};
    for (auto pid : fMVmInfoResult.pids) {
        pidsStr += std::to_string(pid) + ",";
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG << "select pids=[" << pidsStr << "] faultMemSize=" << faultMemSize
        << "KB, fMVmInfoResult.totalNeedBorrowMem=" << fMVmInfoResult.totalNeedBorrowMem << "KB.";
}

MpResult GetRemoteNumaSize(uint64_t &remoteNumaTotalSize, GetNumaSizePara param, NumaBindType bindType)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG << "Get remoteTotalSize remoteNumaId=" << param.remoteNumaId << ".";
    remoteNumaTotalSize =
        GetLocalNumaOnRemoteNumaBorrowSize(param.borrowInNid, param.srcNumaId, param.remoteNumaId, bindType);
    if (remoteNumaTotalSize == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Get remoteNumaTotalSize failed.";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

MpResult GetPreRemoteNumaSize(uint64_t &preRemoteTotalSize, GetNumaSizePara param, NumaBindType bindType)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG << "Get preRemoteTotalSize preRemoteNumaId=" << param.preRemoteNumaId << ".";
    preRemoteTotalSize =
        GetLocalNumaOnRemoteNumaBorrowSize(param.borrowInNid, param.srcNumaId, param.preRemoteNumaId, bindType);
    if (preRemoteTotalSize == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Get preRemoteTotalSize failed.";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

MpResult OverCommitFaultMemIdModule::GetSelectPids(FMVmInfoResult &fMVmInfoResult, uint64_t faultMemSize,
                                                   std::vector<VmNumaInfo> &allVmNumaInfoOnBoth)
{
    auto ret = baseInstance.FindClosestVmForMemAlloc(allVmNumaInfoOnBoth, faultMemSize, fMVmInfoResult.pids,
                                                     fMVmInfoResult.totalNeedBorrowMem);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "GetSelectPids failed, FindClosestVmForMemAlloc failed.";
        return MEM_POOLING_ERROR;
    }

    ShowPids(fMVmInfoResult, faultMemSize);
    return MEM_POOLING_OK;
}
MpResult OverCommitFaultMemIdModule::ExecEmpty(outinterface::SrcMemoryBorrowParam outSrcParam, std::string borrowId,
                                               uint16_t preRemoteNumaId, uint64_t faultMemSize)
{
    // UCache 要加一个场景判断，容器要 smapback为false
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "fMVmInfoResult.pids is empty, Return Fault MemId.";
    auto preRemoteNumaSize =
        GetLocalNumaOnRemoteNumaBorrowSize(outSrcParam.srcNid, outSrcParam.srcNumaId, preRemoteNumaId, mBindType);
    if (preRemoteNumaSize == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "GetLocalNumaOnRemoteNumaBorrowSize failed. preRemoteNumaTotalSize=" << preRemoteNumaSize
            << ", localNumaId=" << outSrcParam.srcNumaId << ", preRemoteNumaId:" << preRemoteNumaId << ".";
        return MEM_POOLING_ERROR;
    }
    auto returnRet = ReturnFaultMem(outSrcParam, borrowId, preRemoteNumaId, faultMemSize, preRemoteNumaSize);
    if (returnRet != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "VmInfoResult pids empty,ReturnFaultMem Failed.";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "VmInfoResult pids empty,MemIdFaultManage Success.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultMemIdModule::IsBorrowIdOfCurNidOverCommit(BorrowInNodeData &borrowInNodeData,
                                                                  uint64_t &faultMemSize, uint16_t &preRemoteNumaId,
                                                                  uid_t &uid, std::string &username)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "IsBorrowIdOfCurNidOverCommit start.";
    // 查询账本信息
    std::string borrowInNid = borrowInNodeData.borrowInNid;
    uint64_t memId = borrowInNodeData.memId;
    std::vector<BorrowRecord> borrowRecords;
    MpResult ret = BorrowRecordHelper::Instance().CollectBorrowRecordsWithFault(borrowInNid, borrowRecords);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "CollectBorrowRecords failed.";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG << "Size of borrowRecords=" << borrowRecords.size() << "KB.";
    if (borrowRecords.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "BorrowRecords is empty.";
        return MEM_POOLING_ERROR;
    }
    for (const auto &record : borrowRecords) {
        auto itr = std::find(record.borrowMemId.begin(), record.borrowMemId.end(), memId);
        if (itr != record.borrowMemId.end()) {
            borrowInNodeData.borrowId = record.name;
            faultMemSize = record.size;
            preRemoteNumaId = record.borrowRemoteNuma;
            uid = record.uid;
            username = record.username;
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << TAG << "Find memId=" << memId << " success, borrowId=" << record.name
                << ", faultMemSize=" << faultMemSize << "KB, preRemoteNumaId=" << preRemoteNumaId << ", uid=" << uid
                << ", username= " << username << ".";
            return MEM_POOLING_OK;
        }
    }
    UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG << "Failed find memId=" << memId << "in Nid=" << borrowInNid << ".";
    return MEM_POOLING_ERROR;
}

MpResult OverCommitFaultMemIdModule::MemIdFaultManage(std::string borrowInNid, uint64_t memId)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG << "MemFM Nid=" << borrowInNid << ", memId=" << memId << ".";
    BorrowInNodeData borNodeData = {.borrowInNid = borrowInNid, .memId = memId};

    uint64_t faultSize;       // borrowId对应的内存
    uint16_t preRemoteNumaId; // borrowId在那个远端numa上面
    uid_t uid;
    std::string username;

    // 查询内存账本，校验输入进来的borrowId是否都是本节点的
    if (IsBorrowIdOfCurNidOverCommit(borNodeData, faultSize, preRemoteNumaId, uid, username) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }

    // 获取memID对应的借入numaId
    std::vector<VmNumaInfo> allVmNumaInfoOnBoth;
    outinterface::SrcMemoryBorrowParam oSrcParam = {.srcNid = borrowInNid, .uid = uid, .username = username};
    struct WaterMark waterMark;

    if (PrepareParamForBorrowMem(oSrcParam, memId, preRemoteNumaId, allVmNumaInfoOnBoth, waterMark) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "PrepareParamForBorrowMem Failed.";
        return MEM_POOLING_ERROR;
    }

    if (allVmNumaInfoOnBoth.empty()) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "No VM on remoteNuma, return directly.";
        // 直接归还该borrowId
        auto ret =
            OverCommitFaultMemIdModule::MemFreeDirectlyExecuteRpc(oSrcParam, preRemoteNumaId, borNodeData.borrowId);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << TAG << "No VM on remoteNuma, return " << borNodeData.borrowId << " directly failed.";
        }
        return ret;
    }

    // 调用碎片接口获取虚拟机列表 pids 和 要借用的内存大小
    FMVmInfoResult fMVmInfoResult;
    if (GetSelectPids(fMVmInfoResult, faultSize, allVmNumaInfoOnBoth) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    // 关闭pid级别冷热流动
    if (!fMVmInfoResult.pids.empty() &&
        DisableSmapProcessMigrateRpc(fMVmInfoResult.pids, borrowInNid) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Disable Smap Process Migrate failed.";
        return MEM_POOLING_ERROR;
    }

    // 为虚拟机组合进行内存借用
    MemBorrowExecuteResult borrowExecResult;
    uint64_t remoteNumaSize{0};
    uint64_t preRemoteSize{0};
    // 判断是否已有借用
    if (faultBidBorrowedMap.find(borNodeData.borrowId) != faultBidBorrowedMap.end()) {
        borrowExecResult = faultBidBorrowedMap[borNodeData.borrowId];
    } else {
        SrcMemoryBorrowParam srcParam{borrowInNid, oSrcParam.srcSocketId, oSrcParam.srcNumaId, uid, username};
        if (MemBorrowExecute(srcParam, fMVmInfoResult.totalNeedBorrowMem, waterMark, borrowExecResult) !=
            MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "MemBorrowExecute failed.";
            return MEM_POOLING_ERROR;
        }
        faultBidBorrowedMap[borNodeData.borrowId] = borrowExecResult;
    }
    struct GetNumaSizePara preRemoteNumaPara = {borrowInNid, oSrcParam.srcNumaId, preRemoteNumaId, preRemoteNumaId};

    // 故障节点所借出的远端numa上有内存使用，不能直接将其归还
    auto remoteNumaId = borrowExecResult.presentNumaId[0];
    bool isDiffRemoteNuma{remoteNumaId != preRemoteNumaId};
    struct GetNumaSizePara remoteNumaPara = {borrowInNid, oSrcParam.srcNumaId, remoteNumaId, preRemoteNumaId};
    if (GetRemoteNumaSize(remoteNumaSize, remoteNumaPara, mBindType) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }

    OverCommitFaultMemIdExecuteParam executeParam{
        fMVmInfoResult.pids, oSrcParam.srcNumaId, remoteNumaId,    preRemoteNumaId, fMVmInfoResult.totalNeedBorrowMem,
        remoteNumaSize,      preRemoteSize,       isDiffRemoteNuma};
    // 调用rpc消息到远端
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "ExecuteParam=" << executeParam.ToString() << ".";
    if (MemIdExecuteRpc(executeParam, borrowInNid) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }

    if (GetPreRemoteNumaSize(preRemoteSize, preRemoteNumaPara, mBindType) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    // 调用 ReturnFaultMem
    if (ReturnFaultMem(oSrcParam, borNodeData.borrowId, preRemoteNumaId, faultSize, preRemoteSize) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "ReturnFaultMem Failed.";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "MemIdFaultManage Success.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultMemIdModule::SetAndDeleteResource(std::string borrowId,
                                                          outinterface::SrcMemoryBorrowParam srcParam,
                                                          std::vector<MemBorrowInfo> memBorrowInfos,
                                                          uint64_t faultMemSize)
{
    // 不绑定numa时传入 -1
    if (mBindType == NumaBindType::BIND_MULTIPLE) {
        srcParam.srcNumaId = -1;
    }
    memBorrowInfos[0].borrowSize -= faultMemSize;
    memBorrowInfos[0].borrowSize *= (1 - GetUcacheUsageRatio(srcParam.srcNid));
    auto setSmapRemoteNumaInfoSend = SetSmapRemoteNumaInfoSend(srcParam, memBorrowInfos);
    auto ret = setSmapRemoteNumaInfoSend.SendMsg();
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "SetSmapRemoteNumaInfoSend failed. ret= " << ret << ".";
        return MEM_POOLING_ERROR;
    }
    ret = OverCommitFaultMemIdModule::MemFreeExecuteRpc(borrowId, srcParam.srcNid);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "UbseDeleteResource failed.";
        memBorrowInfos[0].borrowSize += faultMemSize * (1 - GetUcacheUsageRatio(srcParam.srcNid));
        setSmapRemoteNumaInfoSend = SetSmapRemoteNumaInfoSend(srcParam, memBorrowInfos);
        ret = setSmapRemoteNumaInfoSend.SendMsg();
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << TAG << "SetSmapRemoteNumaInfoSend failed. ret=" << ret << ".";
            return MEM_POOLING_ERROR;
        }
        return MEM_POOLING_ERROR;
    }
    if (memBorrowInfos[0].borrowSize == 0) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "FaultNuma remove, nodeId=" << srcParam.srcNid
            << ", numaId=" << memBorrowInfos[0].presentNumaId << ".";
        FaultNuma::Instance().RemoveFaultNuma(srcParam.srcNid, memBorrowInfos[0].presentNumaId);
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "UbseDeleteResource Success. name=" << borrowId
                                                      << ", presentNumaId=" << memBorrowInfos[0].presentNumaId
                                                      << ", borrowSize=" << memBorrowInfos[0].borrowSize << "KB.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultMemIdModule::ReturnFaultMem(outinterface::SrcMemoryBorrowParam outSrcParam,
                                                    std::string borrowId, uint16_t preRemoteNumaId,
                                                    uint64_t faultMemSize, uint64_t remoteNumaSize)
{
    std::vector<MemBorrowInfo> memBorrowInfos = {{.presentNumaId = preRemoteNumaId, .borrowSize = remoteNumaSize}};
    auto ret = SetAndDeleteResource(borrowId, outSrcParam, memBorrowInfos, faultMemSize);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "SetAndDeleteResource failed.";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "ReturnFaultMem Success.";
    return MEM_POOLING_OK;
}

// rpc消息向agent获取node本地的虚拟机信息
MpResult OverCommitFaultMemIdModule::GetVmNumaInfoMapRpc(std::string importNodeId,
                                                         std::vector<VmNumaInfoWithSocket> &vmNumaInfoWithSocketList,
                                                         uint16_t remoteNumaId)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG << "Master to invoke the slave GetVmNumaInfoMapRpc. NodeId=" << importNodeId
        << ", remoteNumaId=" << remoteNumaId << ".";
    UbseComEndpoint endpoint_get_vm_info = {.moduleId = MP_MODULE_CODE,
                                            .serviceId = message::OPCODE_OVER_COMMIT_MEMID_FAULT_GET_VM_INFO,
                                            .address = importNodeId};
    OverCommitFaultVmNumaInfoParam vmNumaInfoParam = {.remoteNumaId = remoteNumaId};
    RmrsOutStream builder;
    builder << vmNumaInfoParam;
    UbseByteBuffer reqData = {
        .data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = [](uint8_t *data) {
            delete[] data;
        }};
    uint32_t ret = UbseRpcSend(endpoint_get_vm_info, reqData, &vmNumaInfoWithSocketList,
                               OverCommitFaultManagementHandler::GetVmNumaInfoMapResHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "UbseRpcSend failed.";
        return MEM_POOLING_ERROR;
    }
    if (vmNumaInfoWithSocketList.size() == 0) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "GetVmNumaInfoMapResHandler success, but vmNumaInfoWithSocketList is empty, return mem directly.";
        return MEM_POOLING_OK;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "GetVmNumaInfoMapResHandler success.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultMemIdModule::MemFreeExecuteRpc(std::string borrowId, std::string importNodeId)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Master to invoke the slave MemFreeExecuteRpc.";
    UbseComEndpoint endpoint_fm_memfree_execute = {.moduleId = MP_MODULE_CODE,
                                                   .serviceId = message::OPCODE_OVER_COMMIT_MEM_ID_FAULT_RETURN_EXECUTE,
                                                   .address = importNodeId};
    RmrsOutStream builder;
    builder << borrowId;
    UbseByteBuffer reqData = {
        .data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = [](uint8_t *data) {
            delete[] data;
        }};
    uint32_t ret = 0;
    UbseRpcSend(endpoint_fm_memfree_execute, reqData, &ret,
                over_commit::OverCommitFaultManagementHandler::MemIdReturnExecuteResHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "MemFreeExecuteRpc failed.";
        return ret;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "MemFreeExecuteRpc success.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultMemIdModule::DisableSmapProcessMigrateRpc(std::vector<pid_t> pids, std::string importNodeId)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Master to invoke the slave DisableSmapProcessMigrate.";
    UbseComEndpoint endpoint_fm_disable_pid = {
        .moduleId = MP_MODULE_CODE, .serviceId = message::OPCODE_SMAP_PROCESS_MIGRATE_DISABLE, .address = importNodeId};
    RmrsOutStream builder;
    builder << pids;
    UbseByteBuffer reqData = {
        .data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = [](uint8_t *data) { delete[] data; }};
    uint32_t ret = 0;
    UbseRpcSend(endpoint_fm_disable_pid, reqData, &ret,
                over_commit::OverCommitFaultManagementHandler::DisableSmapProcessMigrateResHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "DisableSmapProcessMigrate failed, ret = " << ret;
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "DisableSmapProcessMigrate success.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultMemIdModule::MemFreeDirectlyExecuteRpc(outinterface::SrcMemoryBorrowParam oSrcParam,
                                                               uint16_t preRemoteNumaId,
                                                               std::string borrowId)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Master to invoke the slave MemFreeDirectlyExecuteRpc.";
    // 直接归还说明远端numa上没有虚机使用，直接setRemoteNumaInfo为0即可
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "MemFreeDirectly means remoteNuma has no used mem,"
        << "set remoteNuma=" << preRemoteNumaId << "size to zero.";
    std::vector<MemBorrowInfo> memBorrowInfos = {{.presentNumaId = preRemoteNumaId, .borrowSize = 0}};
    auto setSmapRemoteNumaInfoSend = SetSmapRemoteNumaInfoSend(oSrcParam, memBorrowInfos);
    auto ret = setSmapRemoteNumaInfoSend.SendMsg();
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "SetSmapRemoteNumaInfoSend failed. ret= " << ret << ".";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "FaultNuma remove, nodeId=" << oSrcParam.srcNid
            << ", numaId=" << memBorrowInfos[0].presentNumaId << ".";
    FaultNuma::Instance().RemoveFaultNuma(oSrcParam.srcNid, preRemoteNumaId);

    UbseComEndpoint endpoint_fm_memfree_execute = {
        .moduleId = MP_MODULE_CODE,
        .serviceId = message::OPCODE_OVER_COMMIT_MEM_ID_FAULT_DIRECTLY_RETURN_EXECUTE,
        .address = oSrcParam.srcNid};
    RmrsOutStream builder;
    builder << borrowId;
    UbseByteBuffer reqData = {
        .data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = [](uint8_t* data) {
            delete[] data;
        }};
    ret = 0;
    UbseRpcSend(endpoint_fm_memfree_execute, reqData, &ret,
                over_commit::OverCommitFaultManagementHandler::MemIdReturnDirectlyExecuteResHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "MemFreeDirectlyExecuteRpc failed.";
        return ret;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "MemFreeDirectlyExecuteRpc success.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultMemIdModule::MemIdExecuteRpc(OverCommitFaultMemIdExecuteParam param, std::string importNodeId)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Master to invoke the slave MemIdExecuteRpc.";
    UbseComEndpoint endpoint_fm_memid_execute = {.moduleId = MP_MODULE_CODE,
                                                 .serviceId = message::OPCODE_OVER_COMMIT_MEM_ID_FAULT_EXECUTE,
                                                 .address = importNodeId};
    if (mBindType == NumaBindType::BIND_MULTIPLE) {
        param.localNumaId = -1;
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "BindType is BIND_MULTIPLE, change localNumaId.";
    }
    RmrsOutStream builder;
    builder << param;
    UbseByteBuffer reqData = {
        .data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = [](uint8_t *data) {
            delete[] data;
        }};
    uint32_t ret;
    UbseRpcSend(endpoint_fm_memid_execute, reqData, &ret,
                over_commit::OverCommitFaultManagementHandler::MemIdExecuteResHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "MemIdExecuteRpc failed.";
        return ret;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "MemIdExecuteRpc success.";
    return MEM_POOLING_OK;
}

MpResult SetSmapRemoteNumaInfoExec(int16_t localNumaId, uint16_t remoteNumaId, uint64_t borrowSize)
{
    // 多numa时，memBorrowInfoWithSrc中需要填充0
    uint64_t numaId = (localNumaId == -1) ? 0 : localNumaId;
    over_commit::MemBorrowInfoWithSrc memBorrowInfoWithSrc = {
        .srcNumaId = numaId, .presentNumaId = remoteNumaId, .borrowSize = borrowSize};
    std::vector<over_commit::MemBorrowInfoWithSrc> memBorrowInfoWithSrcs = {memBorrowInfoWithSrc};
    MpResult ret = MpSmapHelper::SetSmapRemoteNumaInfo(localNumaId, memBorrowInfoWithSrcs);
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG << "SetSmapRemoteNumaInfo. localNumaId=" << localNumaId << ", presentNumaId=" << remoteNumaId
        << ", borrowSize=" << borrowSize << "KB.";
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Failed to SetSmapRemoteNumaInfo.";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultMemIdModule::AdjustFaultHandleBorrowedMemSize(OverCommitFaultMemIdExecuteParam &param,
                                                                      const uint64_t adjustSize)
{
    // 1. 基础校验
    if (adjustSize == ADJUST_SIZE_NONE) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "No need to adjust borrowed size.";
        return MEM_POOLING_OK;
    }
    if (adjustSize > ADJUST_SIZE_LARGE) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "AdjustSize is too large.";
        return MEM_POOLING_ERROR;
    }

    // 2. 查找借用记录
    BorrowRecord targetRecord;
    auto ret = FindTargetBorrowRecord(MpConfiguration::GetInstance().GetNodeId(), param.remoteNumaId, targetRecord);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "FindTargetBorrowRecord failed.";
        return ret;
    }

    // 3. 执行内存借用
    UbseMemNumaDesc memDesc;
    ret = ExecuteMemBorrow(targetRecord, adjustSize, MpConfiguration::GetInstance().GetNodeId(),
                           param.remoteNumaId, memDesc);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "ExecuteMemBorrow failed.";
        return ret;
    }

    // 4. 更新参数
    param.borrowSize += memDesc.size / KB;
    param.remoteNumaTotalSize += memDesc.size / KB;
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Adjust borrowSize end, add " << memDesc.size << "kb";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultMemIdModule::FindTargetBorrowRecord(const std::string& nodeId, uint32_t remoteNumaId,
                                                            BorrowRecord& outRecord)
{
    std::vector<BorrowRecord> records;
    MpResult ret = BorrowRecordHelper::Instance().CollectBorrowRecordsWithFault(nodeId, records);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Get BorrowRecords failed.";
        return MEM_POOLING_ERROR;
    }

    for (const auto& record : records) {
        if (record.borrowRemoteNuma == remoteNumaId) {
            outRecord = record;
            return MEM_POOLING_OK;
        }
    }

    UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Cannot find target borrow record.";
    return MEM_POOLING_ERROR;
}

MpResult OverCommitFaultMemIdModule::ExecuteMemBorrow(const BorrowRecord& record, uint64_t adjustSize,
                                                      const std::string& nodeId, uint32_t targetNuma,
                                                      UbseMemNumaDesc& outMemDesc)
{
    // 解析槽位ID
    uint32_t slotId = 0;
    if (MpStringUtil::SafeStoul(record.lentNode, slotId) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Parse lentNode to slotId failed.";
        return MEM_POOLING_ERROR;
    }

    // 生成唯一ID失败
    std::string uniqueName;
    if (MemBorrowExecutor::Instance().GenerateUniqueId(nodeId, uniqueName, true) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Generate unique name failed.";
        return MEM_POOLING_ERROR;
    }

    // 初始化参数
    uint8_t usrInfo[ubse::mem::controller::UBSE_MAX_USR_INFO_LEN] = {0};
    UbseMemBorrower borrower{nodeId, -1, record.uid, record.username};
    errno_t copyErr = memcpy_s(usrInfo, sizeof(usrInfo), &record.borrowLocalNuma, sizeof(record.borrowLocalNuma));
    if (copyErr != 0) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Memcpy_s failed, error_code=" << copyErr << ".";
        return MEM_POOLING_ERROR;
    }
    // 执行借用
    for (const auto& lentNuma : record.lentNuma) {
        std::vector<UbseMemNumaLender> lenders{{slotId, record.lentSocketId, lentNuma.numaId, adjustSize * GB_TO_BYTE}};
        UbseResult errCode = UbseMemNumaCreateWithLender(uniqueName, borrower, lenders, usrInfo, outMemDesc);
        if (errCode == UBSE_OK && outMemDesc.numaId == targetNuma) return MEM_POOLING_OK;
    }

    UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Cannot borrow new mem for target numa.";
    return MEM_POOLING_ERROR;
}

uint64_t CalculateRemoteUsedMemSum(const std::vector<pid_t> &pids, const std::vector<VmNumaInfoWithSocket> &infos)
{
    // 1. 快速查找表
    std::unordered_map<pid_t, const VmNumaInfoWithSocket *> pidMap;
    for (const auto &info : infos) {
        pidMap[info.pid] = &info;
    }

    // 2. 取第一个有效 remoteNumaId 作为基准
    std::optional<uint16_t> targetRemoteNumaId;
    uint64_t sum = 0;

    for (pid_t pid : pids) {
        // 找不到直接失败
        auto it = pidMap.find(pid);
        if (it == pidMap.end()) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << TAG << "Pid = " << pid << " mismatch with collected infos.";
            return 0;
        }

        const auto &info = *it->second;
        // 第一次设置基准
        if (!targetRemoteNumaId) {
            targetRemoteNumaId = info.remoteNumaId;
        } else if (info.remoteNumaId != *targetRemoteNumaId) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << TAG << "Pid = " << pid << " remoteNumaId = " << info.remoteNumaId
                << "mismatch with targetRemoteNumaId = " << *targetRemoteNumaId;
            return 0;
        }
        // 累加
        sum += info.remoteUsedMem;
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Pid = " << pid << " add " << info.remoteUsedMem
                                                        << "to numa" << info.remoteNumaId << ", now total = " << sum;
    }

    // 全部校验通过，返回总和
    return sum;
}

/**
 * @brief 判断当前内存是否足够迁移
 *
 * 返回 ERROR：出错，结束流程
 * 返回 OK 且 adjustSize == 0：容器场景 或 虚机场景内存足够迁移
 * 返回 OK 且 adjustSize > 0：虚机场景内存不足，需要调整
 */
MpResult OverCommitFaultMemIdModule::CheckBorrowedMemSizeForPidMigrate(OverCommitFaultMemIdExecuteParam param,
                                                                       uint64_t &adjustSize)
{
    // 初始化输出参数
    adjustSize = ADJUST_SIZE_NONE;

    // 容器场景：不做调整，直接返回OK
    if (MpConfiguration::GetInstance().GetMpSceneType() == MpSceneType::CONTAINER_SCENE) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "CONTAINER_SCENE no adjust.";
        return MEM_POOLING_OK;
    }

    // 非虚机场景：直接返回OK（防御性判断）
    if (MpConfiguration::GetInstance().GetMpSceneType() != MpSceneType::VIRTUAL_SCENE) {
        return MEM_POOLING_OK;
    }

    // ===== 虚机场景：计算远端NUMA实际占用内存 =====
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    MpResult ret = OverCommitFaultMemIdModule::Instance().GetRemoteNumaVms(
        param.preRemoteNumaId, vmNumaInfoWithSocketList);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Failed to GetRemoteNumaVms.";
        return MEM_POOLING_ERROR;
    }

    // 统计远端内存使用总量（单位：KB）
    uint64_t totalRemoteUsedMem = CalculateRemoteUsedMemSum(param.pids, vmNumaInfoWithSocketList);

    // 计算偏移：实际使用 - 借用大小（KB）
    uint64_t bias = (totalRemoteUsedMem > param.borrowSize) ? (totalRemoteUsedMem - param.borrowSize) : 0;

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG << "totalRemoteUsedMem = " << totalRemoteUsedMem
        << ", borrowSize = " << param.borrowSize
        << ", bias = " << bias;

    // ===== 根据偏移量确定调整大小 =====
    if (bias >= BIAS_LIMIT_MAX) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Bias too large, failed to adjust.";
        return MEM_POOLING_ERROR;
    }

    if (bias >= BIAS_THRESHOLD_MID) {
        adjustSize = ADJUST_SIZE_LARGE;
    } else if (bias >= BIAS_THRESHOLD_SMALL) {
        adjustSize = ADJUST_SIZE_MID;
    } else if (bias > 0) {
        adjustSize = ADJUST_SIZE_SMALL;
    } else {
        adjustSize = ADJUST_SIZE_NONE;
    }

    return MEM_POOLING_OK;
}

MpResult OverCommitFaultMemIdModule::MemIdExecute(OverCommitFaultMemIdExecuteParam param)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "MemIdExecute start.";
    // 修改remoteNumaId大页
    bool hugePageAllocated = false;
    std::vector<uint64_t> remoteNumaIds = {static_cast<uint64_t>(param.remoteNumaId)};
    std::vector<uint64_t> borrowSizes = {param.borrowSize * KB}; // kbBYTE
    if (MpConfiguration::GetInstance().GetPageType() == PageType::PAGE_2M) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Param remoteNumaIds[0] = " << remoteNumaIds[0];
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "Param borrowSizes[0] = " << borrowSizes[0] << "Byte";
        MpResult retAllocate = MpSmapHelper::GetInstance().AllocateHugePages(remoteNumaIds, borrowSizes);
        if (retAllocate != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "AllocateHugePages Failed.";
            return MEM_POOLING_ERROR;
        }
        hugePageAllocated = true;
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "AllocateHugePages Success.";
    }

    if (param.isDiffRemoteNuma) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "DiffRemoteNuma.";
        if (param.pids.empty()) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << TAG << "RemoteNuma is not in use yet, MemIdExecute Success.";
            return MEM_POOLING_OK;
        }
        // 迁移虚拟机
        std::string pidsStr{};
        for (auto pid : param.pids) {
            pidsStr += std::to_string(pid) + ",";
        }
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "VmsMigrateOtherRemoteNuma pids= [" << pidsStr << "]"
            << "from numa=" << param.preRemoteNumaId << " to numa=" << param.remoteNumaId << ".";
        auto ret = VmsMigrateOtherRemoteNuma(param.pids, param.preRemoteNumaId, param.remoteNumaId, param.localNumaId,
                                             param.remoteNumaTotalSize);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "VmsMigrate failed.";
            MpSmapHelper::GetInstance().RollBackHugePagesIfNeeded(hugePageAllocated, remoteNumaIds, borrowSizes);
            return MEM_POOLING_ERROR;
        }
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "MemIdExecute Success.";
    return MEM_POOLING_OK;
}

// pid级别迁移到其他远端numa
MpResult OverCommitFaultMemIdModule::VmsMigrateOtherRemoteNuma(std::vector<pid_t> &pids, uint16_t preRemoteNumaId,
                                                               uint16_t remoteNumaId, int16_t localNumaId,
                                                               uint64_t remoteNumaTotalSize)
{
    int enable = 0; //  enable 0 表示禁用
    int flags = 0;  //  flags 0 预留字段无意义

    // 调用远端pid迁移到远端接口
    // 关闭pid级别冷热迁移，pids不会为空
    int retSmap = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), enable, flags);
    if (MEM_POOLING_OK != static_cast<MpResult>(retSmap)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "SmapEnableProcessMigrateHelper faild enable=" << enable << ", retSmap=" << retSmap << ".";
        return MEM_POOLING_ERROR;
    } else {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "SmapEnableProcessMigrateHelper NotSame Nid Sucess. enable=" << enable << ".";
    }

    // pid级别远端迁移
    MpResult retRemote =
        MpSmapHelper::SmapMigratePidRemoteNumaHelper(pids.data(), pids.size(), preRemoteNumaId, remoteNumaId);
    if (MEM_POOLING_OK != retRemote) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "SmapMigratePidRemoteNumaHelper failed ret=" << retRemote << ".";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG << "SmapMigratePidRemoteNumaHelper not same nodeId success.";
    for (std::size_t i = 0; i < pids.size(); i++) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "Migrate Pids, index=" << i << ", pid=" << pids[i] << ".";
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Migrate to presentNumaId=" << remoteNumaId << ".";

    //  新节点 setRemoteNumaInfo
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "SetSmapRemoteNumaInfoExec diff remote numa.";
    MpResult ret = SetSmapRemoteNumaInfoExec(localNumaId, remoteNumaId, remoteNumaTotalSize);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "SetSmapRemoteNumaInfoExec failed new remote numa.";
        return ret;
    }

    // 开启pid级别冷热流动
    enable = 1; //  enable 1 表示启用
    MpResult retSmap02 =
        static_cast<MpResult>(MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), enable, flags));
    if (MEM_POOLING_OK != retSmap02) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "SmapEnableProcessMigrateHelper faild enable=" << enable << ", retSmap=" << retSmap02 << ".";
        return MEM_POOLING_ERROR;
    } else {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG << "SmapEnableProcessMigrateHelper not same nodeId success, enable=" << enable << ".";
    }

    return MEM_POOLING_OK;
}

MpResult OverCommitFaultMemIdModule::GetRemoteNumaVms(uint16_t remoteNumaId,
                                                      std::vector<VmNumaInfoWithSocket> &vmNumaInfoWithSocketList)
{
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    MpResult ret = mempooling::exportV2::Exporter::GetVmInfoImmediately(vmDomainInfos);
    if (MEM_POOLING_OK != ret) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "GetVmInfoImmediately failed.";
        return MEM_POOLING_ERROR;
    }
    if (vmDomainInfos.empty()) {
        LOG_DEBUG << "CurRemoteNode vm empty, numaId=" << remoteNumaId << ".";
        return MEM_POOLING_OK;
    }
    std::vector<mempooling::exportV2::NumaInfo> numaInfos;
    ret = mempooling::exportV2::Exporter::GetNumaInfoImmediately(numaInfos);
    if (MEM_POOLING_OK != ret) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "GetNumaInfoImmediately failed.";
        return MEM_POOLING_ERROR;
    }
    if (numaInfos.empty()) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "numaInfos is empty.";
        return MEM_POOLING_OK;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG << "Enter allVmNumaInfoInfoList.";
    for (const mempooling::exportV2::VmDomainInfo &vmDomainInfo : vmDomainInfos) {
        VmNumaInfoWithSocket info{};
        info.pid = vmDomainInfo.metaData.pid;

        for (const auto &[_, numaInfo] : vmDomainInfo.numaInfo) {
            if (numaInfo.isLocal) {
                // 本地 NUMA
                info.localNumaId = numaInfo.numaId;
                info.localUsedMem = numaInfo.usedMem;
                info.socketId = numaInfo.socketId; // ★ socketId 来自本地 NUMA
            } else {
                // 远端 NUMA
                info.remoteNumaId = numaInfo.numaId;
                info.remoteUsedMem = numaInfo.usedMem;
            }
        }
        auto vmLocalNumaInfo = std::find_if(
            numaInfos.begin(), numaInfos.end(),
            [info](mempooling::exportV2::NumaInfo &numaInfo) { return info.localNumaId == numaInfo.metaData.numaId; });
        if (vmLocalNumaInfo == numaInfos.end()) {
            return MEM_POOLING_ERROR;
        }
        info.localFreeMem = vmLocalNumaInfo->metaData.numaPageInfo[HUGE_PAGE_SIZE].hugePageFree * HUGE_PAGE_SIZE;
        if (info.remoteUsedMem > 0 && info.remoteNumaId == remoteNumaId) {
            vmNumaInfoWithSocketList.push_back(info);
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << TAG << "Add pid=" << info.pid << ", localNumaId=" << info.localNumaId
                << ", remoteNumaId=" << info.remoteNumaId << ", localUsedMem=" << info.localUsedMem
                << "KB, remoteUsedMem=" << info.remoteUsedMem << "KB, socketId=" << info.socketId
                << ", localFreeMem=" << info.localFreeMem << "KB.";
        }
    }
    LOG_DEBUG << "allVmNumaInfoInfoList size()=" << vmNumaInfoWithSocketList.size() << ".";
    return MEM_POOLING_OK;
}

void OverCommitFaultMemIdModule::GetBothVmNumaInfo(std::vector<VmNumaInfoWithSocket> &allVmNumaInfoOnRemoteNuma,
                                                   uint16_t localNumaId, std::vector<VmNumaInfo> &numaInfoOnBoth,
                                                   int16_t &srcSocketId)
{
    for (const VmNumaInfoWithSocket &vmNumaInfo : allVmNumaInfoOnRemoteNuma) {
        if (vmNumaInfo.localNumaId == localNumaId) {
            VmNumaInfo info;
            info.pid = vmNumaInfo.pid;
            info.localNumaId = vmNumaInfo.localNumaId;
            info.remoteNumaId = vmNumaInfo.remoteNumaId;
            info.localUsedMem = vmNumaInfo.localUsedMem;
            info.remoteUsedMem = vmNumaInfo.remoteUsedMem;
            srcSocketId = vmNumaInfo.socketId;
            numaInfoOnBoth.push_back(info);
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << TAG << "Add pid=" << vmNumaInfo.pid << ", localNumaId=" << vmNumaInfo.localNumaId
                << ", remoteNumaId=" << vmNumaInfo.remoteNumaId << ", localUsedMem=" << vmNumaInfo.localUsedMem
                << ", remoteUsedMem=" << vmNumaInfo.remoteUsedMem << "KB.";
        }
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG << "GetBothVmNumaInfo size()=" << numaInfoOnBoth.size() << ".";
}

} // namespace mempooling