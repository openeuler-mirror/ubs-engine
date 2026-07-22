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

#include "alarm_handler.h"

#include <rapidjson/document.h>
#include <securec.h>
#include <ubse_election.h>
#include <ubse_error.h>
#include <ubse_event.h>
#include <ubse_logger.h>
#include <ubse_ras.h>
#include <regex>
#include "escape_algorithm_helper.h"
#include "resource_collect.h"
#include "resource_query.h"
#include "status_manager.h"
#include "vm_string_util.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace rapidjson;
using namespace ubse::log;
using namespace ubse::event;
using namespace ubse::ras;

std::mutex AlarmHandler::alarmLock{};

VmResult AlarmHandler::Init()
{
    UBSE_LOG_INFO << "AlarmEventHandler init start";

    // Subscribe OOM events directly via RAS fault handler framework
    const auto ret = RegisterAlarmFaultHandler(ALARM_OOM_EVENT, "virt_agent_oom", AlarmHandler::OomAdapter,
                                               AlarmHandlerPriority::HIGH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register oom alarm fault handler failed," << FormatRetCode(ret);
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "AlarmEventHandler init success";
    return VM_OK;
}

VmResult AlarmHandler::GenAlarmNumaInfo(const Notify& notify, std::vector<UbsVirtNumaMemoryDebtInfo>& debtInfos,
                                        AlarmNumaInfo& alarmNumaInfo)
{
    UBSE_LOG_DEBUG << "AlarmNumaInfo generated start.";
    // get hostname
    std::vector<UbseNodeNumaInfo> numaNodeInfoList{};
    auto res = UbseGetNodeNumaInfoByNodeId(notify.nodeId, numaNodeInfoList);
    if (res != UBSE_OK || numaNodeInfoList.empty()) {
        UBSE_LOG_ERROR << "UbseGetNodeNumaInfoByNodeId failed, res=" << static_cast<uint32_t>(res);
        return VM_ERROR;
    }

    for (auto& numaNodeInfo : numaNodeInfoList) {
        if (numaNodeInfo.numaId == notify.numaId) {
            alarmNumaInfo.numaLoc.hostName = numaNodeInfo.hostName;
        }
    }
    alarmNumaInfo.numaLoc.hostId = notify.nodeId;
    alarmNumaInfo.numaLoc.socketId = notify.socketId;
    alarmNumaInfo.numaLoc.numaId = notify.numaId;
    // get borrowItemInfo
    for (auto& debtInfo : debtInfos) {
        if (debtInfo.numaId == notify.numaId) {
            BorrowItem borrowItem{};
            borrowItem.requestSize.push_back(debtInfo.size);
            borrowItem.name = debtInfo.name;
            alarmNumaInfo.borrowItemInfo.borrowItem.emplace_back(borrowItem);
        }
    }
    alarmNumaInfo.oomEventFlag = notify.oomEventFlag;
    UBSE_LOG_DEBUG << "AlarmNumaInfo generated successfully, borrowItem size="
                   << alarmNumaInfo.borrowItemInfo.borrowItem.size();
    return VM_OK;
}

VmResult AlarmHandler::ConvertUbseDebtInfosToVirtDebtInfos(const std::vector<UbseNumaMemoryImportDebtInfo>& debtInfos,
                                                           std::vector<UbsVirtNumaMemoryDebtInfo>& virtDebtInfos)
{
    try {
        virtDebtInfos.reserve(debtInfos.size());
        size_t i = 0;
        for (const auto& debtInfo : debtInfos) {
            int16_t tempNumaId;
            // Copy first 2 bytes from usrInfo to tempNumaId (business logic alignment with rmrs)
            if (memcpy_s(&tempNumaId, sizeof(tempNumaId), debtInfo.usrInfo, sizeof(tempNumaId)) != EOK) {
                UBSE_LOG_ERROR << "memcpy_s failed.";
                return VM_ERROR;
            }
            UBSE_LOG_DEBUG << "num " << i << ", tempNumaId=" << tempNumaId << ", borrowId=" << debtInfo.name;
            virtDebtInfos.emplace_back(debtInfo, tempNumaId);
            i++;
        }
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Failed to convert debt infos. Error is " << e.what();
        return VM_ERROR;
    }
    UBSE_LOG_DEBUG << "Input debtInfo size=" << debtInfos.size();
    UBSE_LOG_DEBUG << "Output virtDebtInfo size=" << virtDebtInfos.size();
    return VM_OK;
}

VmResult AlarmHandler::GetVirtDebtInfos(std::vector<UbsVirtNumaMemoryDebtInfo>& virtDebtInfos)
{
    std::vector<UbseNumaMemoryImportDebtInfo> debtInfos{};
    auto res = UbseGetNumaMemImportDebtInfoWithLocalNode(debtInfos);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get numaMemDebtInfo by nodeId failed, res=" << static_cast<uint32_t>(res);
        return VM_ERROR;
    }

    if (ConvertUbseDebtInfosToVirtDebtInfos(debtInfos, virtDebtInfos) != VM_OK) {
        UBSE_LOG_ERROR << "Failed to convert debt infos.";
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult AlarmHandler::ParseOomMsg(const std::string& input, uint16_t& numaCount, std::vector<uint16_t>& numaIds,
                                   uint8_t& reason)
{
    if (input.empty()) {
        UBSE_LOG_ERROR << "Empty input";
        return VM_ERROR;
    }
    std::vector<std::string> parts;
    std::stringstream ss(input);
    std::string token;
    while (std::getline(ss, token, '_')) {
        parts.push_back(token);
    }

    if (parts.size() < 3) {
        UBSE_LOG_ERROR << "Format error";
        return VM_ERROR;
    }

    try {
        numaCount = std::stoi(parts[0]);
        if (const auto actualNumaIds = parts.size() - 2; actualNumaIds != numaCount) {
            UBSE_LOG_ERROR << "Mismatch: declared " << numaCount << " numaIds but got " << actualNumaIds;
            return VM_ERROR;
        }
        for (size_t i = 1; i <= numaCount; ++i) {
            numaIds.push_back(std::stoi(parts[i]));
        }
        reason = std::stoi(parts.back());
    } catch (const std::exception e) {
        UBSE_LOG_ERROR << "Parse oom msg error, " << e.what();
        return VM_ERROR;
    }
    return VM_OK;
}

uint32_t AlarmHandler::OomAdapter(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo)
{
    if (alarmFaultEvent != ALARM_OOM_EVENT) {
        UBSE_LOG_ERROR << "Unexpected alarm fault event type=" << alarmFaultEvent;
        return static_cast<uint32_t>(VM_ERROR);
    }
    UBSE_LOG_INFO << "[oom] receive oom event, info=" << faultInfo;

    uint16_t numaCount = 0;
    std::vector<uint16_t> numaIds{};
    uint8_t reason = 0;
    auto ret = ParseOomMsg(faultInfo, numaCount, numaIds, reason);
    if (ret != VM_OK) {
        return static_cast<uint32_t>(VM_ERROR);
    }
    if (reason != HUGE_PAGE_OOM) {
        UBSE_LOG_WARN << "Not hugepage oom , ignore.";
        return VM_OK;
    }
    // Currently, only one numaId will be sent, hardcode
    const auto oomNumaId = numaIds[0];

    // Get current node ID
    ubse::election::UbseRoleInfo curNodeInfo{};
    if (const auto ret = ubse::election::UbseGetCurrentNodeInfo(curNodeInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[oom] UbseGetCurrentNodeInfo failed, " << FormatRetCode(ret);
        return static_cast<uint32_t>(VM_ERROR);
    }

    // Look up socketId from NUMA topology
    std::vector<UbseNodeNumaInfo> numaNodeInfoList{};
    if (const auto res = UbseGetNodeNumaInfoByNodeId(curNodeInfo.nodeId, numaNodeInfoList);
        res != UBSE_OK || numaNodeInfoList.empty()) {
        UBSE_LOG_ERROR << "[oom] lookup numa info failed, res=" << static_cast<uint32_t>(res);
        return static_cast<uint32_t>(VM_ERROR);
    }
    unsigned int socketId = 0;
    bool found = false;
    for (const auto& numaInfo : numaNodeInfoList) {
        if (static_cast<int32_t>(numaInfo.numaId) == oomNumaId) {
            socketId = numaInfo.socketId;
            found = true;
            break;
        }
    }
    if (!found) {
        UBSE_LOG_ERROR << "[oom] no socket info found for numaId=" << oomNumaId;
        return static_cast<uint32_t>(VM_ERROR);
    }

    // Construct Notify (OOM specific, waterNotify=false)
    return static_cast<uint32_t>(OomEventHandler(Notify{
        .nodeId = curNodeInfo.nodeId,
        .socketId = socketId,
        .numaId = static_cast<unsigned int>(oomNumaId),
        .waterNotify = false,
        .oomEventFlag = true,
    }));
}

VmResult AlarmHandler::OomEventHandler(const Notify& notify)
{
    UBSE_LOG_INFO << "[oom] start oom event handling, nodeId=" << notify.nodeId << ", socketId=" << notify.socketId
                  << ", numaId=" << notify.numaId;

    // Double-check StillInTask (same as MemNotifyEventHandler).
    // Instead of returning VM_OK directly, await concurrent borrow or wait for non-borrow completion.
    if (StatusManager::StillInTask(notify.nodeId, notify.socketId, notify.numaId)) {
        UBSE_LOG_DEBUG << "[oom] StillInTask (pre-lock). nodeId=" << notify.nodeId << ", socketId=" << notify.socketId
                       << ", numaId=" << notify.numaId;
        const auto shFuture =
            StatusManager::GetInFlightBorrowSharedFuture(notify.nodeId, notify.socketId, notify.numaId);
        if (shFuture != nullptr) {
            const auto borrowResult = shFuture->get();
            UBSE_LOG_DEBUG << "[oom] waited for concurrent borrow result=" << static_cast<int>(borrowResult);
            return borrowResult;
        }
        // Non-borrow task: wait for task completion, then fall through to post-lock check
        StatusManager::WaitForTaskCompletion(notify.nodeId, notify.socketId, notify.numaId);
    }
    std::lock_guard<std::mutex> lockGuard(alarmLock);
    if (StatusManager::StillInTask(notify.nodeId, notify.socketId, notify.numaId)) {
        UBSE_LOG_DEBUG << "[oom] StillInTask (post-lock). nodeId=" << notify.nodeId << ", socketId=" << notify.socketId
                       << ", numaId=" << notify.numaId;
        const auto shFuture =
            StatusManager::GetInFlightBorrowSharedFuture(notify.nodeId, notify.socketId, notify.numaId);
        if (shFuture != nullptr) {
            const auto borrowResult = shFuture->get();
            UBSE_LOG_DEBUG << "[oom] waited for concurrent borrow result=" << static_cast<int>(borrowResult);
            return borrowResult;
        }
        StatusManager::WaitForTaskCompletion(notify.nodeId, notify.socketId, notify.numaId);
    }

    return ProcessOomActions(notify);
}

VmResult AlarmHandler::ProcessOomActions(const Notify& notify)
{
    // Get debt infos
    std::vector<UbsVirtNumaMemoryDebtInfo> virtDebtInfos{};
    auto ret = GetVirtDebtInfos(virtDebtInfos);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[oom] Failed to get debt infos.";
        return ret;
    }

    // Generate alarm NUMA info
    AlarmNumaInfo alarmNumaInfo{};
    ret = GenAlarmNumaInfo(notify, virtDebtInfos, alarmNumaInfo);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[oom] Failed to generate alarmNumaInfo.";
        return ret;
    }
    alarmNumaInfo.oomEventFlag = true;

    // Handle unused borrow IDs (dispatches sync return/migrate directly)
    if (!HandlerNoUsedBorrowIds(alarmNumaInfo, WatermarkWarningType::HIGH_WATERMARK)) {
        UBSE_LOG_INFO << "[oom] handled by no-used-borrow-id dispatch, done.";
        return VM_OK;
    }

    // Set up borrow completion state only when BORROW is possible
    auto borrowState = std::make_shared<BorrowCompletionState>();
    borrowState->future = borrowState->promise.get_future().share(); // single get_future(), store shared copy
    auto future = borrowState->future;                               // local copy for waiting
    StatusManager::SetBorrowCompletionState(std::move(borrowState));

    // Execute escape algorithm and dispatch (may be sync or async via queue)
    ret = AlarmEventHandler(alarmNumaInfo, virtDebtInfos, WatermarkWarningType::HIGH_WATERMARK);
    if (ret != VM_OK) {
        StatusManager::GetAndClearBorrowCompletionState();
        return ret;
    }

    // If borrow completion state was consumed by WhetherEnterBorrowQueue,
    // wait for the async borrow to complete; otherwise nothing to wait for.
    if (const auto remainingState = StatusManager::GetAndClearBorrowCompletionState();
        remainingState == nullptr && future.valid()) {
        UBSE_LOG_DEBUG << "[oom] waiting for async borrow result...";
        const auto borrowResult = future.get();
        UBSE_LOG_INFO << "[oom] async borrow completed, result=" << static_cast<int>(borrowResult);
        if (borrowResult == VM_OK) {
            // Asynchronous migration tasks need to wait by default
            std::this_thread::sleep_for(std::chrono::seconds(DEFAULT_ASYNC_MIGRATE_WAIT_SECONDES));
        }
        return borrowResult;
    }

    UBSE_LOG_DEBUG << "[oom] oom event handling done (no async borrow).";
    return VM_OK;
}

VmResult AlarmHandler::MemNotifyEventHandler(std::string& eventId, std::string& eventMessage)
{
    WatermarkWarningType eventType;
    if (eventId == UBSE_MEM_RETURN_EVENT_ID) {
        eventType = WatermarkWarningType::LOW_WATERMARK;
    } else if (eventId == UBSE_MEM_BORROW_EVENT_ID) {
        eventType = WatermarkWarningType::HIGH_WATERMARK;
    } else if (eventId == UBSE_MEM_CLEAR_EVENT_ID) {
        eventType = WatermarkWarningType::CLEAR_BORROW;
    } else {
        UBSE_LOG_ERROR << "Invalid eventId=" << eventId;
        return VM_ERROR_INVAL;
    }
    Notify notify{};
    VmResult ret = MemHandler::TransNotify(eventMessage, notify);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to parse notify messages=" << eventMessage;
        return ret;
    }

    if (StatusManager::StillInTask(notify.nodeId, notify.socketId, notify.numaId)) {
        UBSE_LOG_DEBUG << "[alarm] StillInTask. notify=" << notify.ToJson();
        return VM_OK;
    }
    std::lock_guard<std::mutex> lockGuard(alarmLock);
    if (StatusManager::StillInTask(notify.nodeId, notify.socketId, notify.numaId)) {
        UBSE_LOG_DEBUG << "[alarm] StillInTask. notify=" << notify.ToJson();
        return VM_OK;
    }
    // get debt infos about alarm numa
    std::vector<UbsVirtNumaMemoryDebtInfo> virtDebtInfos{};
    ret = GetVirtDebtInfos(virtDebtInfos);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to get debt infos.";
        return ret;
    }
    UBSE_LOG_DEBUG << "Get virtDebtInfo size=" << virtDebtInfos.size();
    AlarmNumaInfo alarmNumaInfo{};
    ret = GenAlarmNumaInfo(notify, virtDebtInfos, alarmNumaInfo);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to generate alarmNumaInfo. ret=" << ret;
        return ret;
    }
    // Unused borrowed memory is processed. borrow/oom, and return.
    if (!HandlerNoUsedBorrowIds(alarmNumaInfo, eventType)) {
        return VM_OK;
    }

    // For HIGH_WATERMARK (borrow) events, set up completion state so OomEventHandler
    // can wait for the borrow result via g_inFlightBorrowMap. MemNotifyEventHandler
    // itself does not wait — it just records the state for cross-OOM visibility.
    if (eventType == WatermarkWarningType::HIGH_WATERMARK) {
        auto borrowState = std::make_shared<BorrowCompletionState>();
        borrowState->future = borrowState->promise.get_future().share();
        StatusManager::SetBorrowCompletionState(std::move(borrowState));
    }

    const auto alarmRet = AlarmEventHandler(alarmNumaInfo, virtDebtInfos, eventType);

    // Clean up TLS borrow state if it wasn't consumed by WhetherEnterBorrowQueue
    // (strategy returned NOPE/RETURN instead of BORROW, or AlarmEventHandler failed).
    if (eventType == WatermarkWarningType::HIGH_WATERMARK) {
        StatusManager::GetAndClearBorrowCompletionState();
    }

    return alarmRet;
}

VmResult AlarmHandler::BorrowClearEventHandler(const AlarmNumaInfo& alarmNumaInfo)
{
    EscapeAction escapeAction;
    escapeAction.actionType = EscapeActionType::RETURN;
    escapeAction.curNodeLoc = alarmNumaInfo.numaLoc;
    BorrowItemInfo borrowItemInfo = alarmNumaInfo.borrowItemInfo;
    if (borrowItemInfo.borrowItem.empty()) {
        UBSE_LOG_ERROR << "[alarm] Numa borrowed is none, numaId=" << alarmNumaInfo.numaLoc.numaId
                       << ", hostId=" << alarmNumaInfo.numaLoc.hostId;
        return VM_INVALID_PARAM_ERROR;
    }
    escapeAction.strategyTips.emplace_back(StrategyTip::NOPE);
    for (size_t i = 0; i < borrowItemInfo.borrowItem.size(); i++) {
        escapeAction.returnMemNames.emplace_back(borrowItemInfo.borrowItem[i].name);
    }
    UBSE_LOG_INFO << "[alarm] Build borrow clear strategy=" << escapeAction.ToString();
    StatusManager::GetInstance().EscapeStrategyHandle(escapeAction);
    UBSE_LOG_INFO << "[alarm] Handle borrow clear strategy end.";
    return VM_OK;
}

bool AlarmHandler::HandlerNoUsedBorrowIds(const AlarmNumaInfo& alarmNumaInfo, WatermarkWarningType eventType)
{
    if (eventType == WatermarkWarningType::CLEAR_BORROW) {
        return true;
    }
    auto borrowMap = ResourceCollect::GetInstance().GetGlobalBorrowMap();
    mempooling::SrcMemoryBorrowParam borrowParam{};
    std::vector<BorrowIdStatus> noUsedBorrowIds{};
    std::vector<std::string> borrowIdsInMem = GenVectorByBorrowItem(alarmNumaInfo);
    VMNodeLocInfo alarmNumaLoc = alarmNumaInfo.numaLoc;
    // Resolve the problem that the recorded status is inconsistent with the actual status during the first startup.
    // Perform a memory migration.
    bool isFirstMig = (eventType == WatermarkWarningType::HIGH_WATERMARK && StatusManager::isFirstMigOperation());
    for (auto& [borrowId, borrowIdStatus] : borrowMap) {
        if (borrowIdStatus.nodeLocInfo.hostId == alarmNumaLoc.hostId &&
            borrowIdStatus.nodeLocInfo.numaId == alarmNumaLoc.numaId &&
            std::find(borrowIdsInMem.begin(), borrowIdsInMem.end(), borrowId) != borrowIdsInMem.end()) {
            if (isFirstMig || borrowIdStatus.memMigrateStatus == MemMigrateStatus::READY_TO_MIGRATE) {
                noUsedBorrowIds.emplace_back(borrowIdStatus);
                borrowParam.srcNid = borrowIdStatus.nodeLocInfo.hostId;
                borrowParam.srcSocketId = borrowIdStatus.nodeLocInfo.socketId;
                borrowParam.srcNumaId = borrowIdStatus.nodeLocInfo.numaId;
            }
        }
    }
    if (noUsedBorrowIds.empty()) {
        return true;
    }
    VMNodeLocInfo curNodeLocInfo{
        .hostId = alarmNumaLoc.hostId,
        .socketId = alarmNumaLoc.socketId,
        .numaId = alarmNumaLoc.numaId,
    };

    UBSE_LOG_INFO << "[alarm] Start to handle no used borrowIds.";
    curNodeLocInfo.hostName = alarmNumaLoc.hostName;
    StatusManager::AddTaskFilterSet(curNodeLocInfo);
    if (eventType == WatermarkWarningType::LOW_WATERMARK) {
        StatusManager::ReturnByBorrowIdStatus(borrowParam, noUsedBorrowIds);
    } else {
        StatusManager::MigrateByBorrowIdStatus(borrowParam, noUsedBorrowIds);
    }
    StatusManager::RemoveTaskFilterSet(curNodeLocInfo);
    return false;
}

std::vector<std::string> AlarmHandler::GenVectorByBorrowItem(const AlarmNumaInfo& alarmNumaInfo)
{
    std::vector<std::string> result;
    for (const auto& [exportLocNum, exportLocInfo, requestSize, exportMemId, importMemId, name] :
         alarmNumaInfo.borrowItemInfo.borrowItem) {
        result.emplace_back(name);
    }
    return result;
}

VmResult AlarmHandler::AlarmEventHandler(AlarmNumaInfo& alarmNumaInfo,
                                         std::vector<UbsVirtNumaMemoryDebtInfo>& debtInfos,
                                         WatermarkWarningType eventType)
{
    bool isMemReturn = eventType == WatermarkWarningType::LOW_WATERMARK;
    bool oomEventFlag = alarmNumaInfo.oomEventFlag;
    bool isMemBorrowClear = eventType == WatermarkWarningType::CLEAR_BORROW;
    VMNodeLocInfo alarmNumaLoc = alarmNumaInfo.numaLoc;
    UBSE_LOG_INFO << "[alarm] Received alarm, numa borrowItem.size()=" << alarmNumaInfo.borrowItemInfo.borrowItem.size()
                  << ", numaId=" << alarmNumaLoc.numaId << ", socketId=" << alarmNumaLoc.socketId
                  << ", hostId=" << alarmNumaLoc.hostId << ", isMemReturn=" << isMemReturn
                  << ", oomEventFlag=" << oomEventFlag << ", isMemBorrowClear=" << isMemBorrowClear;
    // borrow clear event
    if (isMemBorrowClear) {
        return BorrowClearEventHandler(alarmNumaInfo);
    }
    // get globalNumaInfoMap
    GlobalNumaInfoMap globalNumaInfoMap = GetGlobalResource(alarmNumaInfo, debtInfos);
    // Check whether the return operation is required.
    if (isMemReturn && globalNumaInfoMap[alarmNumaLoc].numaMemBorrow == 0) {
        return VM_INVALID_PARAM_ERROR;
    }
    alarmNumaInfo.isMemReturn = isMemReturn;
    // add vmBasicInfos
    std::unordered_map<std::string, VMBasicInfo> sampleVmInfoOfNuma =
        ResourceCollect::GetInstance().GetVMInfo(alarmNumaLoc);
    alarmNumaInfo.vmBasicInfos = sampleVmInfoOfNuma;

    EscapeAction escapeActionFromStrategy;
    UBSE_LOG_INFO << "[alarm] Get vm strategy start.";
    EscapeAlgorithmFunc getStrategyAlgorithm = EscapeAlgorithmModule::GetStrategyAlgorithm();
    if (getStrategyAlgorithm == nullptr) {
        return VM_ERROR;
    }
    StrategyConfig strategyConf;
    EscapeAlgorithmHelper::GetStrategyConf(strategyConf);
    auto res = getStrategyAlgorithm(strategyConf, alarmNumaInfo, globalNumaInfoMap, escapeActionFromStrategy);
    if (res != 0) {
        UBSE_LOG_WARN << "[alarm] getStrategyAlgorithm failed, res=" << res;
    }
    UBSE_LOG_INFO << "[alarm] getStrategyAlgorithm succeeded.";
    StatusManager::GetInstance().EscapeStrategyHandle(escapeActionFromStrategy);
    return VM_OK;
}

GlobalNumaInfoMap AlarmHandler::GetGlobalResource(const AlarmNumaInfo& alarmNumaInfo,
                                                  std::vector<UbsVirtNumaMemoryDebtInfo>& debtInfos)
{
    vector<HostVmDomainInfo> hostVmDomainInfoList{};
    vector<HostNumaCpuInfo> hostNumaCpuInfoList{};
    HostVmDomainInfo hostVmDomainInfo{};
    HostNumaCpuInfo hostNumaCpuInfo{};

    ResourceQuery::GetLocalHostVmCollectData(hostVmDomainInfo, hostNumaCpuInfo);
    hostVmDomainInfoList.push_back(hostVmDomainInfo);
    hostNumaCpuInfoList.push_back(hostNumaCpuInfo);

    std::lock_guard<std::mutex> lockGuard(ResourceCollect::mAllLock);
    ResourceCollect::GetInstance().VmResourceCollectInfoHandle(hostVmDomainInfoList, hostNumaCpuInfoList);
    ResourceCollect::GetInstance().FillGlobalWithNumaMemInfo(alarmNumaInfo, debtInfos);

    return ResourceCollect::GetInstance().GetGlobalSampleNumaInfo();
}

enum NodeLocLevel : uint32_t
{
    HOSTID = 0,
    SOCKETID,
    NUMAID
};

VmResult ParseNodeLocInfo(const std::string& nodeLocInfoStr, Notify& notify)
{
    std::stringstream ss(nodeLocInfoStr);
    std::string level;
    uint32_t i = 0;

    if (nodeLocInfoStr.empty()) {
        UBSE_LOG_ERROR << "empty nodeLoc str.";
        return VM_ERROR_INVAL;
    }

    while (std::getline(ss, level, '/')) {
        try {
            if (i == NodeLocLevel::HOSTID) {
                notify.nodeId = level;
            } else if (i == NodeLocLevel::SOCKETID) {
                notify.socketId = VmStringUtil::SafeStoi16(level);
            } else if (i == NodeLocLevel::NUMAID) {
                notify.numaId = VmStringUtil::SafeStoi16(level);
            } else {
                UBSE_LOG_WARN << "invalid nodeLoc str=" << nodeLocInfoStr;
                break;
            }
        } catch (std::exception& e) {
            UBSE_LOG_ERROR << "VM parse nodeLoc info failed, error=" << e.what();
            return VM_ERROR;
        }

        i++;
    }
    return VM_OK;
}

VmResult ParseNodeLocInfoJson(Value& pstJson, const std::string& key, Notify& notify)
{
    if (!pstJson.HasMember(key.c_str()) || !pstJson[key.c_str()].IsString()) {
        UBSE_LOG_ERROR << "Failed to get " << key;
        return VM_ERROR;
    }
    std::string nodeLocInfoStr = pstJson[key.c_str()].GetString();

    auto ret = ParseNodeLocInfo(nodeLocInfoStr, notify);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to parse numaLoc=" << nodeLocInfoStr;
        return ret;
    }
    return VM_OK;
}
} // namespace vm