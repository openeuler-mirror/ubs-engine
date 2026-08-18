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

#include <cerrno>
#include <cstring>

#include "ubse_error.h"
#include "ubse_logger.h"

#include "ubse_storage.h"
#include "ubse_storage_module.h"
namespace process_mem::manager {
UBSE_DEFINE_THIS_MODULE("process_mem");
uint64_t ProcessMemPidConfigManager::GetExactStartTime(pid_t pid)
{
    std::string path = "/proc/" + std::to_string(pid) + "/stat";
    FILE* file = fopen(path.c_str(), "r");
    if (!file) {
        UBSE_LOG_WARN << "GetExactStartTime: fopen failed for pid=" << pid << ", path=" << path << ", errno=" << errno
                      << " (" << std::strerror(errno) << ")";
        return 0;
    }

    unsigned long long starttime = 0;

    if (fscanf_s(file,
                 "%*d %*[^)] %*c %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*lu %*lu %*d %*d %*d %*d %*d %*d %llu",
                 &starttime) == 1) {
        if (fclose(file) != 0) {
            UBSE_LOG_WARN << "fclose_failed errno=" << errno << " error=" << std::strerror(errno);
        }
        return static_cast<uint64_t>(starttime);
    }

    UBSE_LOG_WARN << "GetExactStartTime: fscanf_s failed to parse stat for pid=" << pid << ", path=" << path
                  << ", errno=" << errno << " (" << std::strerror(errno) << ")";
    if (fclose(file) != 0) {
        UBSE_LOG_WARN << "fclose_failed errno=" << errno << " error=" << std::strerror(errno);
    }
    return 0;
}

namespace {
const std::string& GetProcMemKeyPrefix(bool isPid)
{
    return isPid ? def::PROC_MEM_PID_KEY_PREFIX : def::PROC_MEM_NAME_KEY_PREFIX;
}
} // namespace

uint32_t ProcessMemPidConfigManager::PersistProcMemConfig(const def::ProcessMemNewConfigInfo& config)
{
    ubse::serial::UbseSerialization serializer;
    auto ret = config.Serialize(serializer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Serialize ProcMemConfig failed for identifier=" << config.identifier;
        return UBSE_ERROR;
    }
    UbseByteBuffer byteBuffer{};
    byteBuffer.data = serializer.GetBuffer();
    byteBuffer.len = serializer.GetLength();
    const auto& prefix = GetProcMemKeyPrefix(config.isPid);
    ret = ubse::storage::UbseStoragePutData(prefix, config.identifier, &byteBuffer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseStoragePutData ProcMemConfig failed for prefix=" << prefix
                       << ", identifier=" << config.identifier << ", ret=" << ubse::log::FormatRetCode(ret);
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "PersistProcMemConfig: persisted prefix=" << prefix << ", identifier=" << config.identifier
                  << ", maxMemory=" << config.maxMemory << ", remoteRatio=" << config.remoteRatio;
    return UBSE_OK;
}

void ProcessMemPidConfigManager::GetAllPersistedProcMemConfigs(std::vector<def::ProcessMemNewConfigInfo>& configs)
{
    for (const auto& prefix : {def::PROC_MEM_PID_KEY_PREFIX, def::PROC_MEM_NAME_KEY_PREFIX}) {
        std::vector<std::string> keys;
        auto listRet = ubse::storage::UbseStorageListKeys(prefix, keys);
        if (listRet != UBSE_OK) {
            UBSE_LOG_WARN << "UbseStorageListKeys failed for prefix=" << prefix;
            continue;
        }
        for (const auto& key : keys) {
            std::vector<def::ProcessMemNewConfigInfo> tmpVec;
            auto ret = ubse::storage::UbseStorageQueryData(prefix, key, &tmpVec, QueryProcMemConfigCallback);
            if (ret == UBSE_OK && !tmpVec.empty()) {
                configs.push_back(tmpVec[0]);
            } else {
                UBSE_LOG_WARN << "UbseStorageQueryData failed for prefix=" << prefix << ", key=" << key
                              << ", ret=" << ubse::log::FormatRetCode(ret);
            }
        }
    }
}

uint32_t ProcessMemPidConfigManager::DeleteProcMemConfig(bool isPid, const std::string& identifier)
{
    const auto& prefix = GetProcMemKeyPrefix(isPid);
    auto ret = ubse::storage::UbseStorageDeleteData(prefix, identifier);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to delete ProcMem config for prefix=" << prefix << ", identifier=" << identifier
                       << ", ret=" << ubse::log::FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

uint32_t ProcessMemPidConfigManager::PersistFossilConfig(pid_t pid, const def::FossilPidConfigInfo& fossil)
{
    ubse::serial::UbseSerialization serializer;
    auto ret = fossil.Serialize(serializer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Serialize FossilConfig failed for pid=" << pid;
        return UBSE_ERROR;
    }
    UbseByteBuffer byteBuffer{};
    byteBuffer.data = serializer.GetBuffer();
    byteBuffer.len = serializer.GetLength();
    const std::string key = std::to_string(pid);
    ret = ubse::storage::UbseStoragePutData(def::PROC_MEM_FOSSIL_KEY_PREFIX, key, &byteBuffer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseStoragePutData FossilConfig failed for pid=" << pid
                       << ", ret=" << ubse::log::FormatRetCode(ret);
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "PersistFossilConfig: pid=" << pid << " name=" << fossil.name << " maxMemory=" << fossil.maxMemory
                  << " remoteRatio=" << fossil.remoteRatio;
    return UBSE_OK;
}

uint32_t ProcessMemPidConfigManager::DeleteFossilConfig(pid_t pid)
{
    auto ret = ubse::storage::UbseStorageDeleteData(def::PROC_MEM_FOSSIL_KEY_PREFIX, std::to_string(pid));
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to delete FossilConfig for pid=" << pid << ", ret=" << ubse::log::FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

void ProcessMemPidConfigManager::GetAllFossilConfigs(std::vector<std::pair<pid_t, def::FossilPidConfigInfo>>& fossils)
{
    std::vector<std::string> keys;
    auto listRet = ubse::storage::UbseStorageListKeys(def::PROC_MEM_FOSSIL_KEY_PREFIX, keys);
    if (listRet != UBSE_OK) {
        UBSE_LOG_WARN << "UbseStorageListKeys failed for prefix=" << def::PROC_MEM_FOSSIL_KEY_PREFIX;
        return;
    }
    for (const auto& key : keys) {
        std::vector<def::FossilPidConfigInfo> tmpVec;
        auto ret = ubse::storage::UbseStorageQueryData(def::PROC_MEM_FOSSIL_KEY_PREFIX, key, &tmpVec,
                                                       QueryFossilConfigCallback);
        if (ret == UBSE_OK && !tmpVec.empty()) {
            auto pidOpt = def::ParsePidFromIdentifier(key);
            if (pidOpt.has_value()) {
                fossils.emplace_back(pidOpt.value(), tmpVec[0]);
            }
        } else {
            UBSE_LOG_WARN << "UbseStorageQueryData failed for prefix=" << def::PROC_MEM_FOSSIL_KEY_PREFIX
                          << ", key=" << key << ", ret=" << ubse::log::FormatRetCode(ret);
        }
    }
}

void ProcessMemPidConfigManager::QueryFossilConfigCallback(const std::string& keyPrefix, const std::string& key,
                                                           const UbseByteBuffer& buff, void* ctx)
{
    if (buff.data == nullptr || buff.len == 0) {
        UBSE_LOG_ERROR << "QueryFossilConfigCallback failed, invalid buffer for keyPrefix=" << keyPrefix
                       << " key=" << key;
        return;
    }
    def::FossilPidConfigInfo fossil{};
    ubse::serial::UbseDeSerialization deserializer(buff.data, buff.len);
    auto ret = fossil.Deserialize(deserializer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Deserialize FossilConfig failed for keyPrefix=" << keyPrefix << " key=" << key;
        return;
    }
    auto fossils = static_cast<std::vector<def::FossilPidConfigInfo>*>(ctx);
    fossils->push_back(fossil);
}

void ProcessMemPidConfigManager::QueryProcMemConfigCallback(const std::string& keyPrefix, const std::string& key,
                                                            const UbseByteBuffer& buff, void* ctx)
{
    if (buff.data == nullptr || buff.len == 0) {
        UBSE_LOG_ERROR << "QueryProcMemConfigCallback failed, invalid buffer for keyPrefix=" << keyPrefix
                       << " key=" << key;
        return;
    }
    def::ProcessMemNewConfigInfo config{};
    ubse::serial::UbseDeSerialization deserializer(buff.data, buff.len);
    auto ret = config.Deserialize(deserializer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Deserialize ProcMemConfig failed for keyPrefix=" << keyPrefix << " key=" << key;
        return;
    }
    auto configs = static_cast<std::vector<def::ProcessMemNewConfigInfo>*>(ctx);
    configs->push_back(config);
}

} // namespace process_mem::manager
