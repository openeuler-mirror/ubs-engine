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
#include <ubse_error.h>
#include <ubse_event.h>
#include <ubse_logger.h>
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

std::mutex AlarmHandler::alarmLock{};

VmResult AlarmHandler::Init()
{
    std::string eventId;
    VmResult ret;

    UBSE_LOG_INFO << "AlarmEventHandler init start";
    eventId = UBSE_MEM_BORROW_EVENT_ID;
    ret = UbseSubEvent(eventId, AlarmHandler::MemNotifyEventHandler, HIGH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem high event sub failed," << FormatRetCode(ret);
        return VM_ERROR;
    }
    eventId = UBSE_MEM_RETURN_EVENT_ID;
    ret = UbseSubEvent(eventId, AlarmHandler::MemNotifyEventHandler, MEDIUM);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem low event sub failed," << FormatRetCode(ret);
        return VM_ERROR;
    }
    eventId = UBSE_MEM_CLEAR_EVENT_ID;
    ret = UbseSubEvent(eventId, AlarmHandler::MemNotifyEventHandler, MEDIUM);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem clear event sub failed," << FormatRetCode(ret);
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult AlarmHandler::GenAlarmNumaInfo(const Notify &notify, std::vector<UbsVirtNumaMemoryDebtInfo> &debtInfos,
                                        AlarmNumaInfo &alarmNumaInfo)
{
    UBSE_LOG_DEBUG << "AlarmNumaInfo generated start.";
    // get hostname
    std::vector<UbseNodeNumaInfo> numaNodeInfoList{};
    auto res = UbseGetNodeNumaInfoByNodeId(notify.nodeId, numaNodeInfoList);
    if (res != UBSE_OK || numaNodeInfoList.empty()) {
        UBSE_LOG_ERROR << "UbseGetNodeNumaInfoByNodeId failed, res=" << static_cast<uint32_t>(res);
        return VM_ERROR;
    }

    for (auto &numaNodeInfo : numaNodeInfoList) {
        if (numaNodeInfo.numaId == notify.numaId) {
            alarmNumaInfo.numaLoc.hostName = numaNodeInfo.hostName;
        }
    }
    alarmNumaInfo.numaLoc.hostId = notify.nodeId;
    alarmNumaInfo.numaLoc.socketId = notify.socketId;
    alarmNumaInfo.numaLoc.numaId = notify.numaId;
    // get borrowItemInfo
    for (auto &debtInfo : debtInfos) {
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

VmResult AlarmHandler::ConvertUbseDebtInfosToVirtDebtInfos(const std::vector<UbseNumaMemoryImportDebtInfo> &debtInfos,
                                                           std::vector<UbsVirtNumaMemoryDebtInfo> &virtDebtInfos)
{
    try {
        virtDebtInfos.reserve(debtInfos.size());
        size_t i = 0;
        for (const auto &debtInfo : debtInfos) {
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
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Failed to convert debt infos. Error is " << e.what();
        return VM_ERROR;
    }
    UBSE_LOG_DEBUG << "Input debtInfo size=" << debtInfos.size();
    UBSE_LOG_DEBUG << "Output virtDebtInfo size=" << virtDebtInfos.size();
    return VM_OK;
}

VmResult AlarmHandler::GetVirtDebtInfos(std::vector<UbsVirtNumaMemoryDebtInfo> &virtDebtInfos)
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

VmResult AlarmHandler::MemNotifyEventHandler(std::string &eventId, std::string &eventMessage)
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
    // oom
    if (!notify.waterNotify && ParseOomMessage(eventMessage, notify) != VM_OK) {
        UBSE_LOG_ERROR << "Failed to parse oom message.";
        return VM_ERROR;
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
    return AlarmEventHandler(alarmNumaInfo, virtDebtInfos, eventType);
}

VmResult AlarmHandler::BorrowClearEventHandler(const AlarmNumaInfo &alarmNumaInfo)
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

bool AlarmHandler::HandlerNoUsedBorrowIds(const AlarmNumaInfo &alarmNumaInfo, WatermarkWarningType eventType)
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
    for (auto &[borrowId, borrowIdStatus] : borrowMap) {
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

std::vector<std::string> AlarmHandler::GenVectorByBorrowItem(const AlarmNumaInfo &alarmNumaInfo)
{
    std::vector<std::string> result;
    for (const auto &[exportLocNum, exportLocInfo, requestSize, exportMemId, importMemId, name] :
         alarmNumaInfo.borrowItemInfo.borrowItem) {
        result.emplace_back(name);
    }
    return result;
}

VmResult AlarmHandler::AlarmEventHandler(AlarmNumaInfo &alarmNumaInfo,
                                         std::vector<UbsVirtNumaMemoryDebtInfo> &debtInfos,
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

GlobalNumaInfoMap AlarmHandler::GetGlobalResource(const AlarmNumaInfo &alarmNumaInfo,
                                                  std::vector<UbsVirtNumaMemoryDebtInfo> &debtInfos)
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
    auto globalNumaInfo = ResourceCollect::GetInstance().GetGlobalSampleNumaInfo();
    // add numaMemBorrow, numaMemLend, numaLoc
    FillGlobalWithNumaMemInfo(alarmNumaInfo, debtInfos, *globalNumaInfo);

    return *globalNumaInfo;
}

void AlarmHandler::FillGlobalWithNumaMemInfo(const AlarmNumaInfo &alarmNumaInfo,
                                             std::vector<UbsVirtNumaMemoryDebtInfo> &debtInfos,
                                             GlobalNumaInfoMap &globalNumaInfoMapIn)
{
    if (alarmNumaInfo.numaLoc.hostId == "") {
        UBSE_LOG_ERROR << "alarmNumaInfo is invalid.";
        return;
    }
    if (debtInfos.empty()) {
        UBSE_LOG_DEBUG << "DebtInfos is empty.";
    }

    UBSE_LOG_INFO << "Fill global numa info for escape strategy.";
    VMNodeLocInfo alarmNumaLoc = alarmNumaInfo.numaLoc;
    // add numaMemBorrow
    uint64_t totalBorrow = 0;
    for (auto debtInfo : debtInfos) {
        if (debtInfo.numaId == alarmNumaLoc.numaId) {
            if (UINT64_MAX - debtInfo.size < totalBorrow) {
                totalBorrow = UINT64_MAX;
                UBSE_LOG_WARN << "The totalBorrow exceeds the range of uint64.";
                break;
            }
            totalBorrow += debtInfo.size;
        }
    }

    globalNumaInfoMapIn[alarmNumaLoc].numaMemBorrow = totalBorrow;
    globalNumaInfoMapIn[alarmNumaLoc].numaLoc = alarmNumaLoc;
    // get numaMemLend
    std::vector<UbseNodeNumaInfo> numaNodeInfoList{};
    auto ret = UbseGetNodeNumaInfoByNodeId(alarmNumaLoc.hostId, numaNodeInfoList);
    if (ret != UBSE_OK || numaNodeInfoList.empty()) {
        UBSE_LOG_ERROR << "Get nodeNumaInfo by nodeId failed, ret=" << static_cast<uint32_t>(ret);
        return;
    }
    for (auto numaNodeInfo : numaNodeInfoList) {
        if (numaNodeInfo.numaId == alarmNumaLoc.numaId) {
            globalNumaInfoMapIn[alarmNumaLoc].numaMemLend = numaNodeInfo.memLent;
        }
    }

    UBSE_LOG_DEBUG << "Numa Info from Alarm, numaId=" << alarmNumaLoc.numaId << ", hostId=" << alarmNumaLoc.hostId
                   << ", socketId=" << alarmNumaLoc.socketId
                   << ", numaMemBorrow=" << globalNumaInfoMapIn[alarmNumaLoc].numaMemBorrow
                   << "byte, numaMemLend=" << globalNumaInfoMapIn[alarmNumaLoc].numaMemLend << "byte.";
}

enum NodeLocLevel : uint32_t {
    HOSTID = 0,
    SOCKETID,
    NUMAID
};

VmResult ParseNodeLocInfo(const std::string &nodeLocInfoStr, Notify &notify)
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
        } catch (std::exception &e) {
            UBSE_LOG_ERROR << "VM parse nodeLoc info failed, error=" << e.what();
            return VM_ERROR;
        }

        i++;
    }
    return VM_OK;
}

VmResult ParseNodeLocInfoJson(Value &pstJson, const std::string &key, Notify &notify)
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

VmResult AlarmHandler::ParseOomMessage(const std::string &eventMessage, Notify &notify)
{
    Document msgBody;
    msgBody.Parse(eventMessage.c_str());
    if (msgBody.HasParseError()) {
        UBSE_LOG_ERROR << "Bad Json Format=" << eventMessage;
        return VM_ERROR_INVAL;
    }
    auto ret = ParseNodeLocInfoJson(msgBody, "notifyNumaLoc", notify);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to parse notifyNumaLoc=" << eventMessage;
        return VM_ERROR_INVAL;
    }
    if (!msgBody.HasMember("oomEventFlag") || !msgBody["oomEventFlag"].IsNumber()) {
        UBSE_LOG_WARN << "null oomEventFlag=" << eventMessage;
        return VM_OK;
    }
    notify.oomEventFlag = static_cast<int32_t>(msgBody["oomEventFlag"].GetInt64());
    return VM_OK;
}
} // namespace vm
