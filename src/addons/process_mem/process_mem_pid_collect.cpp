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

#include "process_mem_pid_collect.h"

#include <securec.h>
#include <fstream>
#include <string>

#include "ubse_conf.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_timer.h"
#include "process_mem_pid_config_manager.h"
#include "process_mem_pid_info_manager.h"

namespace process_mem::collect {
UBSE_DEFINE_THIS_MODULE("process_mem");

using namespace ubse::timer;
using namespace process_mem::manager;

const uint32_t PROCESS_MEM_COLLECT_INTERVAL = 10;
const size_t DEFAULT_PAGE_SIZE = 4096;

std::vector<pid_t> GetChildrenPidsFallback(pid_t parentPid)
{
    std::vector<pid_t> children;
    char cmd[256];
    snprintf_s(cmd, sizeof(cmd), sizeof(cmd), "pgrep -P %d 2>/dev/null", parentPid);

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe)
        return children;

    pid_t pid;
    while (fscanf_s(pipe.get(), "%d", &pid) == 1) {
        children.push_back(pid);
    }
    return children;
}

std::vector<pid_t> GetChildrenPids(pid_t parentPid)
{
    // 优先尝试 /proc/pid/children（高效）
    std::string path = "/proc/" + std::to_string(parentPid) + "/task/" + std::to_string(parentPid) + "/children";
    std::ifstream file(path);
    if (file.is_open()) {
        std::string line;
        if (std::getline(file, line)) {
            std::istringstream iss(line);
            std::vector<pid_t> children;
            pid_t pid;
            while (iss >> pid) {
                children.push_back(pid);
            }
            return children;
        }
    }
    // 回退到 pgrep
    return GetChildrenPidsFallback(parentPid);
}

uint32_t GetPidInfoByCollect(def::ProcessMemPidInfo& pidInfo, CollectInfoMap& pidCollectInfo)
{
    std::unordered_map<uint32_t, size_t> numaDistribution{};
    auto ret =
        ProcessMemPidCollect::GetInstance().CollectProcessNumaMemDistribution(pidInfo.configInfo.pid, numaDistribution);
    // 读取失败认为进程不存在
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "pid=" << std::to_string(pidInfo.configInfo.pid) << " collect failed=" << ret;
        pidInfo.processStatus = def::ProcessStatus::INACTIVE;
        return ret;
    }
    pidInfo.processStatus = def::ProcessStatus::IDLE;
    pidCollectInfo[pidInfo.configInfo.pid] = numaDistribution;
    return UBSE_OK;
}

uint32_t ProcessMemPidCollect::Init()
{
    const std::string section = "process_mem.pid";
    const std::string configKey = "collect.interval";
    uint32_t collectInterval = 0;
    ubse::config::UbseGetUInt(section, configKey, collectInterval);
    constexpr uint32_t maxCollectInterval = 3600;
    constexpr uint32_t defaultCollectInterval = 10;
    if (collectInterval < 1 || collectInterval > maxCollectInterval) {
        collectInterval = defaultCollectInterval;
    }
    UbseTimerHandlerRegister(
        "numaDistributionCollectTimer", [this]() -> uint32_t { return this->CycleCollectNumaInfo(); }, collectInterval);
    return UBSE_OK;
}

void ProcessMemPidCollect::UnInit()
{
    UbseTimerHandlerUnregister("numaDistributionCollectTimer");
}

uint32_t ProcessMemPidCollect::CycleCollectNumaInfo()
{
    CollectInfoMap pidCollectInfo{};
    std::vector<def::ProcessMemPidInfo> pidInfos{};
    ProcessMemPidInfoManager::GetInstance().GetAllPidInfo(pidInfos);
    for (auto pidInfo : pidInfos) {
        bool isFaultPid = (pidInfo.processStatus == def::ProcessStatus::FAULT);
        if (GetPidInfoByCollect(pidInfo, pidCollectInfo) != UBSE_OK) {
            if (!isFaultPid) {
                ProcessMemPidInfoManager::GetInstance().UnsetPidInfo(pidInfo.configInfo.pid);
            }
            continue;
        }
        CollectChildPids(pidInfo.configInfo.pid, pidInfo.configInfo, pidCollectInfo);
    }
    decltype(collectHandlers) handlerCopy;
    {
        std::shared_lock<std::shared_mutex> lock(collectHandlersMutex);
        handlerCopy = collectHandlers;
    }
    for (const auto& handler : handlerCopy) {
        handler.second(pidCollectInfo);
    }
    return UBSE_OK;
}

void ProcessMemPidCollect::CollectChildPids(pid_t parentPid, const def::ProcessMemPidConfigInfo& parentConfig,
                                            CollectInfoMap& pidCollectInfo)
{
    auto childrenPids = GetChildrenPids(parentPid);
    for (auto pid : childrenPids) {
        def::ProcessMemPidInfo childPidInfo{};
        childPidInfo.ppid = parentPid;
        childPidInfo.configInfo = parentConfig;
        childPidInfo.configInfo.pid = pid;
        childPidInfo.startTime = ProcessMemPidConfigManager::GetExactStartTime(pid);

        auto existingInfo = ProcessMemPidInfoManager::GetInstance().GetPidInfoMap(pid);
        bool alreadyConfigured =
            (existingInfo.configInfo.pid != -1 && existingInfo.startTime == childPidInfo.startTime);

        if (GetPidInfoByCollect(childPidInfo, pidCollectInfo) != UBSE_OK) {
            if (!alreadyConfigured) {
                ProcessMemPidInfoManager::GetInstance().UnsetPidInfo(pid);
            }
            continue;
        }

        if (!alreadyConfigured) {
            ProcessMemPidInfoManager::GetInstance().SetPidInfoMap(childPidInfo);
        }
    }
}

uint32_t ProcessMemPidCollect::RegisterCollectHandler(const std::string& name,
                                                      NumaMemDistributionCollectHandler handler)
{
    std::unique_lock<std::shared_mutex> lock(collectHandlersMutex);
    if (!handler) {
        UBSE_LOG_ERROR << "handler=" << name << " is nullptr";
        return UBSE_ERROR;
    }
    collectHandlers[name] = handler;
    return UBSE_OK;
}

void ProcessMemPidCollect::UnRegisterCollectHandler(const std::string& name)
{
    std::unique_lock<std::shared_mutex> lock(collectHandlersMutex);
    collectHandlers.erase(name);
}

size_t GetPageSize()
{
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize == -1) {
        UBSE_LOG_ERROR << "get sys page size failed, use 4096.";
        return DEFAULT_PAGE_SIZE;
    }
    return static_cast<size_t>(pageSize);
}

bool GetNumaInfoFromToken(const std::string& token, const size_t pageSize,
                          std::unordered_map<uint32_t, size_t>& numaMemDistribution)
{
    constexpr size_t numaTokenPrefixLen = 2;
    if (token.size() > numaTokenPrefixLen && token[0] == 'N' && std::isdigit(token[1])) {
        size_t pos = token.find('=');
        if (pos != std::string::npos && pos > 1) {
            std::string nodeStr = token.substr(1, pos - 1);
            std::string pagesStr = token.substr(pos + 1);
            try {
                int numa = std::stoi(nodeStr);
                size_t pages = std::stoull(pagesStr);
                numaMemDistribution[numa] += pages * pageSize;
            } catch (const std::exception& e) {
                // 忽略非法格式
                return false;
            }
        }
    }
    return true;
}

void ParseLine(const std::string& line, std::unordered_map<uint32_t, size_t>& numaMemDistribution)
{
    std::stringstream numaOss(line);
    std::string token;
    size_t pageSize = GetPageSize();
    while (numaOss >> token) {
        GetNumaInfoFromToken(token, pageSize, numaMemDistribution);
    }
}

uint32_t ProcessMemPidCollect::CollectProcessNumaMemDistribution(
    pid_t pid, std::unordered_map<uint32_t, size_t>& numaMemDistribution)
{
    std::string pidPath = "/proc/" + std::to_string(pid) + "/numa_maps";

    std::ifstream file(pidPath);
    if (!file.is_open()) {
        UBSE_LOG_ERROR << "pidPath is " << pidPath << "pid=" << std::to_string(pid) << " numa_maps not exists";
        return UBSE_ERROR;
    }

    std::string line;
    while (std::getline(file, line)) {
        ParseLine(line, numaMemDistribution);
    }
    file.close();

    for (const auto& [key, value] : numaMemDistribution) {}
    return UBSE_OK;
}
} // namespace process_mem::collect
