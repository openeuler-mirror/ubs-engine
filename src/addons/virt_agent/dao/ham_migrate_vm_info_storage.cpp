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

#include "ham_migrate_vm_info_storage.h"

#include <ubse_error.h>
#include <ubse_storage.h>
#include <ubse_logger.h>
#include "ham_migrate_vm_info_message.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("vm_plugin");
using namespace ubse::log;
static const std::string HAM_MIGRATE_KEY_PREFIX = "ubse_";
static const std::string HAM_MIGRATE_KEY = "ham_migrate";
ReadWriteLock HamMigrateVmInfoStorage::hamMigrateLock{};
std::vector<HamMigrateVmInfo> HamMigrateVmInfoStorage::hamMigrateCache{};

VmResult HamMigrateVmInfoStorage::SetHamMigrateVmInfo(const HamMigrateVmInfo &hamMigrateVmInfo)
{
    return OpHamMigrate(hamMigrateCache, [&hamMigrateVmInfo](std::vector<HamMigrateVmInfo> &vmInfos) {
        UBSE_LOG_DEBUG << "[StoreHamMigrates] updated, nodeId = " << hamMigrateVmInfo.nodeId
                       << ", pid = " << hamMigrateVmInfo.pid;
        vmInfos.erase(remove(vmInfos.begin(), vmInfos.end(), hamMigrateVmInfo), vmInfos.end());
        vmInfos.emplace_back(hamMigrateVmInfo);
    });
}

VmResult HamMigrateVmInfoStorage::SetHamMigrateVmInfos(const std::vector<HamMigrateVmInfo> &hamMigrateVmInfos)
{
    return OpHamMigrate(hamMigrateCache, [&hamMigrateVmInfos](std::vector<HamMigrateVmInfo> &vmInfos) {
        for (const auto &hamMigrateVmInfo : hamMigrateVmInfos) {
            UBSE_LOG_DEBUG << "[StoreHamMigrates] updated, nodeId = " << hamMigrateVmInfo.nodeId
                           << ", pid = " << hamMigrateVmInfo.pid;
            vmInfos.erase(remove(vmInfos.begin(), vmInfos.end(), hamMigrateVmInfo), vmInfos.end());
        }
        vmInfos.insert(vmInfos.end(), hamMigrateVmInfos.begin(), hamMigrateVmInfos.end());
    });
}

VmResult HamMigrateVmInfoStorage::GetHamMigrateVmInfo(const std::string &nodeId, int pid,
                                                      HamMigrateVmInfo &hamMigrateVmInfo)
{
    ReadLocker<ReadWriteLock> lock(&hamMigrateLock);
    for (const auto &vmInfo : hamMigrateCache) {
        if (nodeId == vmInfo.nodeId && pid == vmInfo.pid) {
            hamMigrateVmInfo = vmInfo;
            UBSE_LOG_INFO << "[GetHamMigrate] nodeId = " << nodeId << ", pid = " << pid
                          << ", config = " << hamMigrateVmInfo.ToString();
            return VM_OK;
        }
    }
    UBSE_LOG_WARN << "[GetHamMigrate] can not find vmInfo, pid = " << pid << ", nodeId = " << nodeId;
    return VM_ERROR;
}

void HamMigrateVmInfoStorage::GetHamMigrateVmInfos(const std::string &nodeId,
                                                   std::vector<HamMigrateVmInfo> &hamMigrateVmInfos)
{
    ReadLocker<ReadWriteLock> lock(&hamMigrateLock);
    for (const auto &vmInfo : hamMigrateCache) {
        if (nodeId == vmInfo.nodeId) {
            hamMigrateVmInfos.emplace_back(vmInfo);
        }
    }
    UBSE_LOG_INFO << "[GetHamMigrate] nodeId = " << nodeId << ", size = " << hamMigrateVmInfos.size();
}

VmResult HamMigrateVmInfoStorage::DelHamMigrateVmInfo(const std::string &nodeId, pid_t pid)
{
    HamMigrateVmInfo hamMigrateVmInfo;
    hamMigrateVmInfo.nodeId = nodeId;
    hamMigrateVmInfo.pid = pid;
    return OpHamMigrate(hamMigrateCache, [&hamMigrateVmInfo](std::vector<HamMigrateVmInfo> &vmInfos) {
        UBSE_LOG_DEBUG << "[StoreHamMigrates] deleted, nodeId = " << hamMigrateVmInfo.nodeId
                       << ", pid = " << hamMigrateVmInfo.pid;
        vmInfos.erase(remove(vmInfos.begin(), vmInfos.end(), hamMigrateVmInfo), vmInfos.end());
    });
}

VmResult HamMigrateVmInfoStorage::OpHamMigrate(std::vector<HamMigrateVmInfo> &hamMigrateVmInfos,
                                               const std::function<void(std::vector<HamMigrateVmInfo> &)> &func)
{
    UBSE_LOG_INFO << "[SaveHamMigrates] start.";
    UBSE_LOG_DEBUG << "[SaveHamMigrates] hamMigrateVmInfos size = " << hamMigrateVmInfos.size();

    HamMigrateVmInfoMessage dataMessage;
    WriteLocker<ReadWriteLock> lock(&hamMigrateLock);

    if (func) {
        func(hamMigrateVmInfos);
    }

    dataMessage.SetData(hamMigrateVmInfos);

    auto ret = dataMessage.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[SaveHamMigrates] serialize failed, " << FormatRetCode(ret);
        return VM_ERROR;
    }

    UbseByteBuffer buffer{
        .data = dataMessage.SerializedData(), .len = dataMessage.SerializedDataSize(), .freeFunc = nullptr};

    ret = UbseStoragePutData(HAM_MIGRATE_KEY_PREFIX, HAM_MIGRATE_KEY, &buffer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[SaveHamMigrates] put failed, " << FormatRetCode(ret);
        return VM_ERROR;
    }

    UBSE_LOG_DEBUG << "[SaveHamMigrates] put succeed, data = " << ToString(dataMessage.GetData());

    UBSE_LOG_INFO << "[SaveHamMigrates] end.";
    return VM_OK;
}

VmResult HamMigrateVmInfoStorage::GetAllHamMigrateVmInfos(std::vector<HamMigrateVmInfo> &hamMigrateVmInfos)
{
    ReadLocker<ReadWriteLock> lock(&hamMigrateLock);
    auto ret = UbseStorageQueryData(HAM_MIGRATE_KEY_PREFIX, HAM_MIGRATE_KEY, &hamMigrateVmInfos, QueryHandler);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[GetHamMigrates] failed, " << FormatRetCode(ret);
        return ret;
    }
    hamMigrateCache.clear();
    hamMigrateCache.insert(hamMigrateCache.end(), hamMigrateVmInfos.begin(), hamMigrateVmInfos.end());
    UBSE_LOG_INFO << "[GetHamMigrates] size = " << hamMigrateVmInfos.size();
    return ret;
}

void HamMigrateVmInfoStorage::GetHamMigrateVmInfosByDstNodeId(const std::string &dstNodeId,
                                                              std::vector<HamMigrateVmInfo> &hamMigrateVmInfos)
{
    ReadLocker<ReadWriteLock> lock(&hamMigrateLock);
    for (const auto &vmInfo : hamMigrateCache) {
        if (vmInfo.dstNodeId == dstNodeId) {
            hamMigrateVmInfos.emplace_back(vmInfo);
        }
    }
    UBSE_LOG_INFO << "[GetHamMigrate] dstNodeId = " << dstNodeId << ", size = " << hamMigrateVmInfos.size();
}

void HamMigrateVmInfoStorage::QueryHandler(const std::string &keyPrefix, const std::string &key,
                                           const UbseByteBuffer &buff, void *ctx)
{
    auto *hamMigrateVmInfos = static_cast<std::vector<HamMigrateVmInfo> *>(ctx);
    if (hamMigrateVmInfos == nullptr || buff.data == nullptr) {
        UBSE_LOG_WARN << "[GetHamMigrates] ctx or buff is nullptr.";
        return;
    }
    HamMigrateVmInfoMessage hamMigrateVmInfoSimpo{};
    auto ret = hamMigrateVmInfoSimpo.SetInputRawData(buff.data, buff.len);
    if (ret != VM_OK) {
        UBSE_LOG_WARN << "[GetHamMigrates] simpo set input failed.";
        return;
    }
    ret = hamMigrateVmInfoSimpo.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_WARN << "[GetHamMigrates] simpo deserialize failed.";
        return;
    }
    auto hamMigrateVmInfo = hamMigrateVmInfoSimpo.GetData();
    for (const auto &migrateVmInfo : hamMigrateVmInfo) {
        hamMigrateVmInfos->emplace_back(migrateVmInfo);
    }
}

std::string HamMigrateVmInfoStorage::ToString(const std::vector<HamMigrateVmInfo> &HamMigrateVmInfos)
{
    std::ostringstream oss;
    for (auto &hamMigrateVmInfo : HamMigrateVmInfos) {
        oss << "(";
        oss << hamMigrateVmInfo.ToString();
        oss << "),";
    }
    auto json = oss.str();
    if (!json.empty()) {
        json.pop_back();
    }
    return "[" + json + "]";
}
} // namespace vm