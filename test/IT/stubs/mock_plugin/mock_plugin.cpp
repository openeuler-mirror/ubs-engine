/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "mock_plugin.h"

#include <securec.h>

#include <algorithm>
#include <set>
#include <sstream>
#include <vector>

#include "ubse_error.h"
#include "ubse_mem_controller.h"
#include "ubse_node_controller.h"
#include "ubse_ras.h"

using namespace ubse::ras;
using namespace ubse::log;
using namespace ubse::mem::controller;
using namespace ubse::nodeController;
using namespace ubse::common::def;

// ==================== 故障迁移辅助函数 (模拟 mempooling/RMRS) ====================

/**
 * @brief 等待集群中非故障节点进入平稳状态
 */
static UbseResult WaitWorking(const std::string& faultNodeId)
{
    std::set<uint32_t> staticNodeInfoList = UbseNodeController::GetInstance().UbseGetAllDeployedNode();
    auto nodeMap = UbseNodeController::GetInstance().GetAllNodes();
    for (const auto& nodeId : staticNodeInfoList) {
        std::string tmpNodeId = std::to_string(nodeId);
        if (tmpNodeId == faultNodeId) {
            continue;
        }
        auto it = nodeMap.find(tmpNodeId);
        if (it == nodeMap.end()) {
            UBSE_LOGGER_WARN(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "Node not found: " << tmpNodeId;
            continue;
        }
        if (it->second.clusterState == UbseNodeClusterState::UBSE_NODE_INIT ||
            it->second.clusterState == UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
            UBSE_LOGGER_WARN(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
                << "Node:" << tmpNodeId
                << " not smoothing, clusterState:" << static_cast<uint32_t>(it->second.clusterState);
        }
        UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
            << "Node:" << tmpNodeId << " clusterState=" << static_cast<uint32_t>(it->second.clusterState);
    }
    return UBSE_OK;
}

/**
 * @brief 获取故障节点的 numa 借用账本信息
 */
static UbseResult GetDebtInfo(const std::string& faultNodeId, std::vector<UbseNumaMemoryDebtInfo>& debtInfos)
{
    UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "GetDebtInfo for node: " << faultNodeId;
    auto ret = UbseGetNumaMemDebtInfoWithNode(faultNodeId, debtInfos);
    if (ret != UBSE_OK) {
        UBSE_LOGGER_ERROR(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "GetDebtInfo failed, ret=" << ret;
        return UBSE_ERROR_AGAIN;
    }
    UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
        << "GetDebtInfo success, count=" << debtInfos.size();
    return UBSE_OK;
}

/**
 * @brief 在非故障节点上借用 numa 内存（故障迁移场景）
 */
static UbseResult BorrowMem(const std::string& faultNodeId, const UbseNumaMemoryDebtInfo& info)
{
    UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
        << "BorrowMem, name=" << info.name << ", size=" << info.size;

    UbseMemBorrower borrower;
    borrower.nodeId = info.borrowNodeId;

    std::vector<UbseMemNumaLender> lenders;
    for (size_t i = 0; i < info.lentNumaIdList.size() && i < info.lentNumaSizeList.size(); ++i) {
        UbseMemNumaLender lender{};
        auto nodeMap = UbseNodeController::GetInstance().GetAllNodes();
        auto it = nodeMap.find(info.lentNodeId);
        if (it != nodeMap.end()) {
            lender.slotId = it->second.slotId;
        } else {
            UBSE_LOGGER_WARN(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
                << "Lent node not found in nodeMap: " << info.lentNodeId << ", skip";
            continue;
        }
        lender.socketId = static_cast<uint32_t>(info.lentSocketIdList.empty() ? 0 : info.lentSocketIdList[i]);
        lender.numaId = static_cast<uint64_t>(info.lentNumaIdList[i]);
        lender.size = info.lentNumaSizeList[i];
        lenders.push_back(lender);
    }

    if (lenders.empty()) {
        UBSE_LOGGER_WARN(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
            << "No valid lender for " << info.name << ", skip borrow";
        return UBSE_OK;
    }

    std::string borrowName = info.name + "_fault";

    uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN] = {0};
    if (memcpy_s(usrInfo, sizeof(usrInfo), info.usrInfo, sizeof(info.usrInfo)) != EOK) {
        UBSE_LOGGER_WARN(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "memcpy_s usrInfo failed";
    }

    UbseMemNumaDesc desc;
    auto ret = UbseMemNumaCreateWithLender(borrowName, borrower, lenders, usrInfo, desc);
    if (ret != UBSE_OK) {
        UBSE_LOGGER_ERROR(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
            << "BorrowMem failed, name=" << borrowName << ", ret=" << ret;
        return ret;
    }

    UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
        << "BorrowMem success, name=" << borrowName << ", numaId=" << desc.numaId;
    return UBSE_OK;
}

/**
 * @brief 删除与故障节点相关的 numa 借用
 */
static UbseResult DeleteMem(const std::string& faultNodeId, const UbseNumaMemoryDebtInfo& info)
{
    UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "DeleteMem, name=" << info.name;

    UbseMemBorrower borrower;
    borrower.nodeId = info.borrowNodeId;

    auto ret = UbseMemNumaDelete(info.name, borrower);
    if (ret != UBSE_OK) {
        UBSE_LOGGER_ERROR(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
            << "DeleteMem failed, name=" << info.name << ", ret=" << ret;
        return ret;
    }

    UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "DeleteMem success, name=" << info.name;
    return UBSE_OK;
}

// ==================== OOM 辅助函数 (模拟 virt_agent 接收 OOM 回调) ====================

static constexpr uint16_t HUGEPAGE_OOM = 2;

/**
 * @brief 解析 OOM faultInfo 中的 numaId 列表和 reason
 *
 * faultInfo 格式: "nrNid_numaId1_numaId2_..._reason"
 * 例如: "2_0_1_2" 表示 nr_nid=2, numaId=[0,1], reason=2(大页OOM)
 */
static UbseResult ParseOomFaultInfo(const std::string& faultInfo, std::vector<uint16_t>& numaIds, uint16_t& reason)
{
    std::vector<std::string> parts;
    std::stringstream ss(faultInfo);
    std::string token;
    while (std::getline(ss, token, '_')) {
        parts.push_back(token);
    }
    if (parts.size() < 3) {
        UBSE_LOGGER_ERROR(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
            << "OOM faultInfo format invalid: " << faultInfo;
        return UBSE_ERROR_INVAL;
    }
    try {
        uint16_t nrNid = static_cast<uint16_t>(std::stoi(parts[0]));
        if (nrNid < 1 || nrNid + 2 != parts.size()) {
            UBSE_LOGGER_ERROR(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
                << "OOM nrNid mismatch, nrNid=" << nrNid << ", parts=" << parts.size();
            return UBSE_ERROR_INVAL;
        }
        for (uint16_t i = 1; i <= nrNid; ++i) {
            numaIds.push_back(static_cast<uint16_t>(std::stoi(parts[i])));
        }
        reason = static_cast<uint16_t>(std::stoi(parts.back()));
    } catch (const std::exception& e) {
        UBSE_LOGGER_ERROR(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
            << "Parse OOM faultInfo exception: " << e.what();
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

// ==================== 故障处理主函数 ====================

uint32_t MockPluginFaultHandle(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo)
{
    UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
        << "MockPluginFaultHandle, event=" << alarmFaultEvent << ", faultInfo=" << faultInfo;

    const std::string& faultNodeId = faultInfo;

    if (auto ret = WaitWorking(faultNodeId); ret != UBSE_OK) {
        UBSE_LOGGER_WARN(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "WaitWorking failed, continue anyway";
    }

    std::vector<UbseNumaMemoryDebtInfo> debtInfos;
    if (auto ret = GetDebtInfo(faultNodeId, debtInfos); ret != UBSE_OK) {
        UBSE_LOGGER_ERROR(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "GetDebtInfo failed";
        return ret;
    }

    if (debtInfos.empty()) {
        UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "No debt info, nothing to migrate";
        return UBSE_OK;
    }

    for (const auto& info : debtInfos) {
        UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
            << "Processing debt, name=" << info.name << ", borrowNode=" << info.borrowNodeId
            << ", lentNode=" << info.lentNodeId << ", size=" << info.size;

        if (info.lentNodeId == faultNodeId) {
            if (auto ret = BorrowMem(faultNodeId, info); ret != UBSE_OK) {
                UBSE_LOGGER_ERROR(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "BorrowMem failed";
                return ret;
            }
        }

        if (info.borrowNodeId == faultNodeId) {
            if (auto ret = DeleteMem(faultNodeId, info); ret != UBSE_OK) {
                UBSE_LOGGER_ERROR(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "DeleteMem failed";
                return ret;
            }
        }
    }

    UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "MockPluginFaultHandle completed";
    return UBSE_OK;
}

uint32_t MockPluginOomHandle(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo)
{
    UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
        << "MockPluginOomHandle, event=" << alarmFaultEvent << ", faultInfo=" << faultInfo;

    if (alarmFaultEvent != ALARM_OOM_EVENT) {
        UBSE_LOGGER_WARN(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
            << "Unexpected alarm event type=" << alarmFaultEvent;
        return UBSE_ERROR_INVAL;
    }

    std::vector<uint16_t> numaIds;
    uint16_t reason = 0;
    auto ret = ParseOomFaultInfo(faultInfo, numaIds, reason);
    if (ret != UBSE_OK) {
        UBSE_LOGGER_ERROR(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "ParseOomFaultInfo failed";
        return ret;
    }

    std::string numaIdStr;
    for (size_t i = 0; i < numaIds.size(); ++i) {
        if (i > 0)
            numaIdStr += ",";
        numaIdStr += std::to_string(numaIds[i]);
    }

    UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
        << "OOM received: numaIds=[" << numaIdStr << "], reason=" << reason
        << (reason == HUGEPAGE_OOM ? "(hugepage)" : "(smallpage)");

    if (reason != HUGEPAGE_OOM) {
        UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "Not hugepage OOM, return 1";
        return 1;
    }

    UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "MockPluginOomHandle completed";
    return UBSE_OK;
}

// ==================== 插件生命周期 ====================

uint32_t UbsePluginInit(uint16_t modCode)
{
    UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "Plugin init, modCode=" << modCode;

    // 注册故障回调：REBOOT / PANIC / KERNEL_REBOOT (模拟 mempooling/RMRS)
    auto ret = RegisterAlarmFaultHandler(ALARM_REBOOT_EVENT, MOCK_PLUGIN_MODULE_NAME, MockPluginFaultHandle,
                                         AlarmHandlerPriority::HIGH);
    ret |= RegisterAlarmFaultHandler(ALARM_PANIC_EVENT, MOCK_PLUGIN_MODULE_NAME, MockPluginFaultHandle,
                                     AlarmHandlerPriority::HIGH);
    ret |= RegisterAlarmFaultHandler(ALARM_KERNEL_REBOOT_EVENT, MOCK_PLUGIN_MODULE_NAME, MockPluginFaultHandle,
                                     AlarmHandlerPriority::HIGH);

    // 注册 OOM 回调 (模拟 virt_agent 逃逸借用)
    ret |= RegisterAlarmFaultHandler(ALARM_OOM_EVENT, MOCK_PLUGIN_MODULE_NAME, MockPluginOomHandle,
                                     AlarmHandlerPriority::HIGH);
    if (ret != UBSE_OK) {
        UBSE_LOGGER_ERROR(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE)
            << "RegisterAlarmFaultHandler failed, ret=" << ret;
        return ret;
    }

    UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "Plugin init success";
    return UBSE_OK;
}

void UbsePluginDeInit()
{
    UBSE_LOGGER_INFO(MOCK_PLUGIN_MODULE_NAME, MOCK_PLUGIN_MODULE_CODE) << "Plugin deinit";
}
