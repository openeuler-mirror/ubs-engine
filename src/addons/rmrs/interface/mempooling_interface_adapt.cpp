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

#include "mempooling_interface.h"

#include <bits/types.h>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "ubse_node_controller.h"
#include "LibvirtHelper.h"
#include "exporter.h"
#include "mem_borrow_executor.h"
#include "mem_manager.h"
#include "mempool_borrow_module.h"
#include "mempool_migrate_helper.h"
#include "mempool_migrate_module.h"
#include "mp_smap_controller.h"
#include "mp_smap_helper.h"
#include "mp_vm_quota_util.h"
#include "process_mem_pid_manager_def.h"
#include "rmrs_resource_query.h"

namespace mempooling {
constexpr int SMAP_OK = 0;
constexpr int SMAP_ERROR = 1;
constexpr int MAX_PIDLIST_SIZE = 300;

using std::map;
using std::string;
using std::vector;
using namespace mempooling::smap;

constexpr size_t MIGRATE_PID_MIN_SIZE = 1;                     // 设计规格，迁出执行&策略pid个数范围1~100
constexpr size_t MIGRATE_PID_MAX_SIZE = 100;                   // 设计规格，迁出执行&策略pid个数范围1~100
constexpr uint64_t MIGRATE_MEM_MAX_SIZE = 3221225472;          // 设计规格，迁出策略决策最大内存3T
constexpr uint32_t SMAP_ADD_PROCESS_DURATION_DEFAULT_TIME = 1; // 设计规格，统计周期数组，默认值为1s。
constexpr size_t SMAP_ADD_PROCESS_VEC_MIN_SIZE = 1;            // 设计规格，添加进程扫描数组范围1~100
constexpr size_t SMAP_ADD_PROCESS_VEC_MAX_SIZE = 100;          // 设计规格，添加进程扫描数组范围1~100
constexpr size_t SMAP_REMOVE_PROCESS_VEC_MIN_SIZE = 1;         // 设计规格，去除进程扫描数组范围1~100
constexpr size_t SMAP_REMOVE_PROCESS_VEC_MAX_SIZE = 100;       // 设计规格，去除进程扫描数组范围1~100
constexpr size_t SMAP_ENABLE_PROCESS_VEC_MIN_SIZE = 1;         // 设计规格，去除进程扫描数组范围1~100
constexpr size_t SMAP_ENABLE_PROCESS_VEC_MAX_SIZE = 100;       // 设计规格，去除进程扫描数组范围1~100

std::atomic<int> g_executionInProgress(0); // 标记函数是否在执行
// RAII 类，自动管理 g_executionInProgress 的状态
class MempoolingInterfaceAdapt {
public:
    MempoolingInterfaceAdapt()
    {
        g_executionInProgress.fetch_add(1, std::memory_order_acquire);
    }

    ~MempoolingInterfaceAdapt()
    {
        g_executionInProgress.fetch_sub(1, std::memory_order_release);
    }
};

uint32_t ValidateParamsOfAntiNode(const map<string, vector<string>>& nodeAntiMap)
{
    std::vector<std::string> nodeIds = mempooling::MpConfiguration::GetInstance().GetNodeIds();
    std::unordered_set<std::string> nodeSet;
    nodeSet.reserve(nodeIds.size());
    for (auto& id : nodeIds) {
        nodeSet.insert(id);
    }

    // key 集合
    std::unordered_set<std::string> keySet;
    keySet.reserve(nodeAntiMap.size());

    for (const auto& kv : nodeAntiMap) {
        const std::string& keyNode = kv.first;
        keySet.insert(keyNode);

        if (nodeSet.find(keyNode) == nodeSet.end()) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[AntiAffinity] Invalid key nodeId=" << keyNode << ", not found in physical node list.";
            return MEM_POOLING_ERROR;
        }

        for (const auto& valueNode : kv.second) {
            if (nodeSet.find(valueNode) == nodeSet.end()) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[AntiAffinity] Invalid anti nodeId=" << valueNode << " for key nodeId=" << keyNode
                    << ", not found in physical node list.";
                return MEM_POOLING_ERROR;
            }
        }
    }

    if (keySet.size() != nodeSet.size()) {
        for (const auto& nodeId : nodeSet) {
            if (keySet.find(nodeId) == keySet.end()) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[AntiAffinity] Missing key nodeId=" << nodeId << " in anti-affinity configuration.";
            }
        }
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

uint32_t mempooling::outinterface::UBSRMRSUpdateAntiNode(const map<string, vector<string>>& nodeAntiMap)
{
    uint32_t retParam = ValidateParamsOfAntiNode(nodeAntiMap);
    if (retParam != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][AntiAffinity] Input param invalid.";
        return MEM_POOLING_ERROR;
    }

    uint32_t ret = MEM_POOLING_ERROR;
    // 使用 MempoolingInterfaceAdapt 来管理函数执行状态
    MempoolingInterfaceAdapt guard;

    if (nodeAntiMap.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][AntiAffinity] The anti-affinity list input is empty.";
        return ret;
    }
    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterOtherFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][AntiAffinity] Concurrency is not supported, the current function cannot be entered.";
        return MEM_POOLING_ERROR;
    }
    ret = AntiNode::Instance().Update(nodeAntiMap);
    mgr.ExitOtherFunc();
    return ret;
}

uint32_t mempooling::outinterface::UBSRMRSMemBorrowStrategy(
    const mempooling::outinterface::SrcMemoryBorrowParam& outSrcParam, const uint64_t& borrowSize,
    mempooling::outinterface::MemBorrowStrategyResult& outBorrowStrategyResult)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemBorrow][MemBorrowStrategy] UBSRMRSMemBorrowStrategy start.";
    if (outSrcParam.srcNid != MpConfiguration::GetInstance().GetNodeId()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] Input nodeId param invalid.";
        return MEM_POOLING_ERROR;
    }
    uint32_t ret = MEM_POOLING_ERROR;
    // 使用 MempoolingInterfaceAdapt 来管理函数执行状态
    MempoolingInterfaceAdapt guard;

    mempooling::SrcMemoryBorrowParam srcParam;
    mempooling::MemBorrowStrategyResult borrowStrategyResult;
    // srcParam字段
    srcParam.srcNid = outSrcParam.srcNid;
    srcParam.srcNumaId = outSrcParam.srcNumaId;
    srcParam.srcSocketId = outSrcParam.srcSocketId;

    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterOtherFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] Concurrency is not supported, the current function cannot be entered.";
        return MEM_POOLING_ERROR;
    }
    ret = MempoolBorrowModule::Instance().MemBorrowStrategy(srcParam, borrowSize, borrowStrategyResult);
    mgr.ExitOtherFunc();
    if (ret != MEM_POOLING_OK) {
        return ret;
    }
    // borrowStrategyResult字段
    outBorrowStrategyResult.borrowSize = borrowStrategyResult.borrowSize;
    outBorrowStrategyResult.srcParam.srcNid = borrowStrategyResult.srcParam.srcNid;
    outBorrowStrategyResult.srcParam.srcNumaId = borrowStrategyResult.srcParam.srcNumaId;
    outBorrowStrategyResult.srcParam.srcSocketId = borrowStrategyResult.srcParam.srcSocketId;
    // vector DestMemoryBorrowParam字段
    for (auto destParamItem : borrowStrategyResult.destParam) {
        mempooling::outinterface::DestMemoryBorrowParam outDestParam;
        outDestParam.destNid = destParamItem.destNid;
        outDestParam.destNumaId = destParamItem.destNumaId;
        outDestParam.destNumaNum = destParamItem.destNumaNum;
        outDestParam.destSocketId = destParamItem.destSocketId;
        outDestParam.memSize = destParamItem.memSize;

        outBorrowStrategyResult.destParam.push_back(outDestParam);
    }

    return ret;
}

static uint32_t ValidateBasicParams(const mempooling::outinterface::BatchSrcMemoryBorrowParam& outSrcParam,
                                    const uint64_t& borrowSize)
{
    if (outSrcParam.srcNid != MpConfiguration::GetInstance().GetNodeId()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[BatchBorrow][Validate] Input nodeId param invalid.";
        return MEM_POOLING_ERROR;
    }

    if (outSrcParam.srcNumaNum < 1) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[BatchBorrow][Validate] Input srcNumaNum param invalid, must >= 1.";
        return MEM_POOLING_ERROR;
    }

    if (outSrcParam.srcNumaId.size() != outSrcParam.srcNumaNum) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[BatchBorrow][Validate] Input srcNumaId size mismatch with srcNumaNum.";
        return MEM_POOLING_ERROR;
    }

    if (borrowSize == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[BatchBorrow][Validate] Input borrowSize param invalid, must > 0.";
        return MEM_POOLING_ERROR;
    }

    if (borrowSize > MIGRATE_MEM_MAX_SIZE) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[BatchBorrow][Validate] Input borrowSize exceeds max limit " << MIGRATE_MEM_MAX_SIZE << ".";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

static uint32_t ValidateNumaIdFormat(const std::vector<int16_t>& srcNumaId)
{
    for (size_t i = 0; i < srcNumaId.size(); ++i) {
        if (srcNumaId[i] < 0) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[BatchBorrow][Validate] srcNumaId[" << i << "]=" << srcNumaId[i] << " is invalid, must >= 0.";
            return MEM_POOLING_ERROR;
        }
    }

    std::set<int16_t> numaIdSet;
    for (size_t i = 0; i < srcNumaId.size(); ++i) {
        if (numaIdSet.find(srcNumaId[i]) != numaIdSet.end()) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[BatchBorrow][Validate] Duplicate srcNumaId=" << srcNumaId[i] << " found at index " << i
                << ", NUMA IDs must be unique.";
            return MEM_POOLING_ERROR;
        }
        numaIdSet.insert(srcNumaId[i]);
    }

    return MEM_POOLING_OK;
}

static uint32_t ValidateNumaIdExistence(const std::vector<int16_t>& srcNumaId)
{
    std::vector<mempooling::exportV2::NumaInfo> localNumaInfos;
    if (mempooling::exportV2::Exporter::GetNumaInfoImmediately(localNumaInfos) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[BatchBorrow][Validate] GetNumaInfoImmediately failed, cannot validate NUMA IDs.";
        return MEM_POOLING_ERROR;
    }

    for (size_t i = 0; i < srcNumaId.size(); ++i) {
        bool found = false;
        for (const auto& numaInfo : localNumaInfos) {
            if (numaInfo.metaData.numaId == srcNumaId[i]) {
                found = true;
                break;
            }
        }
        if (!found) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[BatchBorrow][Validate] srcNumaId[" << i << "]=" << srcNumaId[i]
                << " does not exist in local node.";
            return MEM_POOLING_ERROR;
        }
    }

    return MEM_POOLING_OK;
}

uint32_t ValidateParamsOfBatchBorrowStrategy(const mempooling::outinterface::BatchSrcMemoryBorrowParam& outSrcParam,
                                             const uint64_t& borrowSize)
{
    uint32_t ret = ValidateBasicParams(outSrcParam, borrowSize);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }

    ret = ValidateNumaIdFormat(outSrcParam.srcNumaId);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }

    ret = ValidateNumaIdExistence(outSrcParam.srcNumaId);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }

    return MEM_POOLING_OK;
}

static uint32_t GetAlignedBorrowSize(const std::string& srcNid, uint64_t borrowSize, uint64_t& alignedBorrowSize)
{
    constexpr uint64_t MB_TO_KB = 1024ULL;

    const auto allNodeInfo = ubse::nodeController::UbseNodeController::GetInstance().GetAllNodes();
    auto nodeIt = allNodeInfo.find(srcNid);
    if (nodeIt == allNodeInfo.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[BatchBorrow][AlignSize] Cannot find node info for nodeId=" << srcNid;
        return MEM_POOLING_ERROR;
    }

    uint32_t blockSizeMB = nodeIt->second.blockSize;
    if (blockSizeMB == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[BatchBorrow][AlignSize] blockSize=" << blockSizeMB << "MB is invalid, must be non-zero.";
        return MEM_POOLING_ERROR;
    }

    uint64_t blockSizeKB = static_cast<uint64_t>(blockSizeMB) * MB_TO_KB;
    alignedBorrowSize = ((borrowSize + blockSizeKB - 1) / blockSizeKB) * blockSizeKB;

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[BatchBorrow][AlignSize] borrowSize=" << borrowSize << "KB rounded up to " << alignedBorrowSize
        << "KB (blockSize=" << blockSizeMB << "MB).";

    return MEM_POOLING_OK;
}

static void ConvertResultsToOutInterface(
    const std::vector<mempooling::MemBorrowStrategyResult>& borrowStrategyResults,
    const mempooling::outinterface::BatchSrcMemoryBorrowParam& outSrcParam,
    std::vector<mempooling::outinterface::MemBorrowStrategyResult>& outBorrowStrategyResult)
{
    for (const auto& result : borrowStrategyResults) {
        mempooling::outinterface::MemBorrowStrategyResult outResult;
        outResult.borrowSize = result.borrowSize;
        outResult.srcParam.srcNid = result.srcParam.srcNid;
        outResult.srcParam.srcNumaId = result.srcParam.srcNumaId;
        outResult.srcParam.srcSocketId = result.srcParam.srcSocketId;
        outResult.srcParam.uid = outSrcParam.uid;
        outResult.srcParam.username = outSrcParam.username;

        for (const auto& destParam : result.destParam) {
            mempooling::outinterface::DestMemoryBorrowParam outDestParam;
            outDestParam.destNid = destParam.destNid;
            outDestParam.destSocketId = destParam.destSocketId;
            outDestParam.destNumaNum = destParam.destNumaNum;
            outDestParam.destNumaId = destParam.destNumaId;
            outDestParam.memSize = destParam.memSize;
            outResult.destParam.push_back(outDestParam);
        }

        outBorrowStrategyResult.push_back(outResult);
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[BatchBorrow][Convert] Converted " << outBorrowStrategyResult.size() << " results.";

    for (size_t i = 0; i < outBorrowStrategyResult.size(); ++i) {
        const auto& result = outBorrowStrategyResult[i];
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[BatchBorrow][Convert] Result[" << i << "]: "
            << "srcNid=" << result.srcParam.srcNid << ", srcNumaId=" << result.srcParam.srcNumaId
            << ", borrowSize=" << result.borrowSize << "KB"
            << ", destParam.size()=" << result.destParam.size();

        for (size_t j = 0; j < result.destParam.size(); ++j) {
            const auto& dest = result.destParam[j];
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[BatchBorrow][Convert]   Dest[" << j << "]: "
                << "destNid=" << dest.destNid << ", destSocketId=" << dest.destSocketId
                << ", destNumaId=" << (dest.destNumaId.empty() ? -1 : dest.destNumaId[0])
                << ", memSize=" << (dest.memSize.empty() ? 0 : dest.memSize[0]) << "KB";
        }
    }
}

uint32_t mempooling::outinterface::UBSRMRSBatchBorrowStrategy(
    const mempooling::outinterface::BatchSrcMemoryBorrowParam& outSrcParam, const uint64_t& borrowSize,
    std::vector<mempooling::outinterface::MemBorrowStrategyResult>& outBorrowStrategyResult,
    mempooling::outinterface::BorrowStrategy borrowStrategy)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[BatchBorrow][Strategy] UBSRMRSBatchBorrowStrategy start.";

    uint32_t retParam = ValidateParamsOfBatchBorrowStrategy(outSrcParam, borrowSize);
    if (retParam != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[BatchBorrow][Strategy] Input param invalid.";
        return MEM_POOLING_ERROR;
    }

    uint64_t alignedBorrowSize = 0;
    uint32_t ret = GetAlignedBorrowSize(outSrcParam.srcNid, borrowSize, alignedBorrowSize);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }

    MempoolingInterfaceAdapt guard;
    std::vector<mempooling::MemBorrowStrategyResult> borrowStrategyResults;

    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterOtherFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[BatchBorrow][Strategy] Concurrency is not supported.";
        return MEM_POOLING_ERROR;
    }

    ret = migrate::MempoolMigrateModule::BatchBorrowStrategyImpl(
        outSrcParam.srcNid, outSrcParam.srcNumaId, alignedBorrowSize, borrowStrategy, borrowStrategyResults);

    mgr.ExitOtherFunc();

    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[BatchBorrow][Strategy] BatchBorrowStrategyImpl failed, ret=" << ret << ".";
        return ret;
    }

    ConvertResultsToOutInterface(borrowStrategyResults, outSrcParam, outBorrowStrategyResult);
    return MEM_POOLING_OK;
}

uint32_t mempooling::outinterface::UBSRMRSMemBorrowExecute(
    const mempooling::outinterface::SrcMemoryBorrowParam& outSrcParam,
    const vector<mempooling::outinterface::DestMemoryBorrowParam>& outDestParam,
    mempooling::outinterface::MemBorrowExecuteResult& outBorrowExecuteResult)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowExecute] Start to process memory "
                                                        "borrow request.";

    if (outSrcParam.srcNid != MpConfiguration::GetInstance().GetNodeId()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowExecute] SrcNodeId param invalid.";
        return MEM_POOLING_ERROR;
    }
    uint32_t ret = MEM_POOLING_ERROR;
    // 使用 MempoolingInterfaceAdapt 来管理函数执行状态
    MempoolingInterfaceAdapt guard;

    if (outDestParam.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowExecute] Input parameter invalid.";
        return ret;
    }

    mempooling::SrcMemoryBorrowParam srcParam;
    vector<mempooling::DestMemoryBorrowParam> destParam;
    mempooling::MemBorrowExecuteResult borrowExecuteResult;
    // srcParam字段
    srcParam.srcNid = outSrcParam.srcNid;
    srcParam.srcNumaId = outSrcParam.srcNumaId;
    srcParam.srcSocketId = outSrcParam.srcSocketId;
    // vector DestMemoryBorrowParam字段
    for (auto outDestParamItem : outDestParam) {
        mempooling::DestMemoryBorrowParam param;
        param.destNid = outDestParamItem.destNid;
        param.destNumaId = outDestParamItem.destNumaId;
        param.destNumaNum = outDestParamItem.destNumaNum;
        param.destSocketId = outDestParamItem.destSocketId;
        param.memSize = outDestParamItem.memSize;

        destParam.push_back(param);
    }
    bool mustSamePlane = MpConfiguration::GetInstance().GetMustSamePlane();
    bool enableBorrowSplit = MpConfiguration::GetInstance().GetEnableBorrowSplit();
    auto& mgr = ApiConcurrencyManager::getInstance();
    if (mustSamePlane || enableBorrowSplit) {
        if (!mgr.TryEnterOtherFunc()) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowExecute] Concurrency is not "
                                                                 "supported, the current function cannot be entered.";
            return MEM_POOLING_ERROR;
        }
    }
    ret = MempoolBorrowModule::Instance().MemBorrowExecute(srcParam, destParam, borrowExecuteResult);
    mgr.ExitOtherFunc();
    if (ret != MEM_POOLING_OK) {
        return ret;
    }
    // borrowExecuteResult字段
    outBorrowExecuteResult.borrowIds = borrowExecuteResult.borrowIds;
    outBorrowExecuteResult.presentNumaIds = borrowExecuteResult.presentNumaId;

    return ret;
}

uint32_t ValidateParamsOfMigrateStrategy(const string& borrowInNode,
                                         const vector<mempooling::outinterface::VMPresetParam>& outVmInfoList,
                                         const uint64_t& borrowSize)
{
    if (borrowInNode != MpConfiguration::GetInstance().GetNodeId()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][Strategy] Input nodeId param invalid.";
        return MEM_POOLING_ERROR;
    }

    if (outVmInfoList.size() < MIGRATE_PID_MIN_SIZE || outVmInfoList.size() > MIGRATE_PID_MAX_SIZE) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][Strategy] Input VM param invalid.";
        return MEM_POOLING_ERROR;
    }

    if (borrowSize > MIGRATE_MEM_MAX_SIZE) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][Strategy] Input borrow size param invalid.";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

uint32_t mempooling::outinterface::UBSRMRSMigrateStrategy(
    const string& borrowInNode, const vector<mempooling::outinterface::VMPresetParam>& outVmInfoList,
    const uint64_t& borrowSize, mempooling::outinterface::MigrateStrategyResult& outMigrateStrategyResult)
{
    uint32_t retParam = ValidateParamsOfMigrateStrategy(borrowInNode, outVmInfoList, borrowSize);
    if (retParam != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][Strategy] Input param invalid.";
        return MEM_POOLING_ERROR;
    }
    uint32_t ret = MEM_POOLING_ERROR;
    // 使用 MempoolingInterfaceAdapt 来管理函数执行状态
    MempoolingInterfaceAdapt guard;
    mempooling::MigrateStrategyResult migrateStrategyResult;
    vector<turbo::rmrs::VMPresetParam> vmInfoList;
    // vector VMPresetParam字段
    for (auto outVmInfo : outVmInfoList) {
        turbo::rmrs::VMPresetParam vmInfo;
        vmInfo.pid = outVmInfo.pid;
        vmInfo.ratio = outVmInfo.ratio;

        vmInfoList.push_back(vmInfo);
    }

    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterOtherFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][Strategy] Concurrency is not supported, the current function cannot be entered.";
        return MEM_POOLING_ERROR;
    }
    ret = migrate::MempoolMigrateModule::MigrateStrategy(borrowInNode, vmInfoList, borrowSize, migrateStrategyResult);
    mgr.ExitOtherFunc();
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][Strategy] MigrateStrategy failed, error code is " << ret << ".";
        return ret;
    }
    // migrateStrategyResult字段
    outMigrateStrategyResult.waitingTime = migrateStrategyResult.waitingTime;
    // vector migrateStrategyResult vmInfo字段
    for (auto vmInfo : migrateStrategyResult.vmInfoList) {
        mempooling::outinterface::VMMigrateOutParam outVmInfo;
        outVmInfo.pid = vmInfo.pid;
        outVmInfo.memSize = vmInfo.memSize;
        outVmInfo.desNumaId = vmInfo.desNumaId;

        outMigrateStrategyResult.vmInfoList.push_back(outVmInfo);
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][Strategy] MigrateStrategy success.";
    return ret;
}

uint32_t ValidateParamsOfMigrateExecute(const string& borrowInNode,
                                        const vector<mempooling::outinterface::VMMigrateOutParam>& outVmInfoList,
                                        const vector<string>& borrowIdList)
{
    if (borrowInNode != MpConfiguration::GetInstance().GetNodeId()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][OutMigrateExecute] Input nodeId param invalid.";
        return MEM_POOLING_ERROR;
    }

    if (borrowInNode.empty() || outVmInfoList.empty() || borrowIdList.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][OutMigrateExecute] Incoming parameter invalid.";
        return MEM_POOLING_ERROR;
    }

    if (outVmInfoList.size() < MIGRATE_PID_MIN_SIZE || outVmInfoList.size() > MIGRATE_PID_MAX_SIZE) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][OutMigrateExecute] Input VM param invalid.";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

uint32_t mempooling::outinterface::UBSRMRSMigrateExecute(
    const string& borrowInNode, const vector<mempooling::outinterface::VMMigrateOutParam>& outVmInfoList,
    const uint64_t& waitingTime, const vector<string>& borrowIdList)
{
    uint32_t retParam = ValidateParamsOfMigrateExecute(borrowInNode, outVmInfoList, borrowIdList);
    if (retParam != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][OutMigrateExecute] Input param invalid.";
        return MEM_POOLING_ERROR;
    }

    uint32_t ret = MEM_POOLING_ERROR;
    // 使用 MempoolingInterfaceAdapt 来管理函数执行状态
    MempoolingInterfaceAdapt guard;
    vector<mempooling::VMMigrateOutParam> vmInfoList;
    // vector VMMigrateOutParam字段
    for (auto outVmInfo : outVmInfoList) {
        mempooling::VMMigrateOutParam vmInfo;
        vmInfo.pid = outVmInfo.pid;
        vmInfo.memSize = outVmInfo.memSize;
        vmInfo.desNumaId = outVmInfo.desNumaId;

        vmInfoList.push_back(vmInfo);
    }

    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterOtherFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][OutMigrateExecute] Concurrency is not supported, the current function cannot be entered.";
        return MEM_POOLING_ERROR;
    }
    ret = migrate::MempoolMigrateExecute::MigrateExecute(borrowInNode, vmInfoList, waitingTime, borrowIdList);
    mgr.ExitOtherFunc();

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemMigrate][OutMigrateExecute] OutMigrateExecute ret code is " << ret << ".";
    return ret;
}

uint32_t mempooling::outinterface::UBSRMRSMemFree(const string& nodeId)
{
    if (nodeId != MpConfiguration::GetInstance().GetNodeId()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemFree][MemFreeExecute] Input nodeId param invalid.";
        return MEM_POOLING_ERROR;
    }

    uint32_t ret = MEM_POOLING_ERROR;
    // 使用 MempoolingInterfaceAdapt 来管理函数执行状态
    MempoolingInterfaceAdapt guard;

    if (nodeId.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemFree][MemFreeExecute] Incoming parameter invalid.";
        return ret;
    }

    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterMemReturnFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemFree][MemFreeExecute] Concurrency is not supported, the current function cannot be entered.";
        return MEM_POOLING_ERROR;
    }
    ret = MempoolBorrowModule::Instance().MemFree(nodeId);
    mgr.ExitMemReturnFunc();
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemFree][MemFreeExecute] MemFree return code=" << ret << ".";
    return ret;
}

uint32_t mempooling::outinterface::UBSRMRSMemFreeWithMigrate(const std::string& borrowId)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemFree][MemFreeBase] Start to process memory free request, borrowId=" << borrowId << ".";

    if (borrowId.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemFree][MemFreeBase] Input borrowId param is empty.";
        return MEM_POOLING_ERROR;
    }

    std::vector<ubse::mem::controller::UbseNumaMemoryDebtInfo> debtInfos;
    if (MemBorrowExecutor::GetDebtInfosWithRetry(debtInfos) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemFree][MemFreeBase] GetDebtInfosWithRetry failed, borrowId=" << borrowId << ".";
        return MEM_POOLING_ERROR;
    }
    FaultNumaLockGuard lockGuard;
    std::unordered_set<uint16_t> seenNumas;
    for (const auto& debt : debtInfos) {
        if (debt.name == borrowId && debt.remoteNumaId >= 0) {
            auto numaId = static_cast<uint16_t>(debt.remoteNumaId);
            if (seenNumas.count(numaId) > 0) {
                continue;
            }
            seenNumas.insert(numaId);
            if (!FaultNumaLock::Instance().TryAcquireExclusive(numaId)) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[MemFree][MemFreeBase] Fault NUMA locked, numaId=" << numaId << ", borrowId=" << borrowId
                    << ".";
                return MEM_POOLING_HANDLING_FAULT;
            }
            lockGuard.exclusiveNumaIds.push_back(numaId);
        }
    }

    uint32_t ret = MEM_POOLING_ERROR;
    MempoolingInterfaceAdapt guard;

    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterMemReturnFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemFree][MemFreeBase] Concurrency is not supported, the current function cannot be entered.";
        return MEM_POOLING_ERROR;
    }
    ret = MemBorrowExecutor::Instance().MemFreeWithOpsForProcessMem(borrowId, false, true, false);
    mgr.ExitMemReturnFunc();

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemFree][MemFreeBase] MemFreeWithOps return code=" << ret << ", borrowId=" << borrowId << ".";
    return ret;
}

uint32_t mempooling::outinterface::UBSRMRSMemBorrowRollback(const string& borrowInNode, const vector<string>& borrowIds)
{
    if (borrowInNode != MpConfiguration::GetInstance().GetNodeId()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][Strategy] Input nodeId param invalid.";
        return MEM_POOLING_ERROR;
    }

    uint32_t ret = MEM_POOLING_ERROR;
    // 使用 MempoolingInterfaceAdapt 来管理函数执行状态
    MempoolingInterfaceAdapt guard;

    if (borrowInNode.empty() || borrowIds.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemRollback] Incoming parameter invalid.";
        return ret;
    }

    for (auto& borrowId : borrowIds) {
        if (borrowId.empty()) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemRollback] Incoming borrowIds have empty string.";
            return ret;
        }
    }

    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterOtherFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemRollback] Concurrency is not supported, the current function cannot be entered.";
        return MEM_POOLING_ERROR;
    }
    ret = RpcMemBorrowRollback(borrowInNode, borrowIds);
    mgr.ExitOtherFunc();
    return ret;
}

uint32_t mempooling::outinterface::UBSRMRSGetVmInfoListOnNode(
    std::vector<mempooling::outinterface::VmDomainInfo>& outVmDomainInfos)
{
    uint32_t ret = MEM_POOLING_ERROR;
    try {
        // 使用 MempoolingInterfaceAdapt 来管理函数执行状态
        MempoolingInterfaceAdapt guard;
        vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
        ret = HelpGetVmInfoListOnNode(vmDomainInfos);
        if (ret != MEM_POOLING_OK) {
            return ret;
        }
        // vector VmDomainInfo字段
        for (const auto& vmDomainInfo : vmDomainInfos) {
            mempooling::outinterface::VmDomainInfo outVmDomainInfo;
            outVmDomainInfo.metaData.nodeId = vmDomainInfo.metaData.nodeId;
            outVmDomainInfo.metaData.hostName = vmDomainInfo.metaData.hostName;
            outVmDomainInfo.metaData.uuid = vmDomainInfo.metaData.uuid;
            outVmDomainInfo.metaData.name = vmDomainInfo.metaData.name;
            outVmDomainInfo.metaData.vmCreateTime = vmDomainInfo.metaData.vmCreateTime;
            outVmDomainInfo.metaData.maxMem = vmDomainInfo.metaData.maxMem;
            outVmDomainInfo.metaData.pid = vmDomainInfo.metaData.pid;
            outVmDomainInfo.metaData.state = vmDomainInfo.metaData.state;
            for (const auto& [numaId, inNumaInfo] : vmDomainInfo.numaInfo) {
                mempooling::outinterface::VmDomainNumaInfo outNumaInfo;

                outNumaInfo.numaId = inNumaInfo.numaId;
                outNumaInfo.pageSize = inNumaInfo.pageSize;
                outNumaInfo.usedMem = inNumaInfo.usedMem;
                outNumaInfo.socketId = inNumaInfo.socketId;
                outNumaInfo.isLocal = inNumaInfo.isLocal;

                outVmDomainInfo.numaInfo.emplace(numaId, std::move(outNumaInfo));
            }

            outVmDomainInfo.timestamp = vmDomainInfo.timestamp;

            outVmDomainInfos.push_back(outVmDomainInfo);
        }

        return ret;
    } catch (const std::exception& e) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "UBSRMRSGetVmInfoListOnNode caught std::exception: " << e.what();
        return MEM_POOLING_ERROR;
    } catch (...) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "UBSRMRSGetVmInfoListOnNode caught unknown exception";
        return MEM_POOLING_ERROR;
    }
}

uint32_t mempooling::outinterface::UBSRMRSGetNumaInfoListOnNode(
    std::vector<mempooling::outinterface::NumaInfo>& outNumaInfos)
{
    uint32_t ret = MEM_POOLING_ERROR;
    // 使用 MempoolingInterfaceAdapt 来管理函数执行状态
    MempoolingInterfaceAdapt guard;
    vector<mempooling::exportV2::NumaInfo> numaInfos;
    ret = HelpGetNumaInfoListOnNode(numaInfos);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "numaInfos size is " << numaInfos.size();

    for (auto numaInfo : numaInfos) {
        mempooling::outinterface::NumaInfo outNumaInfo;

        outNumaInfo.metaData.nodeId = numaInfo.metaData.nodeId;
        outNumaInfo.metaData.hostName = numaInfo.metaData.hostName;
        outNumaInfo.metaData.numaId = numaInfo.metaData.numaId;
        outNumaInfo.metaData.isLocal = numaInfo.metaData.isLocal;
        outNumaInfo.metaData.memTotal = numaInfo.metaData.memTotal;
        outNumaInfo.metaData.memFree = numaInfo.metaData.memFree;
        for (const auto& [pageType, numaPageData] : numaInfo.metaData.numaPageInfo) {
            mempooling::outinterface::NumaPageData tmpNumaPageData;
            tmpNumaPageData.pageSize = numaPageData.pageSize;
            tmpNumaPageData.hugePageTotal = numaPageData.hugePageTotal;
            tmpNumaPageData.hugePageFree = numaPageData.hugePageFree;
            outNumaInfo.metaData.numaPageInfo.emplace(pageType, std::move(tmpNumaPageData));
        }
        outNumaInfo.metaData.socketId = numaInfo.metaData.socketId;
        outNumaInfo.timestamp = numaInfo.timestamp;
        outNumaInfos.push_back(outNumaInfo);
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "outNumaInfos size is " << outNumaInfos.size();

    return ret;
}

uint32_t mempooling::outinterface::UBSRMRSPidNumaInfoCollect(const SrcMemoryBorrowParam& srcParam,
                                                             const std::vector<pid_t>& pids,
                                                             std::vector<PidInfo>& pidInfos)
{
    uint32_t ret = MEM_POOLING_ERROR;
    // 使用 MempoolingInterfaceAdapt 来管理函数执行状态
    MempoolingInterfaceAdapt guard;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[PidNumaInfoCollect] Container PidNumaInfoCollect Start.";
    if (pids.empty() || pids.size() > MAX_PIDLIST_SIZE) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[PidNumaInfoCollect] Incoming parameter pids are invalid.";
        return ret;
    }
    std::vector<mempooling::RmrsPidInfo> pidInfoList;
    ret = ResourceQuery::HelpGetContainerPidNumaInfoByLocalNode(srcParam.srcNid, pids, pidInfoList);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[PidNumaInfoCollect] Container PidNumaInfoCollect failed.";
        return ret;
    }
    // vector 转成外部PidInfo
    for (const auto& pidInfo : pidInfoList) {
        PidInfo outPidInfo;
        outPidInfo.pid = pidInfo.pid;
        outPidInfo.localUsedMem = pidInfo.totalLocalUsedMem;
        outPidInfo.remoteUsedMem = pidInfo.totalRemoteUsedMem;
        for (auto& localNumaId : pidInfo.localNumaIds) {
            outPidInfo.localNumaIds.push_back(localNumaId);
        }
        pidInfos.push_back(outPidInfo);

        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[PidNumaInfoCollect] Collect result=" << outPidInfo.ToString() << ".";
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[PidNumaInfoCollect] Container PidNumaInfoCollect success.";
    return ret;
}

uint32_t mempooling::outinterface::UBSRMRSSetRunMode(const int& runMode)
{
    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterOtherFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[RunMode] Concurrency is not supported, the current function cannot be entered.";
        return MEM_POOLING_ERROR;
    }
    auto ret = mempooling::smap::MpSmapHelper::SetRunModeAndWrite(runMode);
    mgr.ExitOtherFunc();
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[RunMode] External interface, failed to set runmode, with ret = " << ret << ".";
        return ret;
    }

    if (runMode == 0) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[RunMode] External interface, set overCommitment mode success.";
    } else if (runMode == 1) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[RunMode] External interface, set memFragmentation mode success.";
    }

    return ret;
}

int ValidateParamsOfSmapAddProcessTracking(const std::vector<pid_t>& pidVec, const std::vector<uint32_t>& scanTimeVec,
                                           int scanType, const std::vector<uint32_t>& durationVec)
{
    if (pidVec.size() != scanTimeVec.size() || pidVec.size() != durationVec.size()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] Input parameter sizes do not match.";
        return SMAP_ERROR;
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] InputParam scanType=" << scanType << ".";
    for (size_t i = 0; i < pidVec.size(); ++i) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[SmapCtrl] InputParam pid=" << pidVec[i] << ", scanTime=" << scanTimeVec[i]
            << ", duration=" << durationVec[i] << ".";
    }

    if (pidVec.size() < SMAP_ADD_PROCESS_VEC_MIN_SIZE || pidVec.size() > SMAP_ADD_PROCESS_VEC_MAX_SIZE) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] Input pid param invalid.";
        return SMAP_ERROR;
    }

    if (scanTimeVec.size() < SMAP_ADD_PROCESS_VEC_MIN_SIZE || scanTimeVec.size() > SMAP_ADD_PROCESS_VEC_MAX_SIZE) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] Input scan Time Vec param invalid.";
        return SMAP_ERROR;
    }

    if (durationVec.size() < SMAP_ADD_PROCESS_VEC_MIN_SIZE || durationVec.size() > SMAP_ADD_PROCESS_VEC_MAX_SIZE) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] Input duration Vec param invalid.";
        return SMAP_ERROR;
    }

    return SMAP_OK;
}

int mempooling::outinterface::UBSRMRSSmapAddProcessTracking(const std::vector<pid_t>& pidVec,
                                                            const std::vector<uint32_t>& scanTimeVec, int scanType,
                                                            const std::optional<std::vector<uint32_t>>& durationVec)
{
    std::vector<uint32_t> durationVecParam;
    if (durationVec.has_value()) {
        durationVecParam = durationVec.value();
    } else {
        durationVecParam = std::vector<uint32_t>(pidVec.size(), SMAP_ADD_PROCESS_DURATION_DEFAULT_TIME);
    }
    int retParam = ValidateParamsOfSmapAddProcessTracking(pidVec, scanTimeVec, scanType, durationVecParam);
    if (retParam != SMAP_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] Input param invalid.";
        return SMAP_ERROR;
    }

    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterOtherFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[SmapCtrl] Concurrency is not supported, the current function cannot be entered.";
        return SMAP_ERROR;
    }

    int ret = smap::MpSmapHelper::SmapAddProcessTrackingHelper(pidVec, scanTimeVec, scanType, durationVecParam);
    if (ret == SMAP_OK) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] Out SMAP Add Process Tracking succeeded.";
    }
    mgr.ExitOtherFunc();
    return ret;
}

int mempooling::outinterface::UBSRMRSSmapQueryFreq(const pid_t& pid, std::vector<uint16_t>& dataVec,
                                                   const uint32_t& lengthIn, uint32_t& lengthOut, const int& dataSource)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] InputParam pid=" << pid << ".";
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] InputParam lengthIn=" << lengthIn << ".";

    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterOtherFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[SmapCtrl] Concurrency is not supported, the current function cannot be entered.";
        return SMAP_ERROR;
    }
    if (dataVec.size() < lengthIn) {
        // 若未分配内存，进行自适应分配
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[SmapCtrl] dataVec not preallocated, resizing to lengthIn = " << lengthIn << ".";
        dataVec.resize(lengthIn);
    }
    int ret = smap::MpSmapHelper::QueryVMFreqArray(pid, dataVec.data(), lengthIn, lengthOut, dataSource);
    mgr.ExitOtherFunc();
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "OutSmapQueryVmFreq size = " << lengthOut << ".";
    if (lengthOut > dataVec.size()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] lengthOut exceeds dataVec size.";
        return MEM_POOLING_ERROR;
    }
    for (int i = 0; i < lengthOut; ++i) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "OutSmapQueryVmFreq index = " << i << ",freq = " << dataVec[i] << ".";
    }

    if (ret == SMAP_OK) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] Out SMAP Query Vm Freq succeeded.";
    }

    return ret;
}

int mempooling::outinterface::UBSRMRSSmapRemoveProcessTracking(const std::vector<pid_t>& pidVec, int flags)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] InputParam flags=" << flags << ".";
    for (size_t i = 0; i < pidVec.size(); ++i) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] InputParam pid=" << pidVec[i] << ".";
    }

    if (pidVec.size() < SMAP_REMOVE_PROCESS_VEC_MIN_SIZE || pidVec.size() > SMAP_REMOVE_PROCESS_VEC_MAX_SIZE) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] Input param invalid.";
        return SMAP_ERROR;
    }

    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterOtherFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[SmapCtrl] Concurrency is not supported, the current function cannot be entered.";
        return SMAP_ERROR;
    }
    int ret = smap::MpSmapHelper::SmapRemoveProcessTrackingHelper(pidVec, flags);
    mgr.ExitOtherFunc();
    if (ret == SMAP_OK) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] Out SMAP Remove Process Tracking succeeded.";
    }
    return ret;
}

int mempooling::outinterface::UBSRMRSSmapEnableProcessMigrate(std::vector<pid_t> pidVec, int enable, int flags)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] InputParam enable=" << enable << ".";
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] InputParam flags=" << flags << ".";

    for (size_t i = 0; i < pidVec.size(); ++i) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] InputParam pid=" << pidVec[i] << ".";
    }
    if (pidVec.size() < SMAP_ENABLE_PROCESS_VEC_MIN_SIZE || pidVec.size() > SMAP_ENABLE_PROCESS_VEC_MAX_SIZE) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] Input param invalid.";
        return SMAP_ERROR;
    }

    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterOtherFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[SmapCtrl] Concurrency is not supported, the current function cannot be entered.";
        return SMAP_ERROR;
    }
    int ret = smap::MpSmapHelper::SmapEnableProcessMigrateHelper(pidVec.data(), pidVec.size(), enable, flags);
    mgr.ExitOtherFunc();
    if (ret == SMAP_OK) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl] Out SMAP enabled process migration succeeded.";
    }
    return ret;
}

/* *
 * @brief   移除进程的冷热页迁移
 *
 * @param pids   [IN] 移除的进程信息，包含进程的PID
 * @param pidType  [IN] 进程类型，目前支持4KB和2MB进程类型，int配置类型：0-进程（4K）1-虚拟机（2M）
 * @return int  0：操作成功；非0：操作失败
 */
int mempooling::outinterface::UBSRMRSRemove(const uint16_t remoteNumaId, const std::vector<pid_t>& pids, int pidType)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "Entry Remove.";
    RemoveMsg msg{};
    msg.count = static_cast<int>(pids.size());
    for (size_t i = 0; i < static_cast<size_t>(msg.count); ++i) {
        RemovePayload tmp{};
        tmp.pid = pids[i];
        // remoNumaId为0表示删除进程在所有远端NUMA上的冷热流动
        if (remoteNumaId == 0) {
            tmp.count = 0;
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "Detect remoteNumaId is 0, set payload.count = 0.";
        } else {
            tmp.count = 1;
            tmp.nid[0] = remoteNumaId;
        }
        msg.payload[i] = tmp;
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "Remove, pidType = " << pidType << ".";
    for (size_t i = 0; i < pids.size(); i++) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "Remove, pids = " << pids[i] << ".";
    }

    SmapRemoveFunc smapRemoveFunc = SmapModule::GetSmapRemoveFunc();
    if (smapRemoveFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapHelper] Ptr smapRemoveFunc == nullptr.";
        return MEM_POOLING_ERROR;
    }

    auto ret = smapRemoveFunc(&msg, pidType);
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "Out Remove, with code = " << ret << ".";
    return ret;
}

int mempooling::outinterface::RemoteNumaMigrate(const std::vector<pid_t>& pids, int srcNid, int destNid)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "Entry RemoteNumaMigrate.";
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "RemoteNumaMigrate, srcNid = " << srcNid << ".";
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "RemoteNumaMigrate, destNid = " << destNid << ".";
    for (size_t i = 0; i < pids.size(); i++) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "RemoteNumaMigrate, pids = " << pids[i] << ".";
    }

    pid_t* pidArr = const_cast<pid_t*>(pids.data());
    int len = static_cast<int>(pids.size());
    int retEnable = smap::MpSmapHelper::SmapEnableProcessMigrateHelper(pidArr, len, 0, 1);
    if (retEnable != SMAP_OK) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "RemoteNumaMigrate, ubturbo_smap_process_migrate_enable failed.";
    }

    SmapMigratePidRemoteNumaFunc smapMigratePidRemoteNumaFunc = SmapModule::GetSmapMigratePidRemoteNumaFunc();
    if (smapMigratePidRemoteNumaFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Ptr smapMigratePidRemoteNumaFunc == nullptr.";
        return MEM_POOLING_ERROR;
    }

    MigrateEscapeMsg msg;
    msg.count = len;

    for (int i = 0; i < msg.count; i++) {
        msg.payload[i].pid = pidArr[i];
        msg.payload[i].srcNid = srcNid;
        msg.payload[i].destNid = destNid;
        msg.payload[i].ratio = 100;
        msg.payload[i].migrateMode = MIG_RATIO_MODE; // 模式需要佳豪确认一下
    }

    auto ret = smapMigratePidRemoteNumaFunc(&msg);
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "Out RemoteNumaMigrate, with code = " << ret << ".";

    retEnable = smap::MpSmapHelper::SmapEnableProcessMigrateHelper(pidArr, len, 1, 1);
    if (retEnable != SMAP_OK) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "RemoteNumaMigrate, ubturbo_smap_process_migrate_enable failed.";
    }
    return ret;
}

static MpResult SetRemoteNumaInfoBeforeMigrateOut(const MigrateOutMsg& msg)
{
    std::vector<ubse::mem::controller::UbseNumaMemoryDebtInfo> allDebtInfos;
    if (MemBorrowExecutor::GetDebtInfosWithRetry(allDebtInfos) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "MigrateOut GetDebtInfosWithRetry failed, skip SetSmapRemoteNumaInfo.";
        return MEM_POOLING_ERROR;
    }
    auto validDebtInfos = MemBorrowExecutor::FilterValidDebtInfos(allDebtInfos);

    std::vector<over_commit::MemBorrowInfoWithSrc> infos;
    for (int i = 0; i < msg.count; ++i) {
        uint16_t remoteNumaId = static_cast<uint16_t>(msg.payload[i].inner[0].destNid);
        uint64_t totalSizeKB =
            MemBorrowExecutor::SumDebtInfosSizeBytesForRemoteNuma(validDebtInfos, static_cast<int16_t>(remoteNumaId)) /
            1024;
        if (totalSizeKB > 0) {
            infos.push_back({.srcNumaId = 0, .presentNumaId = remoteNumaId, .borrowSize = totalSizeKB});
        }
    }
    if (!infos.empty() && smap::MpSmapHelper::SetSmapRemoteNumaInfo(-1, infos) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "MigrateOut SetSmapRemoteNumaInfo failed, count=" << infos.size();
    }
    return MEM_POOLING_OK;
}

int mempooling::outinterface::UBSRMRSMigrateOut(const std::vector<MigrateOutPayload>& items, int pidType)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "Entry MigrateOut.";

    FaultNumaLockGuard lockGuard;
    std::unordered_set<uint16_t> seenNumas;
    for (const auto& item : items) {
        auto destNid = static_cast<uint16_t>(item.inner[0].destNid);
        if (seenNumas.count(destNid) > 0) {
            continue;
        }
        seenNumas.insert(destNid);
        if (!FaultNumaLock::Instance().TryAcquireShared(destNid)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "MigrateOut fault NUMA locked, destNid=" << destNid << ".";
            return MEM_POOLING_HANDLING_FAULT;
        }
        lockGuard.sharedNumaIds.push_back(destNid);
    }

    MigrateOutMsg msg{};
    msg.count = static_cast<int>(items.size());
    for (size_t i = 0; i < items.size(); ++i) {
        msg.payload[i] = items[i];
    }

    SmapMigrateOutFunc smapMigrateOutFunc = SmapModule::GetSmapMigrateOut();
    if (smapMigrateOutFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "MigrateOut Ptr smapMigrateOutFunc == nullptr.";
        return MEM_POOLING_ERROR;
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "MigrateOut, pidType=" << pidType << ", count=" << msg.count << ".";
    for (int i = 0; i < msg.count; ++i) {
        const auto& p = msg.payload[i];
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "MigrateOut payload[" << i << "], pid=" << p.pid << ", destNid=" << p.inner[0].destNid
            << ", memSize=" << p.inner[0].memSize << "KB";
    }

    SetRemoteNumaInfoBeforeMigrateOut(msg);

    int ret2 = smapMigrateOutFunc(&msg, pidType);
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "MigrateOut, with code = " << ret2 << ".";
    return ret2;
}

static uint32_t ValidateAndConvertPageSwapPairs(
    const std::vector<mempooling::outinterface::PageSwapPair>& pageSwapPairs, smap::GroupedMigrateOutMsg& msg)
{
    msg.count = 1;
    msg.payload[0].groupCount = static_cast<int>(pageSwapPairs.size());

    for (size_t i = 0; i < pageSwapPairs.size(); ++i) {
        const auto& pair = pageSwapPairs[i];
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[SmapCtrl][Convert] PageSwapPair[" << i << "] " << pair.ToString() << ".";

        if (pair.localNumas.empty() || pair.localNumas.size() > MAX_GROUP_LOCAL_NUMA) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl][Convert] localNumas size invalid.";
            return MEM_POOLING_ERROR;
        }

        if (pair.remoteNumas.empty() || pair.remoteNumas.size() > REMOTE_NUMA_NUM) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl][Convert] remoteNumas size invalid.";
            return MEM_POOLING_ERROR;
        }

        msg.payload[0].groups[i].localCount = static_cast<int>(pair.localNumas.size());
        for (size_t j = 0; j < pair.localNumas.size(); ++j) {
            msg.payload[0].groups[i].locals[j].nid = static_cast<int>(pair.localNumas[j].numaId);
            msg.payload[0].groups[i].locals[j].size = pair.localNumas[j].quota;
        }

        msg.payload[0].groups[i].targetCount = static_cast<int>(pair.remoteNumas.size());
        for (size_t k = 0; k < pair.remoteNumas.size(); ++k) {
            msg.payload[0].groups[i].targets[k].nid = static_cast<int>(pair.remoteNumas[k].numaId);
            msg.payload[0].groups[i].targets[k].size = pair.remoteNumas[k].quota;
        }
    }

    return MEM_POOLING_OK;
}

static void LogGroupedMigrateOutMsg(const smap::GroupedMigrateOutMsg& msg)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[SmapCtrl][LogMsg] msg.count=" << msg.count << ", pid=" << msg.payload[0].pid
        << ", groupCount=" << msg.payload[0].groupCount << ".";

    for (int i = 0; i < msg.payload[0].groupCount; ++i) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[SmapCtrl][LogMsg] Group[" << i << "]"
            << " localCount=" << msg.payload[0].groups[i].localCount
            << ", targetCount=" << msg.payload[0].groups[i].targetCount << ".";

        for (int j = 0; j < msg.payload[0].groups[i].localCount; ++j) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[SmapCtrl][LogMsg] Group[" << i << "] local[" << j << "]"
                << " nid=" << msg.payload[0].groups[i].locals[j].nid
                << ", size=" << msg.payload[0].groups[i].locals[j].size << "KB.";
        }

        for (int k = 0; k < msg.payload[0].groups[i].targetCount; ++k) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[SmapCtrl][LogMsg] Group[" << i << "] target[" << k << "]"
                << " nid=" << msg.payload[0].groups[i].targets[k].nid
                << ", size=" << msg.payload[0].groups[i].targets[k].size << "KB.";
        }
    }
}

uint32_t mempooling::outinterface::UBSRMRSSmapEnableProcessMigrateGrouped(
    pid_t pid, const std::vector<mempooling::outinterface::PageSwapPair>& pageSwapPairs)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl][MigrateGrouped] Entry, pid=" << pid << ".";

    if (pageSwapPairs.empty() || pageSwapPairs.size() > MAX_MIGRATION_GROUP_NUM) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[SmapCtrl][MigrateGrouped] pageSwapPairs size invalid=" << pageSwapPairs.size() << ".";
        return MEM_POOLING_ERROR;
    }

    uint32_t validateRet = MpVmQuotaUtil::ValidateNumaQuota(pid, pageSwapPairs);
    if (validateRet != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl][MigrateGrouped] NUMA quota validation failed.";
        return validateRet;
    }

    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterOtherFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl][MigrateGrouped] Concurrency is not supported.";
        return MEM_POOLING_ERROR;
    }

    smap::GroupedMigrateOutMsg msg{};
    msg.payload[0].pid = pid;

    uint32_t ret = ValidateAndConvertPageSwapPairs(pageSwapPairs, msg);
    if (ret != MEM_POOLING_OK) {
        mgr.ExitOtherFunc();
        return ret;
    }

    smap::SmapMigrateOutGroupedFunc smapMigrateOutGroupedFunc = smap::SmapModule::GetSmapMigrateOutGroupedFunc();
    if (smapMigrateOutGroupedFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[SmapCtrl][MigrateGrouped] GetSmapMigrateOutGroupedFunc failed.";
        mgr.ExitOtherFunc();
        return MEM_POOLING_ERROR;
    }

    LogGroupedMigrateOutMsg(msg);

    int smapRet = smapMigrateOutGroupedFunc(&msg, PID_TYPE);
    mgr.ExitOtherFunc();

    if (smapRet == SMAP_OK) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl][MigrateGrouped] succeeded.";
        return MEM_POOLING_OK;
    }

    UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapCtrl][MigrateGrouped] failed, ret=" << smapRet << ".";
    return MEM_POOLING_ERROR;
}

bool mempooling::outinterface::ApiConcurrencyManager::TryEnterOtherFunc()
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_isRunningOther || m_runningMemReturnCount > 0) {
        return false;
    }
    m_isRunningOther = true;
    return true;
}

void mempooling::outinterface::ApiConcurrencyManager::ExitOtherFunc()
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_isRunningOther = false;
}

bool mempooling::outinterface::ApiConcurrencyManager::TryEnterMemReturnFunc()
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_isRunningOther) {
        return false;
    }
    m_runningMemReturnCount++;
    return true;
}

void mempooling::outinterface::ApiConcurrencyManager::ExitMemReturnFunc()
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_runningMemReturnCount--;
}

} // namespace mempooling