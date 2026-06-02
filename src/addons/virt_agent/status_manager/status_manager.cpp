/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "status_manager.h"

#include <condition_variable>
#include <numeric>
#include <queue>
#include <string>
#include "mempooling_module.h"
#include "resource_collect.h"
#include "vm_configuration.h"
#include "vm_string_util.h"
#include "vm_task_counter.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace vm::mempooling;

std::condition_variable StatusManager::migrateCv;
std::condition_variable StatusManager::borrowCv;
std::atomic_bool StatusManager::firstMigFlag(true);
VmResult StatusManager::Init()
{
    UBSE_LOG_INFO << "StatusManager::Init()";
    return VM_OK;
}

void StatusManager::LoadGlobalBorrowMap()
{
    UBSE_LOG_INFO << "LoadGlobalBorrowMap start.";
    auto &ResCol = ResourceCollect::GetInstance();
    if (const auto ret = ResourceCollect::InitGlobalBorrowMap(); ret != VM_OK) {
        UBSE_LOG_WARN << "Failed to init global borrow map.";
        return;
    }
    const auto globalBorrowMap = ResCol.GetGlobalBorrowMap();
    std::map<VMNodeLocInfo, std::vector<std::string>> nodeLocBorrowIdMap;
    for (const auto &[fst, snd] : globalBorrowMap) {
        if (snd.memMigrateStatus != MemMigrateStatus::READY_TO_MIGRATE) {
            continue;
        }
        if (nodeLocBorrowIdMap.find(snd.nodeLocInfo) != nodeLocBorrowIdMap.end()) {
            nodeLocBorrowIdMap[snd.nodeLocInfo].emplace_back(fst);
        } else {
            nodeLocBorrowIdMap[snd.nodeLocInfo] = {fst};
        }
    }
    for (const auto &[fst, snd] : nodeLocBorrowIdMap) {
        UBSE_LOG_INFO << "[LoadGlobalBorrowMap] nodeLoc = " << fst.toString()
                      << ", borrowIds = " << VectorUtil::VectorToString(snd);
        const std::vector<pid_t> pids = ResCol.GetPidsOnNuma(fst, "migratingOnly");
        const SrcMemoryBorrowParam borrowParam{
            .srcNid = fst.hostId,
            .srcSocketId = fst.socketId,
            .srcNumaId = fst.numaId,
        };
        const auto UBSRMRSMemReturn = MempoolingModule::UBSRMRSMemReturn();
        if (UBSRMRSMemReturn == nullptr) {
            return;
        }
        if (const auto res = UBSRMRSMemReturn(borrowParam, snd, pids); VmResultFail(res)) {
            UBSE_LOG_ERROR << "[LoadGlobalBorrowMap] Failed to return memory, " << FormatRetCode(res);
        } else {
            UBSE_LOG_INFO << "succeed to return memory.";
        }
    }
}

VmResult StatusManager::PerformMemoryBorrow(const SrcMemoryBorrowParam &borrowParam,
                                            const std::vector<uint64_t> &borrowSizes,
                                            MemBorrowExecuteResult &borrowResult)
{
    UBSE_LOG_INFO << "[borrow] task start, borrow_item_size = " << borrowSizes.size();
    try {
        const auto highWater = VmStringUtil::SafeStou16(VmConfiguration::GetInstance().GetHighWatermark());
        const auto lowWater = VmStringUtil::SafeStou16(VmConfiguration::GetInstance().GetLowWatermark());
        const WaterMark waterMark = {.highWaterMark = static_cast<uint16_t>(highWater),
                                     .lowWaterMark = static_cast<uint16_t>(lowWater)};

        const auto UBSRMRSMemBorrow = MempoolingModule::UBSRMRSMemBorrow();
        if (UBSRMRSMemBorrow == nullptr) {
            UBSE_LOG_ERROR << "[borrow] UBSRMRSMemBorrow is nullptr.";
            return VM_ERROR_NULLPTR;
        }

        auto start = std::chrono::steady_clock::now();
        const auto borrowRes = UBSRMRSMemBorrow(borrowParam, borrowSizes, waterMark, borrowResult);
        auto end = std::chrono::steady_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        // A maximum of 4GB memory can be borrowed at a time (see MAX_MEM_PERBORROW_MB), so overflow will not occur.
        uint64_t borrowMemorySize = std::accumulate(borrowSizes.begin(), borrowSizes.end(), static_cast<uint64_t>(0));
        if (VmResultFail(borrowRes)) {
            UBSE_LOG_ERROR << "[borrow] Failed to borrow memory, " << FormatRetCode(borrowRes);
            return VM_ERROR;
        } else {
            // Memory borrowing execution time observation point
            UBSE_LOG_INFO << "[borrow] nodeId=" << borrowParam.srcNid << ", execution time=" << duration.count()
                          << " microseconds, borrowMemSize=" << borrowMemorySize << " Bytes";
        }
        if (borrowResult.borrowIds.size() != borrowResult.presentNumaIds.size()) {
            UBSE_LOG_ERROR << "[borrow] The size of borrowIds is not equal to the size of presentNumaIds"
                           << ", borrowIds_size = " << borrowResult.borrowIds.size()
                           << ", presentNumaIds_size = " << borrowResult.presentNumaIds.size();
            return VM_ERROR;
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Watermark conversion error: " << e.what();
        return VM_ERROR;
    }
    return VM_OK;
}

void StatusManager::MemoryBorrowOperation(const VMNodeLocInfo &originNode, const std::vector<pid_t> &pids,
                                          const std::vector<uint64_t> &borrowSizes)
{
    UBSE_LOG_INFO << "[borrow] task start, borrow_item_size = " << borrowSizes.size();

    const SrcMemoryBorrowParam borrowParam = {
        .srcNid = originNode.hostId,
        .srcSocketId = originNode.socketId,
        .srcNumaId = originNode.numaId,
    };
    MemBorrowExecuteResult borrowResult;
    // 1. Memory borrowing (memory scheduling interface)
    auto ret = PerformMemoryBorrow(borrowParam, borrowSizes, borrowResult);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[borrow] borrow memory failed.";
        UBSE_LOG_INFO << "[borrow] task end.";
        return;
    }
    // Borrowing result, if borrowIds is empty, borrowing failed. Need to remove from the result set
    CleanEmptyBorrowRes(borrowResult);
    if (borrowResult.borrowIds.empty()) {
        UBSE_LOG_ERROR << "[borrow] borrow result is empty, borrow memory failed.";
        UBSE_LOG_INFO << "[borrow] task end.";
        return;
    }
    UBSE_LOG_INFO << "[borrow] succeed to borrow memory.";
    auto BorrowIdStatuses = GenerateBorrowIdStatuses(originNode, borrowResult);
    ResourceCollect::GetInstance().UpdateGlobalBorrowMap(BorrowIdStatuses);

    uint64_t borrowMemorySize = std::accumulate(borrowSizes.begin(), borrowSizes.end(), static_cast<uint64_t>(0));
    const vector<VMPresetParam>& vmPresetParam = ConvertToVmPresetParam(pids, originNode.numaId, borrowMemorySize);
    // 2. Memory migration (memory scheduling interface)
    const auto UBSRMRSMemMigrate = MempoolingModule::UBSRMRSMemMigrate();
    if (UBSRMRSMemMigrate == nullptr) {
        UBSE_LOG_ERROR << "[borrow] UBSRMRSMemMigrate is nullptr.";
        return;
    }
    uint32_t res = UBSRMRSMemMigrate(borrowParam, vmPresetParam, borrowResult);
    if (VmResultFail(res)) {
        UBSE_LOG_WARN << "[borrow] failed to migrate memory, " << FormatRetCode(res);
    } else {
        UBSE_LOG_INFO << "[borrow] succeed to migrate memory.";
        MigrateSuccessBorrowId(BorrowIdStatuses);
        ResourceCollect::GetInstance().UpdateGlobalBorrowMap(BorrowIdStatuses);
    }
    markFirstMigOperation();

    UBSE_LOG_INFO << "[borrow] task end.";
}

std::vector<BorrowIdStatus> StatusManager::GenerateBorrowIdStatuses(const VMNodeLocInfo &nodeLoc,
                                                                    const MemBorrowExecuteResult &borrowResult)
{
    std::vector<BorrowIdStatus> result;
    const size_t count = borrowResult.borrowIds.size();
    for (size_t i = 0; i < count; ++i) {
        BorrowIdStatus borrowIdStatus{.borrowId = borrowResult.borrowIds[i],
                                      .presentNumaId = borrowResult.presentNumaIds[i],
                                      .memMigrateStatus = MemMigrateStatus::READY_TO_MIGRATE,
                                      .nodeLocInfo = nodeLoc};
        result.emplace_back(borrowIdStatus);
    }
    return result;
}

void StatusManager::MigrateSuccessBorrowId(std::vector<BorrowIdStatus> &BorrowIdStatuses)
{
    for (auto &[borrowId, presentNumaId, memMigrateStatus, nodeLocInfo] : BorrowIdStatuses) {
        memMigrateStatus = MemMigrateStatus::MIGRATE_SUCCESS;
    }
}

void StatusManager::CleanEmptyBorrowRes(MemBorrowExecuteResult &result)
{
    // Create temporary containers to store new data
    vector<std::string> newBorrowIds;
    vector<uint16_t> newPresentNumaId;
    // Iterate through borrowIds and presentNumaIds. If a borrowId is not empty, add it to newBorrowIds and its
    // corresponding presentNumaId
    for (size_t i = 0; i < result.borrowIds.size() && i < result.presentNumaIds.size(); ++i) {
        if (!result.borrowIds[i].empty()) {
            newBorrowIds.push_back(result.borrowIds[i]);
            newPresentNumaId.push_back(result.presentNumaIds[i]); // Add to newPresentNumaId
        }
    }
    // Replace the old content with the new containers' content
    result.borrowIds.swap(newBorrowIds);
    result.presentNumaIds.swap(newPresentNumaId);
}

float StatusManager::CalculateMemMigrateRatio(int16_t numaId,uint64_t curBorrowMemorySize)
{
    double totalBorrowedMem = static_cast<double>(curBorrowMemorySize);
    double totalVMusedMem = 0;
    auto globalNumaInfoMap = ResourceCollect::GetInstance().GetGlobalSampleNumaInfo();
    auto globalNumaVmInfoMap = ResourceCollect::GetInstance().GetGlobalSampleVMInfo();
    for (const auto& [nodeLoc, numaInfo] : *globalNumaInfoMap) {
        if (nodeLoc.numaId != numaId) {
            continue;
        }
        totalBorrowedMem += static_cast<double>(numaInfo.numaMemBorrow);
    }
    for (const auto& [nodeLoc, vmMap] : globalNumaVmInfoMap) {
        if (nodeLoc.numaId != numaId) {
            continue;
        }
        for (const auto& [uuid, vmInfo] : vmMap) {
            auto memIt = vmInfo.numaMemInfo.find(numaId);
            if (memIt != vmInfo.numaMemInfo.end()) {
                totalVMusedMem += static_cast<double>(memIt->second.usedMem);
            }
        }
    }
    UBSE_LOG_DEBUG << "totalBorrowedMem = " << std::to_string(totalBorrowedMem) << ", totalVMusedMem = "
            << std::to_string(totalVMusedMem);
    float ratio = static_cast<float>(totalBorrowedMem / totalVMusedMem);
    return ratio;
}

std::vector<VMPresetParam> StatusManager::ConvertToVmPresetParam(const std::vector<pid_t>& pids, int16_t numaId,
    uint64_t curBorrowMemorySize)
{
    std::vector<VMPresetParam> vmPresetParams;
    try {
        vmPresetParams.reserve(pids.size());
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Memory allocation failed: " << e.what();
        return {};
    }
    float ratio = CalculateMemMigrateRatio(numaId, curBorrowMemorySize);
    UBSE_LOG_DEBUG << "Memory_migrate_ratio = " << std::to_string(ratio);
    for (pid_t pid : pids) {
        VMPresetParam param{};
        param.pid = pid;
        param.ratio = static_cast<uint16_t>(std::round(ratio));
        vmPresetParams.emplace_back(param);
    }
    return vmPresetParams;
}

VmResult StatusManager::MigrateByBorrowIdStatus(const SrcMemoryBorrowParam &srcMemoryBorrowParam,
                                                std::vector<BorrowIdStatus> &BorrowIdStatuses)
{
    UBSE_LOG_DEBUG << "[borrow] start migrate no used borrowIds.";
    const VMNodeLocInfo curNodeLoc = {.hostName = "",
                                      .hostId = srcMemoryBorrowParam.srcNid,
                                      .socketId = srcMemoryBorrowParam.srcSocketId,
                                      .numaId = srcMemoryBorrowParam.srcNumaId};
    const std::vector<pid_t> pids = ResourceCollect::GetInstance().GetPidsOnNuma(curNodeLoc, "withOutMigrating");
    if (pids.empty()) {
        UBSE_LOG_WARN << "[borrow] pids is empty.";
        return VM_ERROR;
    }

    onst vector<VMPresetParam>& vmPresetParam = ConvertToVmPresetParam(pids, srcMemoryBorrowParam.srcNumaId);
    const auto UBSRMRSMemMigrate = MempoolingModule::UBSRMRSMemMigrate();
    if (UBSRMRSMemMigrate == nullptr) {
        UBSE_LOG_ERROR << "[borrow] UBSRMRSMemMigrate is nullptr.";
        return VM_ERROR;
    }
    MemBorrowExecuteResult borrowResult;
    for (auto &[borrowId, presentNumaId, memMigrateStatus, nodeLocInfo] : BorrowIdStatuses) {
        borrowResult.borrowIds.emplace_back(borrowId);
        borrowResult.presentNumaIds.emplace_back(presentNumaId);
    }
    UBSE_LOG_INFO << "[borrow] borrowIds:" << VectorUtil::VectorToString(borrowResult.borrowIds);
    if (const auto res = UBSRMRSMemMigrate(srcMemoryBorrowParam, vmPresetParam, borrowResult); res != VM_OK) {
        UBSE_LOG_ERROR << "[borrow] migrate failed. waite for next alarm, " << FormatRetCode(res);
        return VM_ERROR;
    }
    for (auto &[borrowId, presentNumaId, memMigrateStatus, nodeLocInfo] : BorrowIdStatuses) {
        memMigrateStatus = MemMigrateStatus::MIGRATE_SUCCESS;
    }
    ResourceCollect::GetInstance().UpdateGlobalBorrowMap(BorrowIdStatuses);
    markFirstMigOperation();
    UBSE_LOG_INFO << "[borrow] migrate successfully.";
    return VM_OK;
}

VmResult StatusManager::ReturnByBorrowIdStatus(const SrcMemoryBorrowParam &srcMemoryBorrowParam,
                                               std::vector<BorrowIdStatus> &BorrowIdStatuses)
{
    UBSE_LOG_DEBUG << "[return] start return no used borrowIds.";
    const VMNodeLocInfo curNodeLoc = {.hostName = "",
                                      .hostId = srcMemoryBorrowParam.srcNid,
                                      .socketId = srcMemoryBorrowParam.srcSocketId,
                                      .numaId = srcMemoryBorrowParam.srcNumaId};
    const std::vector<pid_t> pids = ResourceCollect::GetInstance().GetPidsOnNuma(curNodeLoc, "migratingOnly");
    const auto UBSRMRSMemReturn = MempoolingModule::UBSRMRSMemReturn();
    if (UBSRMRSMemReturn == nullptr) {
        UBSE_LOG_ERROR << "[return] UBSRMRSMemReturn is nullptr.";
        return VM_ERROR;
    }
    std::vector<std::string> returnMemNames;
    for (auto &[borrowId, presentNumaId, memMigrateStatus, nodeLocInfo] : BorrowIdStatuses) {
        returnMemNames.emplace_back(borrowId);
    }
    UBSE_LOG_INFO << "[return] returnMemNames:" << VectorUtil::VectorToString(returnMemNames);
    if (const auto res = UBSRMRSMemReturn(srcMemoryBorrowParam, returnMemNames, pids); VmResultFail(res)) {
        UBSE_LOG_ERROR << "[return] return memory failed, waiter for next alarm, " << FormatRetCode(res);
        return VM_ERROR;
    }
    ResourceCollect::GetInstance().DeleteGlobalBorrowMap(returnMemNames);
    UBSE_LOG_INFO << "[return] return successfully.";
    return VM_OK;
}

void StatusManager::MemoryReturnOperation(EscapeAction &escapeAction)
{
    std::lock_guard lockGuard(returnMutex);
    VmTaskCounter::StartTask("memoryReturn");
    const std::string numaLoc = escapeAction.curNodeLoc.hostId + "/" +
                                std::to_string(escapeAction.curNodeLoc.socketId) + "/" +
                                std::to_string(escapeAction.curNodeLoc.numaId);
    UBSE_LOG_INFO << "[return] vm memory return for node=" << escapeAction.curNodeLoc.hostId
                  << ", numaId=" << escapeAction.curNodeLoc.numaId
                  << ", memNames=" << VectorUtil::VectorToString(escapeAction.returnMemNames);
    auto &ResourceCollect = ResourceCollect::GetInstance();
    const std::vector<pid_t> pids = ResourceCollect.GetPidsOnNuma(escapeAction.curNodeLoc, "migratingOnly");
    const SrcMemoryBorrowParam borrowParam = {
        .srcNid = escapeAction.curNodeLoc.hostId,
        .srcSocketId = escapeAction.curNodeLoc.socketId,
        .srcNumaId = escapeAction.curNodeLoc.numaId,
    };
    const auto UBSRMRSMemReturn = MempoolingModule::UBSRMRSMemReturn();
    if (UBSRMRSMemReturn == nullptr) {
        UBSE_LOG_ERROR << "[return] UBSRMRSMemReturn is nullptr.";
        return;
    }
    if (const auto res = UBSRMRSMemReturn(borrowParam, escapeAction.returnMemNames, pids); VmResultFail(res)) {
        UBSE_LOG_ERROR << "[return] failed to return memory, " << FormatRetCode(res);
    } else {
        ResourceCollect::GetInstance().DeleteGlobalBorrowMap(escapeAction.returnMemNames);
        UBSE_LOG_INFO << "[return] succeed to return memory.";
    }
    UBSE_LOG_INFO << "[return] task end.";
    VmTaskCounter::CompleteTask("memoryReturn");
}

void StatusManager::EscapeStrategyHandle(EscapeAction &escapeAction)
{
    switch (escapeAction.actionType) {
        case EscapeActionType::BORROW:
            UBSE_LOG_INFO << "The escape policy is memory borrow.";
            WhetherEnterBorrowQueue(escapeAction);
            break;
        case EscapeActionType::RETURN:
            UBSE_LOG_INFO << "The escape policy is memory return.";
            if (escapeAction.returnMemNames.empty()) {
                UBSE_LOG_WARN << "[return] returnMemNames is empty.";
                return;
            }
            UBSE_LOG_INFO << "[return] task start.";
            AddTaskFilterSet(escapeAction.curNodeLoc);
            MemoryReturnOperation(escapeAction);
            RemoveTaskFilterSet(escapeAction.curNodeLoc);
            break;
        case EscapeActionType::NOPE:
            UBSE_LOG_INFO << "The escape policy is none.";
            break;
        default:
            UBSE_LOG_WARN << "Unknown escape action type=" << static_cast<int>(escapeAction.actionType);
            return;
    }
}
void StatusManager::WhetherEnterBorrowQueue(const EscapeAction &escapeAction)
{
    AddTaskFilterSet(escapeAction.curNodeLoc);
    std::unique_lock<std::mutex> borrowLocLock(borrowMutex);
    g_borrowQueue.push(escapeAction);
    UBSE_LOG_INFO << "Memory borrow information enqueues, borrow_queue_size = " << g_borrowQueue.size();
    UBSE_LOG_INFO << "start to notify borrowCv.";
    borrowCv.notify_all();
}

void StatusManager::BorrowQueueOperation()
{
    auto &ResourceCollect = ResourceCollect::GetInstance();
    while (true) {
        std::unique_lock<std::mutex> borrowWaitLock(borrowMutex);
        borrowCv.wait(borrowWaitLock, [] { return !g_borrowQueue.empty() || VmConfiguration::exitFlag.load(); });
        if (VmConfiguration::exitFlag.load()) {
            UBSE_LOG_INFO << "[borrow] exit borrow queue success, flag = " << VmConfiguration::exitFlag.load();
            return;
        }
        UBSE_LOG_INFO << "[borrow] notify borrowCv.";

        auto [actionType, strategyTips, curNodeLoc, returnMemNames, borrowSizes] = g_borrowQueue.front();
        g_borrowQueue.pop();
        UBSE_LOG_INFO << "[borrow] Memory borrow information is dequeued, borrow_queue_size = " << g_borrowQueue.size();

        std::lock_guard lockGuard(ResourceCollect::mAllLock);
        std::vector<pid_t> pids = ResourceCollect.GetPidsOnNuma(curNodeLoc, "withOutMigrating");
        if (pids.empty()) {
            RemoveTaskFilterSet(curNodeLoc);
            UBSE_LOG_WARN << "[borrow] pid is empty.";
            continue;
        }
        VmTaskCounter::StartTask("memoryBorrow");
        MemoryBorrowOperation(curNodeLoc, pids, borrowSizes);
        RemoveTaskFilterSet(curNodeLoc);
        VmTaskCounter::CompleteTask("memoryBorrow");
    }
}

void StatusManager::AddTaskFilterSet(const VMNodeLocInfo &nodeLocInfo)
{
    WriteLocker lock(&taskFilterSetLock_);
    g_taskFilterSet.insert(nodeLocInfo.toString());
}

void StatusManager::RemoveTaskFilterSet(const VMNodeLocInfo &nodeLocInfo)
{
    WriteLocker lock(&taskFilterSetLock_);
    g_taskFilterSet.erase(nodeLocInfo.toString());
}

bool StatusManager::StillInTask(const std::string &hostId, const int16_t &socketId, const int16_t &numaId)
{
    ReadLocker lock(&taskFilterSetLock_);
    std::ostringstream oss;
    oss << "\"" << hostId << "\"," << socketId << "," << numaId;
    return g_taskFilterSet.find(oss.str()) != g_taskFilterSet.end();
}
} // namespace vm