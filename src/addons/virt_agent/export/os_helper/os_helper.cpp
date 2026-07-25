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

#include "os_helper.h"

#include <dirent.h>
#include <fstream>
#include <regex>

#include <ubse_logger.h>
#include "vm_string_util.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;

std::string OsHelper::procPathPrefix = "/proc";

/**
 * Read process information under /proc directory and find PIDs containing "containerd" via cgroup files. Currently,
 * only containerd runtime container PIDs are supported.
 * @param containerIds   [IN]  set of container IDs to be queried
 * @param containerInfos [OUT] mapping from container ID to a list of process IDss
 * @return 0 for success, non-zero for error
 */
VmResult OsHelper::GetPidsByContainerIds(const std::unordered_set<std::string>& containerIds,
                                         std::unordered_map<std::string, std::vector<pid_t>>& containerInfos)
{
    DIR* dir = opendir(OsHelper::procPathPrefix.c_str());
    if (dir == nullptr) {
        UBSE_LOG_ERROR << "Read " << OsHelper::procPathPrefix << " failed.";
        return VM_ERROR;
    }
    const struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        // Only process directories that start with a digit (i.e., PID directories)
        if (ent->d_type != DT_DIR || !isdigit(ent->d_name[0])) {
            continue;
        }

        std::string pidStr(ent->d_name);
        std::string cgroupPath = OsHelper::procPathPrefix;
        cgroupPath.append("/").append(pidStr).append("/cgroup");

        std::string containerId = ParseContainerFile(cgroupPath);
        if (!containerId.empty() && containerIds.count(containerId)) {
            try {
                auto pid = VmStringUtil::SafeNotEmptyStopid(pidStr);
                containerInfos[containerId].emplace_back(pid);
            } catch (const std::exception& e) {
                UBSE_LOG_ERROR << "containerInfos emplace failed: " << e.what();
                closedir(dir);
                return VM_ERROR;
            }
        }
    }
    closedir(dir);
    return VM_OK;
}

std::string OsHelper::ParseContainerFile(const std::string& cgroupPath)
{
    std::ifstream file(cgroupPath);
    if (!file.is_open()) {
        std::error_code ec(errno, std::system_category());
        UBSE_LOG_ERROR << "file open failed " << cgroupPath << ": " << ec.message();
        return "";
    }
    std::string line;
    std::regex containerdRegex("cri-containerd-([0-9a-f]{64})(?:\\.scope)?$");

    while (std::getline(file, line)) {
        size_t lastColon = line.find_last_of(':');
        if (lastColon == std::string::npos) {
            continue;
        }
        std::string path = line.substr(lastColon + 1);
        if (path.find("containerd") != std::string::npos) {
            std::smatch match;
            if (std::regex_search(path, match, containerdRegex) && match.size() > 1) {
                file.close();
                return match[1].str();
            }
        }
    }
    file.close();
    return "";
}

} // namespace vm