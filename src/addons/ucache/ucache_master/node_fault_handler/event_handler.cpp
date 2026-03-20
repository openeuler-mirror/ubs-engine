/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "event_handler.h"

#include <chrono>
#include <thread>
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "data_collect.h"
#include "deserialize.h"
#include "ucache_json_util.h"
#include "ucache_config.h"
#include "ucache_error.h"
#include "ucache_master.h"
#include "ucache_string_util.h"
#include "ubse_error.h"


namespace ucache::fault_handler {

using namespace ubse::log;
using namespace data_collect;
using namespace ubse::mem::controller;

std::atomic<bool> EventHandler::gNodeFaultFlag{false};
std::atomic<bool> EventHandler::gMasterStopFlag{false};

void GetMemIdsFromNumaMemMap(std::map<int, std::map<std::string, uint64_t>> &numaMemMap,
                                    std::vector<std::string> &memNameList)
{
    std::string memName;
    for (auto &memSizeMapIter : numaMemMap) {
        int numaId = memSizeMapIter.first;
        auto memSizeMap = memSizeMapIter.second;
        if (memSizeMap.empty()) {
            UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "numa: " << numaId << "has no mem ";
        } else {
            for (auto &sizeMapIter : memSizeMap) {
                memName = sizeMapIter.first;
                memNameList.emplace_back(memName);
            }
        }
    }
}

void GetMemIdFromLendMap(const std::string &nodeId,
                                std::map<std::string, std::vector<NodeMemBorrowInfo>> &lendMap,
                                std::vector<std::string> &memNameList)
{
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "GetMemIdFromLendMap start. node: " << nodeId;
    auto lendMapIter = lendMap.find(nodeId);
    if (lendMapIter == lendMap.end()) {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node(" << nodeId << ") not found in lend map.";
        return;
    } else if (lendMapIter->second.empty()) {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node(" << nodeId << ") has no lend mem.";
        return;
    }
    // 查询所有借出内存的memName

    for (auto &lendInfo : lendMapIter->second) {
        auto numaMemMap = lendInfo.numaNodeBorrowSize;
        if (numaMemMap.empty()) {
            UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Node(" << nodeId << ") has no lend mem to Node(" << lendInfo.destNodeId << ").";
        } else {
            UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "In the numaMemMap Node(" << nodeId << ") lend to Node(" << lendInfo.destNodeId << "):";
            GetMemIdsFromNumaMemMap(numaMemMap, memNameList);
        }
    }
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "GetMemIdFromLendMap end.";
}

void GetMemIdFromBorrowMap(const std::string &nodeId,
                                  std::map<std::string, std::vector<NodeMemBorrowInfo>> &borrowMap,
                                  std::vector<std::string> &memNameList)
{
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "GetMemIdFromBorrowMap start. node: " << nodeId;
    auto borrowMapIter = borrowMap.find(nodeId);
    if (borrowMapIter == borrowMap.end()) {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node(" << nodeId << ") not found in borrow map.";
        return;
    } else if (borrowMapIter->second.empty()) {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node(" << nodeId << ") has no borrow mem.";
        return;
    }
    // 查询所有借出内存的memName
    std::string memName;
    for (auto &borrowInfo : borrowMapIter->second) {
        auto numaMemMap = borrowInfo.numaNodeBorrowSize;
        if (numaMemMap.empty()) {
            UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Node(" << nodeId << ") has no borrow mem from Node(" << borrowInfo.srcNodeId << ").";
        } else {
            UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "In the numaMemMap Node(" << nodeId << ") borrow from Node(" << borrowInfo.srcNodeId << "):";
            GetMemIdsFromNumaMemMap(numaMemMap, memNameList);
        }
    }
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "GetMemIdFromBorrowMap end.";
}

// 归还故障节点相关的内存
uint32_t ReturnMemofFaultyNode(const std::string &nodeId)
{
    // 向内存子系统查询内存账本
    uint32_t ret = UCACHE_OK;
    std::vector<UbseNumaMemoryDebtInfo> debtInfos{};
    UbseResult result = UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos);
    if (result != UBSE_OK && result != UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "UbseGetNumaMemDebtInfoWithNode failed.";
        return UBSE_API_ERROR;
    }
    for (auto &debtInfo : debtInfos) {
        std::string memName = debtInfo.name;
        UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Return memory start, memName=" << memName << ".";
        UbseMemBorrower borrower{.nodeId = debtInfo.borrowNodeId};
        UbseResult result = UbseMemNumaDelete(memName, borrower);
        if (result != UBSE_OK) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Return memory: " << memName << " failed: rack memory API error.";
            ret = EXEC_MEM_RETURN_ERROR;
        } else {
            UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Return memory succeed, memName=" << memName << ".";
        }
    }
    return ret;
}

uint32_t ReturnMemofMemId(const std::string &nodeId, const uint64_t memId, const EventCondition eventCondition)
{
    uint32_t ret = UCACHE_OK;
    std::vector<BorrowMemInfo> borrowInfos{};
    bool parseMemId = false;
    if (eventCondition == EventCondition::UCE_MEMID_FAILURE) {
        parseMemId = true;
    }
    ret = Deserialize::GetBorrowMemInfo(nodeId, borrowInfos);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << " GetBorrowMemInfo failed ret: " << ret << ".";
        return ret;
    }

    std::string memName{};

    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "Node(" << nodeId << ")" << "'s borrowInfos.size is" << borrowInfos.size() << ".";
    for (auto &info : borrowInfos) {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << info.ToString() << ".";
        if (info.borrowNodeId == nodeId &&
            std::find(info.borrowMemId.begin(), info.borrowMemId.end(), memId) != info.borrowMemId.end()) {
            memName = info.name;
            UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Found mem_id(" << memId << ") belong to" << " borrowId: " << memName << ".";
            break;
        }
    }

    if (memName == "") {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Cannot Find mem_id(" << memId << ") in Node(" << nodeId << ")'s borrowInfos.";
        return UCACHE_ERR;
    }

    // 归还对应内存
    UbseMemBorrower borrower{.nodeId = nodeId};
    UbseResult result = UbseMemNumaDelete(memName, borrower);
    if (result != UBSE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Return memory: " << memName << " failed: rack memory API error.";
        ret = EXEC_MEM_RETURN_ERROR;
    }
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Return memory: " << memName << " succeed.";
    return ret;
}

uint32_t EventHandler::AlarmRebootEventHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo)
{
    // 暂停主流程
    gNodeFaultFlag.store(true);
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "AlarmRebootEventHandler start";
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "ALARM_FAULT_TYPE" << alarmFaultEvent
        << " faultInfo: " << faultInfo;

    // 等待主流程退出
    const int sleepMs = 100;
    while (!gMasterStopFlag.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }

    // 归还故障节点相关的内存
    std::string nodeId = faultInfo;
    uint32_t ret = ReturnMemofFaultyNode(nodeId);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "AlarmRebootEventHandler ReturnMemofFaultyNode failed, ret: " << ret;
        return ret;
    }

    // 通知主流程恢复
    gNodeFaultFlag.store(false);
    gMasterStopFlag.store(false);
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "AlarmRebootEventHandler end";
    return ret;
}

uint32_t EventHandler::AlarmPanicEventHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo)
{
    // 暂停主流程
    gNodeFaultFlag.store(true);
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "AlarmPanicEventHandler start";
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "ALARM_FAULT_TYPE: " << alarmFaultEvent
        << " faultInfo: " << faultInfo;

    // 等待主流程退出
    const int sleepMs = 100;
    while (!gMasterStopFlag.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }

    // 归还故障节点相关的内存
    std::string nodeId = faultInfo;
    DataCollect::phyNodeStatMap[nodeId] = PhyNodeStat::FAULT;
    uint32_t ret = ReturnMemofFaultyNode(nodeId);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "AlarmPanicEventHandler ReturnMemofFaultyNode failed, ret: " << ret;
        return ret;
    }

    // 通知主流程恢复
    gNodeFaultFlag.store(false);
    gMasterStopFlag.store(false);
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "AlarmPanicEventHandler end, ret : " << ret;
    return ret;
}

uint32_t EventHandler::AlarmKernelRebootEventHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo)
{
    // 暂停主流程
    gNodeFaultFlag.store(true);
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "AlarmKernelRebootEventHandler start";
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "ALARM_FAULT_TYPE: " << alarmFaultEvent
        << " faultInfo: " << faultInfo;
    std::string nodeId = faultInfo;

    // 等待主流程退出
    const int sleepMs = 100;
    while (!gMasterStopFlag.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
    // 故障节点
    DataCollect::phyNodeStatMap[nodeId] = PhyNodeStat::FAULT;
    // 归还故障节点相关的内存
    uint32_t ret = ReturnMemofFaultyNode(nodeId);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "AlarmKernelRebootEventHandler ReturnMemofFaultyNode failed, ret: " << ret;
        return ret;
    }

    // 通知主流程恢复
    gNodeFaultFlag.store(false);
    gMasterStopFlag.store(false);
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "AlarmKernelRebootEventHandler end, ret : " << ret;
    return ret;
}

uint32_t EventHandler::AlarmUceEventHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo)
{
    gNodeFaultFlag.store(true);
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "AlarmUceEventHandler start, ALARM_FAULT_TYPE: "
        << alarmFaultEvent << " faultInfo: " << faultInfo << ".";

    const int sleepMs = 100;
    while (!gMasterStopFlag.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
    rapidjson::Document pstJson;
    pstJson.Parse(faultInfo.c_str());
    if (pstJson.HasParseError()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "AlarmUceEventHandler faultInfo parse error, faultInfo:" << faultInfo << ".";
        return UCACHE_ERR;
    }
    std::string importNodeIdStr{};
    std::string importMemIdStr{};
    if (JsonUtil::GetJsonStringValue(pstJson, "importNodeID", importNodeIdStr) ||
        JsonUtil::GetJsonStringValue(pstJson, "importMemID", importMemIdStr)) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "EventMessage GetJsonString error, faultInfo:" << faultInfo << ".";
        return UCACHE_ERR;
    }

    uint64_t importMemId = 0;
    uint32_t ret = SafeStoInt(importMemIdStr, importMemId);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "AlarmUceEventHandler importMemIdStr to uint64_t error ret is" << ret;
        return ret;
    }
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "The fault importMemId is:" << importMemId << " importNodeId is:" << importNodeIdStr << ".";

    ret = ReturnMemofMemId(importNodeIdStr, importMemId, EventCondition::UCE_MEMID_FAILURE);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "AlarmUceEventHandler ReturnMemofMemId failed, ret: " << ret;
        return ret;
    }
    gNodeFaultFlag.store(false);
    gMasterStopFlag.store(false);
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "AlarmUceEventHandler end, ret : " << ret;
    return ret;
}

} // namespace ucache::fault_handler