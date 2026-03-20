/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "sentry_observer.h"
#include "src/adapter_plugins/mti/ubse_lcne_module.h"
#include "sys_sentry_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_os_util.h"
#include "ubse_str_util.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "ubse_thread_pool_module.h"
#include "ubse_timer.h"

namespace syssentry {
using namespace ubse::log;
using namespace ubse::adapter_plugins::mti;
DYNAMIC_CREATE(SysSentryModule);
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult SysSentryModule::Initialize()
{
    auto taskExecutor = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return taskExecutor->Create(UBSE_RAS_CONFIG_SYSSENTRY_TASK_NAME, NO_1, NO_128);
}

void SysSentryModule::UnInitialize()
{
    auto taskExecutor = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        UBSE_LOG_WARN << "TaskExecutorModule is null";
        return;
    }
    taskExecutor->Remove(UBSE_RAS_CONFIG_SYSSENTRY_TASK_NAME);
}

UbseResult SysSentryModule::Start()
{
    UbseRasObserver::GetInstance().UbseConfigSysSentryWithRetry(); // 不校验返回值，sysSentry未就绪时不影响ubse其它功能
    auto ret = UbseRasObserver::GetInstance().Start();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Start observer failed, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

void SysSentryModule::Stop()
{
    UbseRasObserver::GetInstance().Stop();
}

void LinkStrings(std::string &result, const std::string linkSymbol, const std::vector<std::string> strings)
{
    for (const auto &item : strings) {
        if (!result.empty()) {
            result += linkSymbol;
        }
        result += item;
    }
}

std::vector<std::string> SplitString(const std::string &str, char delimiter)
{
    std::vector<std::string> result;
    std::istringstream iss(str);
    std::string token;

    while (std::getline(iss, token, delimiter)) {
        result.push_back(token);
    }

    return result;
}

UbseResult ProcessEids(const std::map<UbseDevName, UbseUrmaEidInfo> &allSocketComEid,
                       const std::string& nodeId, std::unordered_map<std::string, std::vector<std::string>>& eids,
                       std::vector<std::string>& eidGroup)
{
    for (const auto& info : allSocketComEid) {
        std::vector<std::string> devVec;
        ubse::utils::Split(info.first.devName, "-", devVec);
        if (devVec.size() < NO_2) {
            UBSE_LOG_ERROR << "Split str failed, devName=" << info.first.devName
                           << ", primaryEid=" << info.second.primaryEid;
            return UBSE_ERROR_INVAL;
        }
        eids[devVec[0]].emplace_back(info.second.primaryEid);
    }
    for (auto &e : eids[nodeId]) {
        eidGroup.emplace_back(e);
    }
    for (const auto &eidPair : eids) {
        for (size_t i = 0; i < eidPair.second.size(); i++) {
            if (eidPair.second[i].empty()) {
                continue;
            }
            if (std::find(eids[nodeId].begin(), eids[nodeId].end(), eidPair.second[i]) !=
                eids[nodeId].end()) {
                continue;
            }
            if (i >= eidGroup.size()) {
                // 当前节点具有的eid数量与其它节点不同，记录警告日志并添加默认值
                UBSE_LOG_WARN << "Node eid count mismatch, adding default eid";
                eidGroup.emplace_back("");
            }
            if (!eidGroup[i].empty()) {
                eidGroup[i] += ",";
            }
            eidGroup[i] += eidPair.second[i];
        }
    }
    return UBSE_OK;
}

UbseResult GetEids(std::string &clientEid, std::string &serverEids)
{
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap{};
    auto result = UbseMtiInterface::GetInstance().GetAllSocketComEid(socketInfoMap);
    if (result != UBSE_OK) {
        UBSE_LOG_WARN << "Get all socket eid failed, " << ubse::log::FormatRetCode(result);
        return result;
    }
    UbseMtiNodeInfo localNodeInfo;
    result = UbseMtiInterface::GetInstance().GetLocalNodeInfo(localNodeInfo);
    if (result != UBSE_OK || localNodeInfo.nodeId.empty()) {
        UBSE_LOG_WARN << "Get local node info failed, " << ubse::log::FormatRetCode(result);
        return result;
    }

    std::unordered_map<std::string, std::vector<std::string>> eids{};
    std::vector<std::string> eidGroup;
    if (auto ret = ProcessEids(socketInfoMap, localNodeInfo.nodeId, eids, eidGroup); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to process eids, local nodeId=" << localNodeInfo.nodeId;
        return ret;
    }
    LinkStrings(serverEids, ";", eidGroup);

    if (eids.find(localNodeInfo.nodeId) == eids.end()) {
        UBSE_LOG_ERROR << "Cannot find local node eid which nodeId=" << localNodeInfo.nodeId;
        return UBSE_ERROR_CONF_INVALID;
    }

    LinkStrings(clientEid, ";", eids[localNodeInfo.nodeId]);
    return UBSE_OK;
}

// 对动态参数转义, 用引号把数据“包裹”起来，shell 不会解析内部的 ;、`
std::string ShellEscape(const std::string &str)
{
    if (str.empty()) {
        return "''";
    }
    std::string result;
    result += '\'';
    for (char c : str) {
        if (c == '\'') {
            result += "'\\''";
        } else {
            result += c;
        }
    }
    result += '\'';
    return result;
}

UbseResult GetCurNodeCna(std::vector<std::string> &busNodeCnas)
{
    UbseMtiCpuTopoInfoMap topo;
    auto ret = UbseMtiInterface::GetInstance().GetClusterCpuTopo(topo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get cluster cpu topo";
        return ret;
    }
    UbseMtiNodeInfo localNodeInfo;
    ret = UbseMtiInterface::GetInstance().GetLocalNodeInfo(localNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get local node info";
        return ret;
    }
    for (auto &devCputopo : topo) {
        auto devName = devCputopo.first;
        std::string devNodeId{};
        std::string devSocketId{};
        if (devName.SplitDevName(devNodeId, devSocketId) != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to split dev name=" << devName.devName;
            continue;
        }

        auto &cpuTopo = devCputopo.second;
        if (std::to_string(cpuTopo.slotId) == localNodeInfo.nodeId) {
            UBSE_LOG_INFO << "Get local node cna=" << cpuTopo.busNodeCna
                          << ", slotId=" << cpuTopo.slotId;
            busNodeCnas.push_back(std::to_string(cpuTopo.busNodeCna));
        }
    }
    if (busNodeCnas.empty()) {
        UBSE_LOG_ERROR << "Failed to get current node cna";
        return UBSE_ERROR_AGAIN;
    }
    return UBSE_OK;
}

UbseResult SetSysSentryFaultEventOn()
{
    std::string commandSetPanicReporter = "sentryctl set sentry_remote_reporter --panic=on 2>&1";
    std::string commandSetKernelRebootReporter = "sentryctl set sentry_remote_reporter --kernel_reboot=on 2>&1";
    std::string commandSetBmcReporter = "sentryctl set sentry_reporter --power_off=on 2>&1";
    std::string commandSetMemFaultReporter = "sentryctl set sentry_reporter --ub_mem_fault=on 2>&1";
    std::string commandSetOomFaultReporter = "sentryctl set sentry_reporter --oom=on 2>&1";
    using CommandDescList = std::vector<std::pair<std::string, std::string>>;
    CommandDescList tasks = {{commandSetPanicReporter, "commandSetPanicReporter"},
                             {commandSetKernelRebootReporter, "commandSetKernelRebootReporter"},
                             {commandSetBmcReporter, "commandSetBmcReporter"},
                             {commandSetMemFaultReporter, "commandSetMemFaultReporter"},
                             {commandSetOomFaultReporter, "commandSetOomFaultReporter"}};
    std::string commandResult;
    for (const auto &[command, desc] : tasks) {
        commandResult = "";
        auto result = ubse::utils::UbseOsUtil::Exec(command, commandResult);
        if (result != UBSE_OK) {
            UBSE_LOG_DEBUG << "Failed to execute: " << desc;
            return UBSE_RAS_ERROR_SET_FAULT_EVENT_ON;
        }
    }
    return UBSE_OK;
}

UbseResult SetSysSentryFaultReporter()
{
    std::string clientEid;
    std::string serverEids;
    std::string cna = "1";
    auto ret = GetEids(clientEid, serverEids);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get eids";
        return UBSE_ERROR;
    }

    if (std::find(clientEid.begin(), clientEid.end(), '\"') != clientEid.end() ||
        std::find(serverEids.begin(), serverEids.end(), '\"') != serverEids.end()) {
        UBSE_LOG_ERROR << "Input eid contains \", clientEid: " << clientEid << " serverEids" << serverEids;
        return UBSE_ERROR_INVAL;
    }
    std::string commandResult;
    std::string commandMonitorSetUid =
        "sentryctl set sentry_remote_reporter --eid=" + ShellEscape(clientEid) + "  2>&1";
    std::string commandMonitorSetCna = "sentryctl set sentry_remote_reporter --cna=" + ShellEscape(cna) + " 2>&1";
    std::string commandSetServerEid = "sentryctl set sentry_urma_comm --server_eid=" + ShellEscape(serverEids) +
                                      " --client_jetty_id=1000 " + "  2>&1";
    using CommandDescList = std::vector<std::pair<std::string, std::string>>;
    CommandDescList tasks = {
        {commandMonitorSetUid, "commandMonitorSetUid"},
        {commandMonitorSetCna, "commandMonitorSetCna"},
        {commandSetServerEid, "commandSetServerEid"},
    };
    for (const auto &[command, desc] : tasks) {
        commandResult = "";
        auto result = ubse::utils::UbseOsUtil::Exec(command, commandResult);
        if (result != UBSE_OK) {
            UBSE_LOG_DEBUG << "Failed to execute: " << desc; // 防止日志中记录命令行信息
            return UBSE_RAS_ERROR_SET_SENTRY_REPORTER;
        }
    }
    return UBSE_OK;
}
} // namespace syssentry