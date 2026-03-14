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

#include "migrate_state_storage.h"

#include <ubse_error.h>
#include <ubse_storage.h>
#include <ubse_logger.h>
#include "migrate_state_map_message.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("vm_plugin");
using namespace ubse::log;
static const std::string MIGRATE_STATE_KEY_PREFIX = "ubse_";
static const std::string MIGRATE_STATE_KEY = "vm_migrate_";
ReadWriteLock MigrateStateStorage::migrateStateLock{};

VmResult MigrateStateStorage::SaveMigrateState(NumaVMInfoMap &numaVmInfoMap, const VMBasicInfo &vmBasicInfo)
{
    return OpMigrateState(numaVmInfoMap, vmBasicInfo, false);
}

VmResult MigrateStateStorage::DelMigrateState(NumaVMInfoMap &numaVmInfoMap, const VMBasicInfo &vmBasicInfo)
{
    return OpMigrateState(numaVmInfoMap, vmBasicInfo, true);
}

VmResult MigrateStateStorage::OpMigrateState(NumaVMInfoMap &numaVmInfoMap, const VMBasicInfo &vmBasicInfo,
                                             const bool isDelete)
{
    UBSE_LOG_INFO << "[SaveMigrateStates] start.";
    UBSE_LOG_DEBUG << "[SaveMigrateStates] uuid = " << vmBasicInfo.uuid << ", isDelete = " << isDelete;

    MigrateStateMapMessage dataMessage;
    WriteLocker<ReadWriteLock> lock(&migrateStateLock);
    for (const auto& [numaId, vmDomainNumaInfo] : vmBasicInfo.numaMemInfo) {
        VMNodeLocInfo numaLoc{};
        numaLoc.socketId = vmDomainNumaInfo.socketId;
        numaLoc.numaId = vmDomainNumaInfo.numaId;
        numaLoc.hostId = vmBasicInfo.nodeId;
        numaLoc.hostName = vmBasicInfo.hostName;
        if (isDelete) {
            numaVmInfoMap[numaLoc].erase(vmBasicInfo.uuid);
        } else {
            numaVmInfoMap[numaLoc][vmBasicInfo.uuid] = vmBasicInfo;
        }
    }
    dataMessage.SetData(numaVmInfoMap);
    auto ret = dataMessage.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[SaveMigrateStates] serialize failed, " << FormatRetCode(ret);
        return VM_ERROR;
    }
    UbseByteBuffer buffer{
        .data = dataMessage.SerializedData(), .len = dataMessage.SerializedDataSize(), .freeFunc = nullptr};
    ret = UbseStoragePutData(MIGRATE_STATE_KEY_PREFIX, MIGRATE_STATE_KEY, &buffer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[SaveMigrateStates] put failed, " << FormatRetCode(ret);
        return VM_ERROR;
    }
    UBSE_LOG_DEBUG << "[SaveMigrateStates] put success, migrateStates = " << ToString(dataMessage.GetData());

    UBSE_LOG_INFO << "[SaveMigrateStates] end.";
    return VM_OK;
}

VmResult MigrateStateStorage::GetMigrateStates(NumaVMInfoMap &numaVmInfoMap)
{
    ReadLocker<ReadWriteLock> lock(&migrateStateLock);
    std::vector<NumaVMInfoMap> datas;
    auto ret = UbseStorageQueryData(MIGRATE_STATE_KEY_PREFIX, MIGRATE_STATE_KEY, &datas, QueryHandler);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[GetMigrateStates] failed, " << FormatRetCode(ret);
        return ret;
    }
    if (datas.empty()) {
        UBSE_LOG_WARN << "[GetMigrateStates] data is empty.";
        return ret;
    }
    numaVmInfoMap = datas[0];
    UBSE_LOG_DEBUG << "[GetMigrateStates] migrateStates = " << ToString(numaVmInfoMap);
    return VM_OK;
}

void MigrateStateStorage::QueryHandler(const std::string &keyPrefix, const std::string &key, const UbseByteBuffer &buff,
                                       void *ctx)
{
    auto *migrateStates = static_cast<std::vector<NumaVMInfoMap> *>(ctx);
    if (migrateStates == nullptr || buff.data == nullptr) {
        UBSE_LOG_WARN << "[GetMigrateStates] ctx or buff is nullptr.";
        return;
    }
    MigrateStateMapMessage dataMessage{};
    auto ret = dataMessage.SetInputRawData(buff.data, buff.len);
    if (ret != VM_OK) {
        UBSE_LOG_WARN << "[GetMigrateStates] message set input failed.";
        return;
    }
    ret = dataMessage.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_WARN << "[GetMigrateStates] message deserialize failed.";
        return;
    }
    migrateStates->emplace_back(dataMessage.GetData());
}

std::string MigrateStateStorage::ToString(const NumaVMInfoMap &numaVmInfoMap)
{
    std::ostringstream oss;
    for (auto &[numaLoc, vmBasicInfoMap] : numaVmInfoMap) {
        for (auto &[uuid, vmBasicInfo] : vmBasicInfoMap) {
            oss << "{";
            oss << R"("uuid":")" << vmBasicInfo.uuid << R"(",)";
            oss << R"("pid":)" << vmBasicInfo.pid << R"(,)";
            for (auto &[numaId, vmDomainNumaInfo] : vmBasicInfo.numaMemInfo) {
                oss << "{";
                oss << R"("socketId":)" << vmDomainNumaInfo.socketId << R"(,)";
                oss << R"("numaId":)" << vmDomainNumaInfo.numaId << R"(,)";
                oss << "},";
            }
            oss << R"("time":)" << vmBasicInfo.vmMigrateInTime << R"(,)";
            if (vmBasicInfo.vmMigrateStatus == MIGRATEABLE) {
                oss << R"("status":"MIGRATEABLE")";
            } else if (vmBasicInfo.vmMigrateStatus == MIGRATEUNABLE) {
                oss << R"("status":"MIGRATEUNABLE")";
            } else if (vmBasicInfo.vmMigrateStatus == MIGRATING) {
                oss << R"("status":"MIGRATING")";
            }
            oss << "},";
        }
    }
    auto json = oss.str();
    if (!json.empty()) {
        json.pop_back();
    }
    return "[" + json + "]";
}

} // namespace vm
