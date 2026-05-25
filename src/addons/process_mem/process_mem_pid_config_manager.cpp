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
#include "process_mem_pid_config_manager.h"

#include <sys/stat.h>
#include <filesystem>

#include "ubse_error.h"
#include "ubse_logger.h"

#include "ubse_storage.h"
#include "src/framework/storage/ubse_storage_module.h"
namespace process_mem::manager {
UBSE_DEFINE_THIS_MODULE("process_mem");
static const std::string PID_CONFIG_KEY_PREFIX = "pidConfig_";
void ProcessMemPidConfigManager::PersistPidConfigInfo(const def::ProcessMemPidInfo& pidInfo)
{
    std::string key = std::to_string(pidInfo.configInfo.pid);
    ubse::serial::UbseSerialization serializer;
    auto ret = pidInfo.SerializePidInfo(serializer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SerialPidDefInfo failed, " << ubse::log::FormatRetCode(ret);
        return;
    }
    UbseByteBuffer byteBuffer{};
    byteBuffer.data = serializer.GetBuffer();
    byteBuffer.len = serializer.GetLength();
    ret = ubse::storage::UbseStoragePutData(PID_CONFIG_KEY_PREFIX, key, &byteBuffer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseStoragePutData failed, " << ubse::log::FormatRetCode(ret);
        return;
    }
}

void ProcessMemPidConfigManager::GetAllPersistedPidConfigInfo(std::vector<def::ProcessMemPidInfo>& pidInfos)
{
    std::vector<std::string> fileNames{};
    try {
        // 1. 遍历目录
        for (const auto& entry : std::filesystem::directory_iterator(ubse::storage::DB_STORE_DIR)) {
            // 2. 过滤：只获取普通文件（排除文件夹）
            if (entry.is_regular_file()) {
                // 3. 获取文件名（不含路径）
                std::string fileName = entry.path().filename().string();
                if (fileName.size() < PID_CONFIG_KEY_PREFIX.size()) {
                    continue;
                }
                if (fileName.compare(0, PID_CONFIG_KEY_PREFIX.size(), PID_CONFIG_KEY_PREFIX) != 0) {
                    continue;
                }
                fileNames.push_back(fileName);
                UBSE_LOG_INFO << "Get one pid file:" << fileName;
            }
        }
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Failed to read directory: " << ubse::storage::DB_STORE_DIR << ", error: " << e.what();
        return;
    }

    // 遍历所有文件获取所有的pid
    auto ret = UBSE_OK;
    for (const auto& pidFileName : fileNames) {
        auto pidStr = pidFileName.substr(PID_CONFIG_KEY_PREFIX.size());
        ret = ubse::storage::UbseStorageQueryData(PID_CONFIG_KEY_PREFIX, pidStr, &pidInfos, QueryPidConfigCallback);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to query pid config for pid: " << pidStr;
            continue;
        }
    }
    return;
}

uint64_t ProcessMemPidConfigManager::GetExactStartTime(pid_t pid)
{
    std::string path = "/proc/" + std::to_string(pid) + "/stat";
    FILE* file = fopen(path.c_str(), "r");
    if (!file) {
        return 0;
    }

    char buf[1024];
    unsigned long long starttime = 0; // 使用 unsigned long long 接收

    // 使用 %*[^)] 跳过带空格的进程名, 使用 %llu 匹配 64 位无符号整数
    if (fscanf_s(file,
                 "%*d %*[^)] %*c %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*lu %*lu %*d %*d %*d %*d %*d %*d %llu",
                 &starttime) == 1) {
        fclose(file);
        return static_cast<uint64_t>(starttime);
    }

    fclose(file);
    return 0;
}

void ProcessMemPidConfigManager::QueryPidConfigCallback(const std::string& keyPrefix, const std::string& key,
                                                        const UbseByteBuffer& buff, void* ctx)
{
    if (buff.data == nullptr || buff.len == 0) {
        UBSE_LOG_ERROR << "QueryPidConfigCallback failed, invalid buffer";
        return;
    }
    def::ProcessMemPidInfo pidInfo{};
    ubse::serial::UbseDeSerialization deserializer(buff.data, buff.len);
    auto ret = pidInfo.DeserializePidInfo(deserializer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "DeSerialPidDefInfo failed, " << ubse::log::FormatRetCode(ret);
        return;
    }
    auto pidInfos = static_cast<std::vector<def::ProcessMemPidInfo>*>(ctx);
    pidInfos->push_back(pidInfo);
    return;
}

bool ProcessMemPidConfigManager::CheckPidConfigInfo(const def::ProcessMemPidInfo& pidInfo)
{
    auto exactStartTime = GetExactStartTime(pidInfo.configInfo.pid);
    if (exactStartTime == 0) {
        UBSE_LOG_ERROR << "Failed to get exact start time for pid: " << pidInfo.configInfo.pid;
        return false;
    }
    return exactStartTime == pidInfo.startTime;
}

void ProcessMemPidConfigManager::DeletePidConfigInfo(pid_t pid)
{
    auto pidStr = std::to_string(pid);
    auto ret = ubse::storage::UbseStorageDeleteData(PID_CONFIG_KEY_PREFIX, pidStr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to delete pid config for pid: " << pidStr << ", " << ubse::log::FormatRetCode(ret);
    }
    UBSE_LOG_INFO << "Delete pid config for pid: " << pidStr;
    return;
}

bool ProcessMemPidConfigManager::IsPidInfoExist(pid_t pid, uint64_t startTime)
{
    auto exactStartTime = GetExactStartTime(pid);
    if (exactStartTime == 0) {
        return false;
    }
    return exactStartTime == startTime;
}

} // namespace process_mem::manager