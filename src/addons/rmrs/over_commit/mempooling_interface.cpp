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

#include <cstdint>
#include <vector>

#include <ubse_logger.h>

#include "collect_util.h"
#include "interface_guard.h"
#include "mem_borrow_executor.h"
#include "mem_manager.h"
#include "mempool_borrow_module.h"
#include "mempool_migrate_module.h"
#include "mempooling_message.h"
#include "mp_configuration.h"
#include "mp_default_struct.h"
#include "mp_error.h"
#include "mp_smap_module.h"
#include "mp_string_util.h"
#include "mp_vector_util.h"
#include "over_commit_def.h"
#include "over_commit_msg.h"
#include "over_commit_msg_handler.h"
#include "over_commit_storage.h"
#include "over_commit_ucache_strategy.h"
#include "set_smap_remote_numa_info_send.h"
#include "smap_query_process_send.h"
#include "smap_remove_send.h"
#include "vm_mem_migrate_strategy.h"

namespace mempooling::outinterface {
using namespace ubse::log;
using namespace ubse::election;
using namespace ubse::mem::controller;
const int HIGH_WATER_MARK_THRESHOLD = 100;
constexpr int MAX_VM_NUM = 100;
constexpr int MAX_BORROW_SIZES = 256;

struct ReturnNeedMaps {
    std::unordered_map<std::string, uint64_t> borrowId2Size{};
    std::unordered_map<uint16_t, uint64_t> borrowIdNuma2Size{};
    std::unordered_map<std::uint16_t, std::vector<pid_t>>& borrowNumaId2Pids;
    std::unordered_map<std::string, std::uint16_t> borrowId2NumaId;
};

/**
 * 从账本信息中, 获取指定全量借用的大小(仅borrowId中涉及的remoteNuma)
 * @param borrowRecords 账本信息
 * @param borrowIds 内存借用资源名
 * @return map<remoteNumaId,totalBorrowSize>
*/
std::vector<MemBorrowInfoWithSrc> GenNumaTotalBorrowSizes(const std::vector<BorrowRecord>& borrowRecords,
                                                          const std::vector<std::string>& borrowIds);

/**
 * 从账本信息中, 获取指定本次借用的大小
 * @param borrowRecords 账本信息
 * @param borrowIds 内存借用资源名
 * @return map<remoteNumaId,CurBorrowSize>
 */
std::vector<MemBorrowInfo> GenNumaCurBorrowSizes(const std::vector<BorrowRecord>& borrowRecords,
                                                 const std::vector<std::string>& borrowIds);

/**
 * 从账本中获取 borrowId借用大小map
 * @param borrowRecords 账本信息
 * @param borrowIds 内存借用资源名
 * @return map<borrowId,CurBorrowSize>
 */
std::unordered_map<std::string, std::uint64_t> GenBorrowId2Size(const std::vector<BorrowRecord>& borrowRecords,
                                                                const std::vector<std::string>& borrowIds);

/**
 * 从账本中获取 borrowId对应numa借用大小map
* @param srcNumaId
* @param borrowRecords 账本信息
 * @param borrowIds 内存借用资源名
 * @return map<borrowId,totalBorrowSize>
 */
std::unordered_map<std::uint16_t, std::uint64_t> GenBorrowIdNuma2Size(const int16_t& srcNumaId,
                                                                      const std::vector<BorrowRecord>& borrowRecords,
                                                                      const std::vector<std::string>& borrowIds);

/**
 * 获取borrowId中使用的远端numaId map
 * @param borrowRecords 账本信息
 * @param borrowIds 内存借用资源名
 * @return map<borrowId,presentNumaId>
 */
std::unordered_map<std::string, std::uint16_t> GenBorrowId2NumaId(const std::vector<BorrowRecord>& borrowRecords,
                                                                  const std::vector<std::string>& borrowIds);

/**
 * 执行单个borrowId归还动作
 * @param srcParam 告警节点信息
 * @param pids numa上虚拟机进程列表
 * @param borrowId 借用内存描述符
 * @param presentNumaId 借用远端NumaId
 * @param returnNeedMaps 全局缓存信息, 包含告警numa在远端numa上借用内存大小的map+borrowId与远端NumaId映射关系
 * @return
 */
uint32_t ReturnBorrowId(const SrcMemoryBorrowParam& srcParam, const std::vector<pid_t>& pids,
                        const std::string& borrowId, const uint16_t& presentNumaId, ReturnNeedMaps& returnNeedMaps);

uint32_t PrepareSocketId(std::map<int, mempooling::NumaMetaData>& numaMemInfos, const std::string& srcNid)
{
    auto ret = CollectUtil::GetNumaMemInfos(srcNid, {0}, numaMemInfos);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow] Get socketID for numa0 failed. Node = " << srcNid << ".";
        return ret;
    }
    auto iter = numaMemInfos.find(0);
    if (iter == numaMemInfos.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow] Get socketID for numa0 is empty. Node = " << srcNid << ".";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

uint32_t checkBorrowParam(const std::vector<uint64_t>& borrowSizes, const WaterMark& waterMark)
{
    if (borrowSizes.empty() || borrowSizes.size() > MAX_BORROW_SIZES) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate] Parma borrowSizes Invalid.";
        return MEM_POOLING_ERROR;
    }

    if (waterMark.highWaterMark > HIGH_WATER_MARK_THRESHOLD || waterMark.highWaterMark <= waterMark.lowWaterMark) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[SetWaterMark] WaterMark value is invalid.";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

uint32_t UBSRMRSMemBorrow(const SrcMemoryBorrowParam& srcParam, const std::vector<uint64_t>& borrowSizes,
                          const WaterMark& waterMark, MemBorrowExecuteResult& result)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow] Start Borrow.";
    if (checkBorrowParam(borrowSizes, waterMark) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    auto guard = InterfaceGuard::InvokeOutMemBorrow();
    auto bindType = (srcParam.srcNumaId == -1) ? NumaBindType::BIND_MULTIPLE : NumaBindType::BIND_SINGLE;
    auto ret = OverCommitStorage::Instance().UpdateBindTypeDB(srcParam.srcNid, bindType);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow] Update bind type failed. srcParam=" << srcParam.ToString()
            << ", bindType=" << BindTypeToStr(bindType) << ".";
    }
    const auto mpSrcParam = mempooling::SrcMemoryBorrowParam{.srcNid = srcParam.srcNid,
                                                             .srcSocketId = srcParam.srcSocketId,
                                                             .srcNumaId = srcParam.srcNumaId,
                                                             .uid = srcParam.uid,
                                                             .username = srcParam.username};
    std::vector<uint64_t> borrowKbSizes;
    for (auto& borrowSize : borrowSizes) {
        (void)borrowKbSizes.emplace_back(borrowSize >> BYTE2KB);
    }

    // 借用执行
    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterOtherFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow] Concurrency is not supported.";
        return MEM_POOLING_ERROR;
    }
    mempooling::MemBorrowExecuteResult mpResult;
    ret = MempoolBorrowModule::MemBorrowExecuteInOverCommit(
        mpSrcParam, borrowSizes,
        mempooling::WaterMark({.highWaterMark = waterMark.highWaterMark, .lowWaterMark = waterMark.lowWaterMark}),
        mpResult);
    mgr.ExitOtherFunc();
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow] MpResult=" << mpResult.ToString() << ".";
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow] MemBorrowExecute failed. ret=" << ret << ".";
        return ret;
    } else if (mpResult.borrowIds.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow] MemBorrowExecute mpResult borrowId is empty.";
        return MEM_POOLING_ERROR;
    }
    result.borrowIds = mpResult.borrowIds;
    result.presentNumaIds = mpResult.presentNumaId;
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow] End Borrow.";
    return ret;
}

uint32_t checkVMParam(const std::vector<VMPresetParam>& vmPresetParams)
{
    if (vmPresetParams.empty() || vmPresetParams.size() > MAX_VM_NUM) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate] VM number Invalid.";
        return MEM_POOLING_ERROR;
    }
    for (auto param : vmPresetParams) {
        if (param.ratio > mempooling::smap::SMAP_MAX_RATIO_MP) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate] Vm ratio Invalid.";
            return MEM_POOLING_ERROR;
        }
    }
    return MEM_POOLING_OK;
}

// 执行UCache迁移
MpResult UcacheMigrate(const std::vector<VMPresetParam>& vmPresetParams, const MemBorrowExecuteResult& result,
                       const SrcMemoryBorrowParam& srcParam)
{
    if (!mempooling::MpConfiguration::GetInstance().GetUcacheEnable()) {
        return MEM_POOLING_OK;
    }
    // pagecache迁移决策
    auto ret = MEM_POOLING_OK;
    UCacheMigrationStrategyParam ucacheStrategy = {};
    std::vector<pid_t> pids{};
    for (auto& param : vmPresetParams) {
        pids.push_back(param.pid);
    }
    std::vector<uint16_t> remoteNumaIds{};
    for (auto numaId : result.presentNumaIds) {
        remoteNumaIds.push_back(numaId);
    }
    GenUCacheMigrationStrategy(srcParam.srcNumaId, pids, remoteNumaIds, srcParam.srcNid, ucacheStrategy);
    if (ucacheStrategy.ucacheUsageRatio == 0) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][ucache] UCache usageratio is 0, no need to migrate.";
        return MEM_POOLING_OK;
    }

    // pagecache迁移执行
    ret = SendUCacheMigrationStrategy(ucacheStrategy, srcParam.srcNid);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][ucache] Send UCache MigrationStrategy failed, result=" << ret << ".";
        return ret;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate] Mem and ucache all migrate End.";
    return ret;
}

uint32_t HandleMemMigrate(const SrcMemoryBorrowParam& srcParam, const std::vector<BorrowRecord>& borrowRecord,
                          const std::vector<VMPresetParam>& vmPresetParams, const std::vector<std::string>& borrowIds,
                          std::vector<MemMigrateResult>& memMigrateResults)
{
    std::vector<MemBorrowInfoWithSrc> memTotalBorrowInfos;

    if (MpConfiguration::GetInstance().GetMpSceneType() == MpSceneType::VIRTUAL_SCENE &&
        MpConfiguration::GetInstance().GetMultiNumaScene() == true) {
        // 多numa场景+虚机场景 迁移执行
        if (memMigrateResults.empty()) {
            // 多numa虚机场景下，若生成迁移策略为空，则直接失败返回
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MultiNuma] MigrateResult is empty.";
            return MEM_POOLING_ERROR;
        } else {
            memTotalBorrowInfos = GenNumaTotalBorrowSizes(borrowRecord, borrowIds);
        }
    } else {
        // 单numa场景 迁移执行
        if (!memMigrateResults.empty()) {
            memTotalBorrowInfos = GenNumaTotalBorrowSizes(borrowRecord, borrowIds);
        } else {
            // memMigrateResults 是empty代表需要rebalance
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemMigrate] Process memMigrateResults is empty need rebalance.";
            for (auto vm : vmPresetParams) {
                MemMigrateResult res;
                res.maxRatio = vm.ratio;
                res.pid = vm.pid;
                (void)memMigrateResults.emplace_back(res);
            }
        }
    }
    // 迁移执行
    auto ret = OverCommitMsgHandler::MigrateLocalHandler(srcParam, memTotalBorrowInfos, memMigrateResults);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate] Migrate failed, retcode=" << ret << ".";
        return ret;
    }

    return ret;
}

// 更新UCache迁移比例
void CallUpdateUcacheUsageRatio(const std::vector<VMPresetParam>& vmPresetParams,
                                const std::vector<BorrowRecord> borrowRecord, const SrcMemoryBorrowParam& srcParam)
{
    if (!MpConfiguration::GetInstance().GetUcacheEnable()) {
        return;
    }
    std::vector<pid_t> pids{};
    for (auto& param : vmPresetParams) {
        pids.push_back(param.pid);
    }
    uint64_t borrowMemKB{};
    for (auto record : borrowRecord) {
        borrowMemKB += record.size;
    }
    UpdateUcacheUsageRatio(pids, borrowMemKB, srcParam.srcNid);
}

uint32_t UBSRMRSMemMigrate(const SrcMemoryBorrowParam& srcParam, const std::vector<VMPresetParam>& vmPresetParams,
                           const MemBorrowExecuteResult& result)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate] Start mem migrate.";
    auto guard = InterfaceGuard::InvokeOutMemMigrate();
    if (checkVMParam(vmPresetParams) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    // 查询账本
    std::vector<BorrowRecord> borrowRecord;
    auto ret = BorrowRecordHelper::Instance().CollectBorrowRecordsOnlyBorrowIn(srcParam.srcNid, srcParam.srcNumaId,
                                                                               borrowRecord);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate] Query borrow record failed. srcParam=" << srcParam.ToString() << ".";
        return MEM_POOLING_ERROR;
    }

    // 决策Pagecache迁移比例
    CallUpdateUcacheUsageRatio(vmPresetParams, borrowRecord, srcParam);
    // 迁移决策
    const auto memCurBorrowInfos = GenNumaCurBorrowSizes(borrowRecord, result.borrowIds);
    if (borrowRecord.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate] BorrowRecords that filter by borrowIds is empty, migrate end.";
        return MEM_POOLING_ERROR;
    }
    std::vector<MemMigrateResult> memMigrateResults{};
    ret = ProcessMemMigrateRemoteId(srcParam, vmPresetParams, memCurBorrowInfos, borrowRecord, memMigrateResults);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate] Process remote numa failed.";
        return MEM_POOLING_ERROR;
    } else if (memMigrateResults.empty()) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate] Process memMigrateResults is empty.";
    }
    // 迁移执行
    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterOtherFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate] Concurrency is not supported, the current function cannot be entered.";
        return MEM_POOLING_ERROR;
    }
    ret = HandleMemMigrate(srcParam, borrowRecord, vmPresetParams, result.borrowIds, memMigrateResults);
    mgr.ExitOtherFunc();
    if (ret != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    if (UcacheMigrate(vmPresetParams, result, srcParam) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate] End mem migrate.";
    return MEM_POOLING_OK;
}

bool HasPidInMigratePids(const std::vector<pid_t>& pids, const std::vector<pid_t>& migratePids)
{
    for (const auto& pid : pids) {
        if (std::find(migratePids.begin(), migratePids.end(), pid) != migratePids.end()) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemReturn] HasPidInMigratePids hits, pid=" << pid << " is in migratePids.";
            return true;
        }
    }
    return false;
}

uint32_t FilterRemoteNumaPidsByLocalNuma(const std::vector<VmNumaInfoWithSocket>& vmNumaInfoWithSocketList,
                                         const std::vector<pid_t>& remoteNumaPids, const uint16_t localNumaId,
                                         std::vector<pid_t>& pidsOnNuma)
{
    for (const auto& pid : remoteNumaPids) {
        // 过滤告警节点上的虚拟机
        for (auto& vminfo : vmNumaInfoWithSocketList) {
            if (vminfo.pid == pid && vminfo.localNumaId == localNumaId) {
                pidsOnNuma.push_back(vminfo.pid);
            }
            if (vminfo.pid == pid) {
                break;
            }
        }
    }
    return MEM_POOLING_OK;
}

MpResult GenpidsOnNuma(int16_t srcNumaId, std::vector<pid_t>& pidsOnNuma,
                       std::vector<mempooling::RmrsPidInfo>& pidInfos)
{
    for (auto pInfo : pidInfos) {
        if (pInfo.localNumaIds.empty()) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] HelpGetContainerPidNumaInfo "
                                                                 "return process Info error localnuma is empty, pid="
                                                              << pInfo.pid << ".";
            return MEM_POOLING_ERROR;
        }

        auto numaId = pInfo.localNumaIds.front();
        if (srcNumaId == -1 || numaId == srcNumaId) {
            (void)pidsOnNuma.emplace_back(pInfo.pid);
        }
    }
    return MEM_POOLING_OK;
}

MpResult ProcessBorrowIds(const SrcMemoryBorrowParam& srcParam, const std::vector<std::string>& borrowIds,
                          const std::vector<pid_t>& migratePids, ReturnNeedMaps& returnNeedMaps)
{
    MpResult ret = MEM_POOLING_OK;
    bool hasPidInMigratePids = false;

    std::vector<std::string> faultProcessBorrowIds;
    auto retfaultProcessBorrowIds = BorrowIdInFaultProcess::Instance().Query(faultProcessBorrowIds);
    if (retfaultProcessBorrowIds != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] Query faultProcessBorrowIds failed.";
        return MEM_POOLING_ERROR;
    }

    for (const auto& borrowId : borrowIds) {
        bool exists = std::find(faultProcessBorrowIds.begin(), faultProcessBorrowIds.end(), borrowId) !=
                      faultProcessBorrowIds.end();
        if (exists) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemReturn] BorrowId=" << borrowId << " is in faultmemt processing.";
            return MEM_POOLING_ERROR;
        }
        std::vector<pid_t> pidsOnNuma;
        const auto remoteNumaPids = returnNeedMaps.borrowNumaId2Pids[returnNeedMaps.borrowId2NumaId[borrowId]];
        std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
        std::vector<mempooling::RmrsPidInfo> pidInfos;
        if (srcParam.srcNumaId < 0) {
            pidsOnNuma = remoteNumaPids;
        } else if (MpConfiguration::GetInstance().GetMpSceneType() == MpSceneType::VIRTUAL_SCENE) {
            ret = over_commit::OverCommitMsg::GetVmNumaInfoMapLocal(vmNumaInfoWithSocketList, srcParam.srcNumaId);
            if (ret != MEM_POOLING_OK) {
                UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] GetVmNumaInfoMapLocal failed.";
            }
            FilterRemoteNumaPidsByLocalNuma(vmNumaInfoWithSocketList, remoteNumaPids, srcParam.srcNumaId, pidsOnNuma);
        } else {
            ret = ResourceQuery::HelpGetContainerPidNumaInfoByLocalNode(srcParam.srcNid, remoteNumaPids, pidInfos);
            if (ret != MEM_POOLING_OK) {
                UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] HelpGetContainerPidNumaInfo failed.";
            }
            ret = GenpidsOnNuma(srcParam.srcNumaId, pidsOnNuma, pidInfos);
            if (ret != MEM_POOLING_OK) {
                UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] GenPidsOnNuma failed.";
            }
        }
        // 迁移中的虚拟机无法归还内存
        if (HasPidInMigratePids(pidsOnNuma, migratePids)) {
            hasPidInMigratePids = true;
            continue;
        }
        // ucache: 检查到pgaecache停止迁移后 CheckAndStopPagecacheMigration
        CheckAndStopUCacheMigration(srcParam.srcNid);
        ret = ReturnBorrowId(srcParam, pidsOnNuma, borrowId, returnNeedMaps.borrowId2NumaId[borrowId], returnNeedMaps);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemReturn] Return borrow id failed. ret=" << ret << ".";
            return ret;
        }
    }

    if (hasPidInMigratePids == true) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemReturn] Has pid in migratePids and MemReturn ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    return ret;
}

uint32_t UBSRMRSMemReturn(const SrcMemoryBorrowParam& srcParam, const std::vector<std::string>& borrowIds,
                          const std::vector<pid_t>& migratePids)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] Start Return.";
    auto guard = InterfaceGuard::InvokeOutMemReturn();
    // 查询账本
    std::vector<BorrowRecord> borrowRecord;
    auto ret = BorrowRecordHelper::Instance().CollectBorrowRecords(srcParam.srcNid, borrowRecord);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemReturn] Query borrow record failed. srcParam=" << srcParam.ToString() << ".";
        return MEM_POOLING_ERROR;
    }
    const auto borrowId2Size = GenBorrowId2Size(borrowRecord, borrowIds);
    auto borrowNuma = srcParam.srcNumaId;
    const auto borrowIdNuma2Size = GenBorrowIdNuma2Size(borrowNuma, borrowRecord, borrowIds);
    auto borrowId2NumaId = GenBorrowId2NumaId(borrowRecord, borrowIds);
    std::vector<std::uint32_t> remoteIds;
    for (const auto& [key, value] : borrowId2NumaId) {
        // 检查值是否已存在
        if (std::find(remoteIds.begin(), remoteIds.end(), value) == remoteIds.end()) {
            remoteIds.push_back(value);
        }
    }
    std::unordered_map<std::uint16_t, std::vector<pid_t>> borrowNumaId2Pids;
    ret = CollectUtil::GetRemoteVmPidsByLocal(remoteIds, borrowNumaId2Pids, true);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemReturn] Query remote process failed. srcParam=" << srcParam.ToString() << ".";
        return MEM_POOLING_ERROR;
    }
    if (borrowIdNuma2Size.empty() || borrowId2NumaId.empty()) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] BorrowId already return.";
        return MEM_POOLING_OK;
    }
    auto returnNeedMaps = ReturnNeedMaps({.borrowId2Size = borrowId2Size,
                                          .borrowIdNuma2Size = borrowIdNuma2Size,
                                          .borrowNumaId2Pids = borrowNumaId2Pids,
                                          .borrowId2NumaId = borrowId2NumaId});

    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterOtherFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] Concurrency is not supported.";
        return MEM_POOLING_ERROR;
    }
    ret = ProcessBorrowIds(srcParam, borrowIds, migratePids, returnNeedMaps);
    mgr.ExitOtherFunc();
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemReturn] ProcessBorrowIds failed. srcParam=" << srcParam.ToString() << ".";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] End Return.";
    return MEM_POOLING_OK;
}

void PrintReturnInfo(const SrcMemoryBorrowParam& srcParam, const std::string& borrowId, const uint16_t& presentNumaId,
                     int pidSize)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] srcParam.srcNid=" << srcParam.srcNid << ".";
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemReturn] srcParam.srcSocketId=" << srcParam.srcSocketId << ".";
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] srcParam.srcNumaId=" << srcParam.srcNumaId << ".";
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] pids_size=" << pidSize << ".";
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] borrowId=" << borrowId << ".";
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] presentNumaId=" << presentNumaId << ".";
}

void PersistenceRemovePid(const int16_t presentRemoteNumaId, const std::vector<pid_t>& pids)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemReturn] borrowIdNuma2Size = 0, need to PersistenceRemovePid.";
    MpResult retRemovePidCompleted = RemovePidCompleted::Instance().Update(presentRemoteNumaId, pids);
    if (retRemovePidCompleted != MEM_POOLING_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemReturn] Update RemovePidCompleted pids failed. ret=" << retRemovePidCompleted << ".";
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] Update RemovePidCompleted pids success.";
    }
}

void DeletePersistenceRemovePid(const int16_t presentRemoteNumaId, const std::vector<pid_t>& pids)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemReturn] Delete PersistenceRemovePid of presentRemoteNumaId=" << presentRemoteNumaId << ".";
    MpResult retRemovePidCompleted = RemovePidCompleted::Instance().Remove(presentRemoteNumaId, pids);
    if (retRemovePidCompleted != MEM_POOLING_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemReturn] Delete PersistenceRemovePid pids failed. ret=" << retRemovePidCompleted << ".";
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] Delete PersistenceRemovePid pids success.";
    }
}

uint32_t ReturnBorrowId(const SrcMemoryBorrowParam& srcParam, const std::vector<pid_t>& pids,
                        const std::string& borrowId, const uint16_t& presentNumaId, ReturnNeedMaps& returnNeedMaps)
{
    PrintReturnInfo(srcParam, borrowId, presentNumaId, pids.size());
    auto& [borrowId2Size, borrowIdNuma2Size, borrowNumaId2Pids, _] = returnNeedMaps;
    if (borrowId.empty()) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] borrowId is empty, skip.";
        return MEM_POOLING_OK;
    }
    borrowIdNuma2Size[presentNumaId] -= borrowId2Size[borrowId];
    if (borrowIdNuma2Size[presentNumaId] == 0 && pids.size() != 0) {
        // 持久化pids
        PersistenceRemovePid(presentNumaId, pids);
    }
    auto borrowSize = borrowIdNuma2Size[presentNumaId] * (1 - GetUcacheUsageRatio(srcParam.srcNid));
    std::vector<MemBorrowInfo> memBorrowInfos = {
        {.presentNumaId = presentNumaId, .borrowSize = static_cast<uint64_t>(borrowSize)}};
    auto ret = OverCommitMsgHandler::SetSmapRemoteNumaLocalHandler(srcParam, memBorrowInfos);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }
    ret = MemBorrowExecutor::Instance().MemFreeWithOps(borrowId, false, true);
    if (ret != MEM_POOLING_OK) {
        UbseMemResult result;
        if (UbseQueryResult(borrowId, result) != 0) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] UbseQueryResult failed.";
            return MEM_POOLING_ERROR;
        }
        if (result.stage == ubse::mem::controller::UbseMemStage::UBSE_EXIST) {
            borrowIdNuma2Size[presentNumaId] += borrowId2Size[borrowId];
            borrowSize = borrowIdNuma2Size[presentNumaId] * (1 - GetUcacheUsageRatio(srcParam.srcNid));
            memBorrowInfos = {{.presentNumaId = presentNumaId, .borrowSize = static_cast<uint64_t>(borrowSize)}};
            MpResult retCode = OverCommitMsgHandler::SetSmapRemoteNumaLocalHandler(srcParam, memBorrowInfos);
            if (retCode != MEM_POOLING_OK) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] Reset process remote numa failed.";
            }
            return ret;
        }
    } else {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemReturn] Success. borrowId=" << borrowId << ", borrowSize=" << borrowId2Size[borrowId] << " KB.";
    }
    if (borrowIdNuma2Size[presentNumaId] == 0 && pids.size() != 0) {
        ret = OverCommitMsgHandler::RemoveLocalHandler(presentNumaId, pids);
        // 执行完成RemovePid后，无论成功与否都从持久化数据库中删除RemovePid数据
        DeletePersistenceRemovePid(presentNumaId, pids);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] Smap remove failed.";
            return ret;
        }
    }
    return MEM_POOLING_OK;
}

std::unordered_set<std::uint16_t> GenBorroIdRemoteNumaSet(const std::vector<BorrowRecord>& borrowRecords,
                                                          const std::vector<std::string>& borrowIds)
{
    std::unordered_set<std::uint16_t> remoteNumaIds;
    for (const auto &[name, size, lentNode, lentMemId, lentSocketId, lentNuma, borrowNode,
                      borrowLocalNuma, borrowRemoteNuma, borrowMemId, uid, username, borrowSocketId] : borrowRecords) {
        if (std::find(borrowIds.begin(), borrowIds.end(), name) == borrowIds.end()) {
            continue;
        }
        (void)remoteNumaIds.emplace(borrowRemoteNuma);
    }
    return remoteNumaIds;
}

std::vector<MemBorrowInfoWithSrc> GenNumaTotalBorrowSizes(const std::vector<BorrowRecord>& borrowRecords,
                                                          const std::vector<std::string>& borrowIds)
{
    const auto remoteNumaIds = GenBorroIdRemoteNumaSet(borrowRecords, borrowIds);
    std::unordered_map<std::uint16_t, uint64_t> remoteNumaId2BorrowSizeMap;
    std::vector<MemBorrowInfoWithSrc> result;
    for (const auto &[name, size, lentNode, lentMemId, lentSocketId, lentNuma, borrowNode, borrowLocalNuma,
                      borrowRemoteNuma, borrowMemId, uid, username, borrowSocketId] : borrowRecords) {
        if (remoteNumaIds.find(borrowRemoteNuma) == remoteNumaIds.end()) {
            continue;
        }
        MemBorrowInfoWithSrc memBorrowInfoWithSrc = {.srcNumaId = static_cast<uint16_t>(borrowLocalNuma),
                                                     .presentNumaId = static_cast<uint16_t>(borrowRemoteNuma),
                                                     .borrowSize = size};
        (void)result.emplace_back(memBorrowInfoWithSrc);
    }
    return result;
}

std::vector<MemBorrowInfo> GenNumaCurBorrowSizes(const std::vector<BorrowRecord>& borrowRecords,
                                                 const std::vector<std::string>& borrowIds)
{
    std::unordered_map<std::uint16_t, uint64_t> remoteNumaId2BorrowSizeMap;
    std::vector<MemBorrowInfo> result;
    for (const auto &[name, size, lentNode, lentMemId, lentSocketId, lentNuma, borrowNode, borrowLocalNuma,
                      borrowRemoteNuma, borrowMemId, uid, username, borrowSocketId] : borrowRecords) {
        if (std::find(borrowIds.begin(), borrowIds.end(), name) == borrowIds.end()) {
            continue;
        }
        if (remoteNumaId2BorrowSizeMap.find(borrowRemoteNuma) == remoteNumaId2BorrowSizeMap.end()) {
            remoteNumaId2BorrowSizeMap[borrowRemoteNuma] = size;
        } else {
            remoteNumaId2BorrowSizeMap[borrowRemoteNuma] += size;
        }
    }

    for (const auto& [fst, snd] : remoteNumaId2BorrowSizeMap) {
        (void)result.emplace_back(MemBorrowInfo({.presentNumaId = fst, .borrowSize = snd}));
    }
    return result;
}

std::unordered_map<std::string, std::uint64_t> GenBorrowId2Size(const std::vector<BorrowRecord>& borrowRecords,
                                                                const std::vector<std::string>& borrowIds)
{
    std::unordered_map<std::string, std::uint64_t> result;
    for (const auto &[name, size, lentNode, lentMemId, lentSocketId, lentNuma, borrowNode, borrowLocalNuma,
                      borrowRemoteNuma, borrowMemId, uid, username, borrowSocketId] : borrowRecords) {
        if (std::find(borrowIds.begin(), borrowIds.end(), name) != borrowIds.end()) {
            result[name] = size;
        }
    }
    return result;
}

std::unordered_map<std::uint16_t, std::uint64_t> GenBorrowIdNuma2Size(const int16_t& srcNumaId,
                                                                      const std::vector<BorrowRecord>& borrowRecords,
                                                                      const std::vector<std::string>& borrowIds)
{
    const auto remoteNumaIds = GenBorroIdRemoteNumaSet(borrowRecords, borrowIds);
    std::unordered_map<std::uint16_t, std::uint64_t> result;
    for (const auto &[name, size, lentNode, lentMemId, lentSocketId, lentNuma, borrowNode, borrowLocalNuma,
                      borrowRemoteNuma, borrowMemId, uid, username, borrowSocketId] : borrowRecords) {
        if (remoteNumaIds.find(borrowRemoteNuma) == remoteNumaIds.end() ||
            (srcNumaId != -1 && borrowLocalNuma != srcNumaId)) {
            continue;
        }
        if (result.find(borrowRemoteNuma) == result.end()) {
            result[borrowRemoteNuma] = size;
        } else {
            result[borrowRemoteNuma] += size;
        }
    }
    return result;
}

std::unordered_map<std::string, std::uint16_t> GenBorrowId2NumaId(const std::vector<BorrowRecord>& borrowRecords,
                                                                  const std::vector<std::string>& borrowIds)
{
    const auto remoteNumaIds = GenBorroIdRemoteNumaSet(borrowRecords, borrowIds);
    std::unordered_map<std::string, std::uint16_t> result;
    for (const auto &[name, size, lentNode, lentMemId, lentSocketId, lentNuma, borrowNode, borrowLocalNuma,
                      borrowRemoteNuma, borrowMemId, uid, username, borrowSocketId] : borrowRecords) {
        if (std::find(borrowIds.begin(), borrowIds.end(), name) != borrowIds.end()) {
            result[name] = borrowRemoteNuma;
        }
    }
    return result;
}

uint32_t UBSRMRSSetWaterMark(const WaterMark& waterMark)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[SetWaterMark] High water line is " << waterMark.highWaterMark
                                                      << "Low water line is" << waterMark.lowWaterMark << ".";
    if (waterMark.highWaterMark > HIGH_WATER_MARK_THRESHOLD || waterMark.highWaterMark <= waterMark.lowWaterMark) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[SetWaterMark] Set value is invalid.";
        return MEM_POOLING_ERROR;
    }
    auto& mgr = ApiConcurrencyManager::getInstance();
    if (!mgr.TryEnterOtherFunc()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[SetWaterMark] Concurrency is not supported, the current function cannot be entered.";
        return MEM_POOLING_ERROR;
    }
    auto ret = OverCommitStorage::Instance().UpdateWaterMark(waterMark.highWaterMark, waterMark.lowWaterMark);
    mgr.ExitOtherFunc();
    if (ret != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

} // namespace mempooling::outinterface