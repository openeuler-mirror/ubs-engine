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

#include <dirent.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "ubse_logger.h"
#include "OsHelper.h"
#include "mp_configuration.h"
#include "mp_file_util.h"
#include "mp_string_util.h"

namespace mempooling::exportV2 {
using namespace ubse::log;
namespace fs = std::filesystem;

const std::string SUB_MODULE_NAME = "[OsHelper] ";

#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME

const std::int16_t VM_PAGESIZE_IDX = 1;
const std::int16_t LOCAL_NUMA_ID_IDX = 1;
const std::int16_t LOCAL_NUMA_MEM_IDX = 2;
const std::int16_t REMOTE_NUMA_ID_IDX = 3;
const std::int16_t REMOTE_NUMA_MEM_IDX = 4;
const std::int16_t HUGE_PAGE_NUM_TO_KB = 2048;
const std::int16_t VM_SINGLE_NUMA = 1;
const std::int16_t VM_DOUBLE_NUMA = 2;

const std::string numaPathPrefix = "/sys/devices/system/node/";
const std::string cpuPathPrefix = "/sys/devices/system/cpu/";
const std::string socketPathSuffix = "/topology/physical_package_id";
const std::string qemuPathPrefix = "/var/run/libvirt/qemu/";
const std::string procPathPrefix = "/proc/";
const unsigned int startTimeIndexInProc = 21;

MpResult OsHelper::GetNumaSet(std::vector<std::uint16_t>& numaSet)
{
    numaSet.clear();
    std::set<std::uint16_t> st;
    const fs::path numaPath(numaPathPrefix);
    if (!fs::exists(numaPath) || !fs::is_directory(numaPath)) {
        LOG_ERROR << "Numa path not exist or not directory, numaPath=" << numaPathPrefix;
        return MEM_POOLING_ERROR;
    }
    for (const auto& entry : fs::directory_iterator(numaPath)) {
        if (entry.is_directory() && entry.path().filename().string().find("node") == 0) {
            std::string idStr = entry.path().filename().string().substr(4);
            try {
                auto numaId = std::stoi(idStr);
                if (numaId >= 0 && numaId <= std::numeric_limits<std::uint16_t>::max()) {
                    st.insert(static_cast<std::uint16_t>(numaId));
                }
            } catch (const std::exception& e) {
                LOG_ERROR << "Invalid NUMA node directory: " << entry.path();
                return MEM_POOLING_ERROR;
            }
        }
    }
    if (st.empty()) {
        LOG_ERROR << "NumaSet is empty.";
        return MEM_POOLING_ERROR;
    }
    numaSet.insert(numaSet.end(), st.begin(), st.end());
    return MEM_POOLING_OK;
}

MpResult OsHelper::GetVmNameSet(std::vector<std::string>& vmNameSet)
{
    vmNameSet.clear();
    std::set<std::string> st;
    for (auto& entry : fs::directory_iterator(qemuPathPrefix)) {
        if (entry.is_regular_file() && entry.path().extension() == ".xml") {
            std::string filename = entry.path().stem().string();
            st.insert(filename);
        }
    }
    vmNameSet.insert(vmNameSet.end(), st.begin(), st.end());
    return MEM_POOLING_OK;
}

MpResult OsHelper::GetHostName(std::string& hostName)
{
    char hostname[HOST_NAME_MAX + 1];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        hostName = std::string(hostname);
        return MEM_POOLING_OK;
    } else {
        LOG_ERROR << "Failed to Get hostname, hostName is void.";
        hostName = "";
        return MEM_POOLING_ERROR;
    }
}

MpResult OsHelper::IsNumaLocal(const std::uint16_t& numaId, bool& isLocal)
{
    std::string filePath = numaPathPrefix + "/node" + std::to_string(numaId) + "/remote";
    std::ifstream file(filePath);
    if (!file.is_open()) {
        LOG_ERROR << "Cannot open file at " << filePath << ": " << std::strerror(errno) << ".";
        isLocal = false;
        return MEM_POOLING_ERROR;
    }
    std::string remoteStr;
    if (!(file >> remoteStr)) {
        LOG_ERROR << "Failed to read content from " << filePath << ".";
        file.close();
        return false;
    }
    file.close();
    isLocal = (remoteStr == "0");
    return MEM_POOLING_OK;
}

MpResult OsHelper::GetSocketIdByNumaId(const std::uint16_t& numaId, std::int16_t& socketId)
{
    socketId = -1;
    std::string filePath = numaPathPrefix + "/node" + std::to_string(numaId) + "/cpulist";
    std::ifstream file(filePath);
    if (!file.is_open()) {
        LOG_ERROR << "Cannot open file at " << filePath << ": " << std::strerror(errno) << ".";
        return MEM_POOLING_ERROR;
    }
    std::string cpulistStr = "";
    if (!(file >> cpulistStr) || cpulistStr.empty()) {
        LOG_ERROR << "Failed to read content from " << filePath << ", content = " << cpulistStr << ".";
        file.close();
        return MEM_POOLING_ERROR;
    }
    file.close();
    // 取该numa绑定的第一个cpu的socketId作为改numa的socketId
    auto cpulist = parseCpuList(cpulistStr);
    if (cpulist.empty()) {
        LOG_ERROR << "Cpulist is empty, cpulistStr = " << cpulistStr << ".";
        return MEM_POOLING_ERROR;
    }
    std::string socketFilePath = cpuPathPrefix + "/cpu" + std::to_string(cpulist[0]) + socketPathSuffix;
    std::ifstream socketFile(socketFilePath);
    if (!socketFile.is_open()) {
        LOG_ERROR << "Cannot open file at " << socketFilePath << ": " << std::strerror(errno) << ".";
        return MEM_POOLING_ERROR;
    }
    std::string socketIdStr = "";
    if (!(socketFile >> socketIdStr) || socketIdStr.empty()) {
        LOG_ERROR << "Failed to read content from " << socketFilePath << ", content = " << socketIdStr << ".";
        socketFile.close();
        return MEM_POOLING_ERROR;
    }
    socketFile.close();
    if (MpStringUtil::SafeStoi16(socketIdStr, socketId)) {
        LOG_ERROR << "SocketId Stoi16 failed, socketIdStr = " << socketIdStr << ".";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult OsHelper::GetMemInfoByNumaId(const std::uint16_t& numaId, NumaInfo& info)
{
    // 1. 读取普通内存信息（meminfo）
    std::string filePath = numaPathPrefix + "/node" + std::to_string(numaId) + "/meminfo";
    std::ifstream meminfo(filePath);
    if (!meminfo.is_open()) {
        LOG_ERROR << "Cannot open file at " << filePath << ": " << std::strerror(errno) << ".";
        return MEM_POOLING_ERROR;
    }
    std::string line;
    std::regex re(R"(Node\s+\d+\s+(\S+):\s+(\d+))");
    std::smatch m;
    while (std::getline(meminfo, line)) {
        if (std::regex_search(line, m, re)) {
            std::string key = m[1];
            std::string val = m[2];
            if (key == "MemTotal") {
                info.metaData.memTotal = MpStringUtil::SafeStoullOld(val);
            } else if (key == "MemFree") {
                info.metaData.memFree = MpStringUtil::SafeStoullOld(val);
            }
        }
    }
    meminfo.close();

    // 2. 读取 hugepage 信息（支持多 pageSize）
    std::string hugepageBase = numaPathPrefix + "/node" + std::to_string(numaId) + "/hugepages";

    for (const auto& entry : std::filesystem::directory_iterator(hugepageBase)) {
        // hugepages-2048kB
        std::string dirName = entry.path().filename().string();

        static const std::regex hugeDirRe(R"(hugepages-(\d+)kB)");
        std::smatch match;
        if (!std::regex_match(dirName, match, hugeDirRe)) {
            continue;
        }
        uint64_t pageSizeKB = MpStringUtil::SafeStoullOld(match[1]);
        NumaPageData pageData;
        pageData.pageSize = pageSizeKB;
        // nr_hugepages
        std::ifstream totalFile(entry.path() / "nr_hugepages");
        if (totalFile.is_open()) {
            totalFile >> pageData.hugePageTotal;
            totalFile.close();
        }
        // free_hugepages
        std::ifstream freeFile(entry.path() / "free_hugepages");
        if (freeFile.is_open()) {
            freeFile >> pageData.hugePageFree;
            freeFile.close();
        }
        info.metaData.numaPageInfo[pageSizeKB] = pageData;
    }

    return MEM_POOLING_OK;
}

MpResult OsHelper::GetVmPidByVmName(const std::string& vmName, pid_t& pid)
{
    pid = -1;
    std::string filePath = qemuPathPrefix + vmName + ".pid";
    auto lineInfo = std::vector<std::string>();
    auto ret = MpFileUtil::GetFileInfo(filePath, lineInfo);
    if (ret != MEM_POOLING_OK || lineInfo.empty()) {
        LOG_ERROR << "Open the pid file of vmName = " << vmName << " failed.";
        return ret;
    }
    std::string pidStr = lineInfo[0];
    if (pidStr.empty()) {
        LOG_ERROR << "Failed to read content from " << filePath << ".";
        return MEM_POOLING_ERROR;
    }
    ret = MpStringUtil::SafeStopid(pidStr, pid);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "PidStr Stoi64 failed, pidStr = " << pidStr << ", ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult OsHelper::GetProcessStartTimeByPid(const pid_t& pid, std::time_t& startTime)
{
    startTime = -1;
    // 构建目标进程的stat文件路径
    std::stringstream ssPath;
    ssPath << procPathPrefix << pid << "/stat";

    // 打开指定PID的stat文件
    auto lineInfo = std::vector<std::string>();
    auto ret = MpFileUtil::GetFileInfo(ssPath.str(), lineInfo);
    if (ret != MEM_POOLING_OK || lineInfo.empty()) {
        LOG_ERROR << "Open the stat file of pid = " << pid << " failed.";
        return ret;
    }
    std::string line = lineInfo[0];
    std::istringstream iss(line);
    std::string token;
    for (size_t i = 0; i < startTimeIndexInProc; ++i) {
        iss >> token;
    }
    // 读取启动时间（单位为jiffies）
    unsigned long startTimeInJiffies;
    iss >> startTimeInJiffies;

    // 转换jiffies到秒
    clock_t ticksPerSecond = sysconf(_SC_CLK_TCK);
    if (ticksPerSecond == -1) {
        LOG_ERROR << "Failed to get ticksPerSecond.";
        return MEM_POOLING_ERROR;
    }

    // 获取系统启动时间
    std::ifstream uptimeFile("/proc/uptime");
    double uptimeInSec;
    if (!(uptimeFile >> uptimeInSec)) {
        return MEM_POOLING_ERROR;
    }
    time_t currentTime = std::time(nullptr);
    time_t sysStartTime = currentTime - static_cast<time_t>(uptimeInSec);

    // 计算进程启动时间
    startTime = sysStartTime + static_cast<time_t>(static_cast<long long>(startTimeInJiffies) /
                                                   std::abs(static_cast<long long>(ticksPerSecond)));
    // 格式化日期和时间
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&startTime), "%Y-%m-%d %H:%M:%S");
    std::string dtime = oss.str();

    return MEM_POOLING_OK;
}

MpResult OsHelper::GetVmNumaInfoByPid(const pid_t& pid, VmDomainInfo& info)
{
    std::string filePath = procPathPrefix + std::to_string(pid) + "/numa_maps";
    std::ifstream file(filePath);
    if (!file.is_open()) {
        LOG_ERROR << "Cannot open file at " << filePath << ": " << std::strerror(errno) << ".";
        return MEM_POOLING_ERROR;
    }

    std::string line;
    static const std::regex pagesizeRegex(R"(.*libvirt.*kernelpagesize_kB=(\d+))");
    static const std::regex numaRegex(R"(N(\d+)=([0-9]+))");

    while (std::getline(file, line)) {
        /* 解析 pageSize */
        std::smatch pagesizeMatch;
        if (line.find("libvirt") == std::string::npos || line.find("qemu") == std::string::npos ||
            line.find("dirty") == std::string::npos) {
            continue;
        }
        if (!std::regex_search(line, pagesizeMatch, pagesizeRegex)) {
            continue; // 没有页大小，跳过
        }
        uint64_t pageSizeKB = MpStringUtil::SafeStoullOld(pagesizeMatch[1]);

        /* 解析 NUMA 分布 */
        for (auto it = std::sregex_iterator(line.begin(), line.end(), numaRegex); it != std::sregex_iterator(); ++it) {
            int16_t numaId = MpStringUtil::SafeStoi16Old((*it)[1]);
            uint64_t pages = MpStringUtil::SafeStoullOld((*it)[2]);
            int64_t usedMemKB = static_cast<int64_t>(pages * pageSizeKB);

            auto& existing = info.numaInfo[numaId];

            /* 第一次出现该 numaId */
            if (existing.usedMem == 0) {
                existing.numaId = numaId;
                existing.pageSize = pageSizeKB;
                existing.usedMem = usedMemKB;

                MpResult ret = IsNumaLocal(numaId, existing.isLocal);
                if (ret != MEM_POOLING_OK) {
                    LOG_ERROR << "IsNumaLocal failed, numaId=" << numaId;
                    return ret;
                }

                if (existing.isLocal) {
                    ret = GetSocketIdByNumaId(numaId, existing.socketId);
                    if (ret != MEM_POOLING_OK) {
                        LOG_ERROR << "GetSocketIdByNumaId failed, numaId=" << numaId;
                        return ret;
                    }
                } else {
                    existing.socketId = -1;
                }
            } else { /* 已存在该 numaId */
                /* 校验 pageSize 不冲突 */
                if (existing.pageSize != pageSizeKB) {
                    LOG_ERROR << "PageSize conflict on numaId=" << numaId << ", old=" << existing.pageSize
                              << "KB, new=" << pageSizeKB << "KB";
                    return MEM_POOLING_ERROR;
                }
                existing.usedMem += usedMemKB;
            }
        }
    }

    file.close();

    if (info.numaInfo.empty()) {
        LOG_ERROR << "No NUMA info parsed for pid=" << pid;
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

vector<uint16_t> OsHelper::parseCpuList(const string& line)
{
    std::vector<uint16_t> cpus;
    std::stringstream ss(line);
    std::string token;
    while (getline(ss, token, ',')) {
        try {
            size_t dashPos = token.find('-');
            if (dashPos != string::npos) {
                auto start = stoi(token.substr(0, dashPos));
                auto end = stoi(token.substr(dashPos + 1));
                for (auto i = start; i <= end; ++i) {
                    cpus.push_back(i);
                }
            } else {
                cpus.push_back(stoi(token));
            }
        } catch (const std::exception& e) {
            continue; // 非法格式，跳过
        }
    }
    return cpus;
}

} // namespace mempooling::exportV2