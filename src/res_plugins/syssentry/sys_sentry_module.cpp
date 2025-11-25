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
#include "src/res_plugins/mti/ubse_lcne_module.h"
#include "sys_sentry module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_inner.h"
#include "ubse_os_util.h"
#include "ubse_str_util.h"

namespace syssentry {
using namespace ubse::mti;
using namespace ubse::log;
DYNAMIC_CREATE(SysSentryModule);
UBSE_DEFINE_THIS_MODULE("ubse", SYS_SENTRY)

UbseResult SysSentryModule::Initialize()
{
    return UBSE_OK;
}

void SysSentryModule::UnInitialize() {}

UbseResult SysSentryModule::Start()
{
    InitSysSentryBaseCommand();
    InitPanicHandle();
    auto ret = UbseRasObserver::GetInstance().Start();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[sentry] Start observer failed, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

void SysSentryModule::Stop()
{
    UbseRasObserver::GetInstance().Stop();
}

void SysSentryModule::InitSysSentryBaseCommand()
{
    std::string commandModprobeSentryReport = "modprobe sentry_reporter 2>&1";
    std::string commandModprobeSentryRemoteReport = "modprobe sentry_remote_reporter 2>&1";
    std::string commandSetPanicReporter = "sentryctl set sentry_remote_reporter --panic=on 2>&1";
    std::string commandSetKernelRebootReporter = "sentryctl set sentry_remote_reporter --kernel_reboot=on 2>&1";
    std::vector<std::string> commands{commandModprobeSentryReport, commandModprobeSentryRemoteReport,
                                      commandSetPanicReporter, commandSetKernelRebootReporter};
    std::string commandResult;
    for (auto command : commands) {
        auto result = ubse::utils::UbseOsUtil::Exec(command, commandResult);
        if (result != UBSE_OK) {
            UBSE_LOG_WARN << "[sentry] command: " << command << ", result: " << commandResult;
        }
    }
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

UbseResult GetEids(std::string &clientEid, std::string &serverEids)
{
    auto lcneModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::mti::UbseLcneModule>();
    if (lcneModule == nullptr) {
        UBSE_LOG_ERROR << "[sentry] Get lcne module failed. ";
        return UBSE_ERROR_NULLPTR;
    }

    auto allSocketComEid = lcneModule->GetAllSocketComEid();
    MtiNodeInfo localNodeInfo;
    auto result = UbseGetLocalNodeInfo(localNodeInfo);
    if (result != UBSE_OK) {
        UBSE_LOG_WARN << "[sentry] get eid fail, " << ubse::log::FormatRetCode(result);
        return result;
    }

    std::unordered_map<std::string, std::vector<std::string>> eids{};
    for (const auto &info : allSocketComEid) {
        std::vector<std::string> devVec;
        ubse::utils::Split(info.first.devName, "-", devVec);
        if (devVec.size() < NO_2) {
            UBSE_LOG_ERROR << "Split str failed, devName=" << info.first.devName;
            return UBSE_ERROR_INVAL;
        }
        eids[devVec[0]].emplace_back(info.second.primaryEid);
    }
    std::vector<std::string> eidGroup;
    for (auto &e : eids[localNodeInfo.nodeId]) {
        eidGroup.emplace_back(e);
    }
    for (const auto &eidPair : eids) {
        for (size_t i = 0; i < eidPair.second.size(); i++) {
            if (eidPair.second[i].empty()) {
                continue;
            }
            if (std::find(eids[localNodeInfo.nodeId].begin(), eids[localNodeInfo.nodeId].end(), eidPair.second[i]) !=
                eids[localNodeInfo.nodeId].end()) {
                continue;
            }
            if (!eidGroup[i].empty()) {
                eidGroup[i] += ",";
            }
            eidGroup[i] += eidPair.second[i];
        }
    }

    LinkStrings(serverEids, ";", eidGroup);

    if (eids.find(localNodeInfo.nodeId) == eids.end()) {
        return UBSE_ERROR_CONF_INVALID;
    }

    LinkStrings(clientEid, ";", eids[localNodeInfo.nodeId]);
    return UBSE_OK;
}

// 对动态参数转义, 用引号把数据“包裹”起来，shell 不会解析内部的 ;、`
std::string ShellEscape(const std::string &str)
{
    if (str.empty())
        return "''";
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

void SysSentryModule::InitPanicHandle()
{
    std::string clientEid;
    std::string serverEids;
    std::string cna = "1";
    auto ret = GetEids(clientEid, serverEids);
    if (ret != UBSE_OK) {
        return;
    }

    std::string commandResult;
    std::string commandMonitorSetUid =
        "sentryctl set sentry_remote_reporter --eid=" + ShellEscape(clientEid) + "  2>&1";
    std::string commandMonitorSetCna = "sentryctl set sentry_remote_reporter --cna=" + ShellEscape(cna) + " 2>&1";
    std::string commandSetServerEid = "sentryctl set sentry_urma_comm --server_eid=" + ShellEscape(serverEids) +
                                      " --client_jetty_id=10000 " + "  2>&1";
    std::vector<std::string> commands{commandMonitorSetUid, commandMonitorSetCna, commandSetServerEid};
    for (const auto &command : commands) {
        commandResult = "";
        auto result = ubse::utils::UbseOsUtil::Exec(command, commandResult);
        if (result != UBSE_OK) {
            UBSE_LOG_WARN << "[sentry] command: " << command << ", result: " << commandResult;
        }
    }
}
} // namespace syssentry