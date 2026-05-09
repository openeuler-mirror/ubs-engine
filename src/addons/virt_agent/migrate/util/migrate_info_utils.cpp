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
#include "migrate_info_utils.h"

#include <filesystem>
#include <fstream>
#include <regex>

#include <dirent.h>

#include <ubse_logger.h>

#include "vm_file_util.h"
#include "vm_string_util.h"

namespace vm {
using namespace ubse::log;
using std::ifstream;
using std::regex;
using std::stringstream;

string MigrateInfoUtil::cpuSocketPath = "/topology/physical_package_id";
string MigrateInfoUtil::numaPathPrefix = "/sys/devices/system/node";
string MigrateInfoUtil::procPathPrefix = "/proc";
/**
 * Read process information from the /proc directory and find the virtual machine's PID based on the given UUID
 * @param uuid const string &: Virtual machine UUID
 * @return PID uint16_t: Virtual machine process PID
 */
pid_t MigrateInfoUtil::GetPidByVmUUID(const string &uuid)
{
    pid_t pid = -1;
    DIR *dir = opendir(MigrateInfoUtil::procPathPrefix.c_str());
    if (dir == nullptr) {
        return pid;
    }
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type != DT_DIR || !isdigit(ent->d_name[0])) {
            continue; // Skip non-digit starting entries, which are not PIDs
        }
        stringstream ssPath;
        ssPath << MigrateInfoUtil::procPathPrefix << "/" << string(ent->d_name) << "/cmdline";
        vector<string> infoLines;
        if (const auto ret = VmFileUtil::GetFileInfo(ssPath.str(), infoLines);
            ret != VM_OK || infoLines.empty() || infoLines[0].find(uuid) == string::npos) {
            continue;
        }
        try {
            pid = VmStringUtil::SafeStoi32(ent->d_name);
        } catch (const std::exception &e) {
            // Conversion failed, return -1
        }
        closedir(dir);
        return pid;
    }

    closedir(dir);
    return pid;
}

VmResult MigrateInfoUtil::GetNumaIdAndPageSizeByPid(const pid_t pid, MigrateInfoBase &numaIdAndPageSize)
{
    auto path = MigrateInfoUtil::procPathPrefix + "/" + std::to_string(pid) + "/numa_maps";
    std::ifstream file(path);
    if (!file.is_open()) {
        return VM_ERROR;
    }
    string line;
    // Data in the same line: N0=15232 N5=669 indicates that NUMA node 0 uses 15232 2M huge pages, and NUMA node 5
    // uses 669 2M huge pages.In the openEuler22 OS version, N0=15232 and N5=669 are in different lines.
    regex n1Regex("N(\\d+)=(\\d+)");
    regex n2Regex("kernelpagesize_kB=(\\d+)");
    auto startTime = std::chrono::high_resolution_clock::now();
    while (std::getline(file, line)) {
        std::smatch n1Match;
        std::smatch n2Match;
        auto it = line.find("/libvirt");
        if (it == std::string::npos) {
            continue;
        }
        line.erase(0, it);
        if (std::regex_search(line, n1Match, n1Regex) && std::regex_search(line, n2Match, n2Regex)) {
            try {
                numaIdAndPageSize.numaId = VmStringUtil::SafeStoi32(n1Match[1]);
                numaIdAndPageSize.pageSize = VmStringUtil::SafeStoi32(n2Match[1]);
            } catch (const std::exception &e) {
                file.close();
                return VM_ERROR;
            }
            file.close();
            return VM_OK;
        }
    }
    file.close();
    return VM_ERROR;
}

VmResult MigrateInfoUtil::GetSocketIdByNumaId(const uint32_t numaId, uint32_t *socketId)
{
    if (socketId == nullptr) {
        return VM_ERROR;
    }
    string cpuPath;
    cpuPath.append(numaPathPrefix)
        .append(&std::filesystem::path::preferred_separator, 1)
        .append("node" + std::to_string(numaId))
        .append(&std::filesystem::path::preferred_separator, 1)
        .append("cpulist");
    std::vector<std::string> nodeInfo{};
    if (const auto ret = VmFileUtil::GetFileInfo(cpuPath, nodeInfo); ret != VM_OK) {
        return ret;
    }
    // If the NUMA node has no bound CPUs, it is not a local NUMA node; skip collection
    if (nodeInfo.size() != 1) {
        return VM_WARN;
    }
    // Obtain cpu info
    if (nodeInfo[0].empty()) {
        return VM_WARN;
    }
    size_t dashPos = nodeInfo[0].find('-');
    int16_t cpuNum = -1;
    try {
        cpuNum = VmStringUtil::SafeStoi16(nodeInfo[0].substr(0, dashPos));
    } catch (const std::exception &e) {
        return VM_ERROR;
    }
    std::vector<std::string> cpuSocket{};
    string socketPath;
    socketPath.append(numaPathPrefix)
        .append(&std::filesystem::path::preferred_separator, 1)
        .append("node" + std::to_string(numaId))
        .append(&std::filesystem::path::preferred_separator, 1)
        .append("cpu" + std::to_string(cpuNum))
        .append(cpuSocketPath);
    if (const auto ret = VmFileUtil::GetFileInfo(socketPath, cpuSocket); ret != VM_OK) {
        return ret;
    }
    if (cpuSocket.empty() || cpuSocket[0].empty()) {
        return VM_WARN;
    }
    try {
        *socketId = VmStringUtil::SafeStoi32(cpuSocket[0]);
    } catch (const std::exception &e) {
        return VM_ERROR;
    }
    return VM_OK;
}
} // namespace vm
