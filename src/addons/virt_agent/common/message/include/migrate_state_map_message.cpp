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

#include "migrate_state_map_message.h"

#include "vm_def.h"
#include "vm_serial_util.h"

namespace vm {
VmResult MigrateStateMapMessage::Serialize()
{
    size_t mapSize = 0;
    for (auto& [numaLoc, vmBasicInfoMap] : migrateStateMap) {
        for (auto& [uuid, vmBasicInfo] : vmBasicInfoMap) {
            mapSize++;
        }
    }
    VmSerialization out;
    out << mapSize;
    for (auto& [numaLoc, vmBasicInfoMap] : migrateStateMap) {
        for (auto& [uuid, vmBasicInfo] : vmBasicInfoMap) {
            out << vmBasicInfo.uuid;
            out << vmBasicInfo.pid;
            out << vmBasicInfo.nodeId;
            out << vmBasicInfo.hostName;
            out << enum_v(vmBasicInfo.vmMigrateStatus);
            out << vmBasicInfo.vmMigrateInTime;
            size_t numaMemInfoSize = vmBasicInfo.numaMemInfo.size();
            out << numaMemInfoSize;
            for (auto& [numaId, vmDomainNumaInfo] : vmBasicInfo.numaMemInfo) {
                out << vmDomainNumaInfo.socketId;
                out << vmDomainNumaInfo.numaId;
            }
        }
    }
    if (!out.Check()) {
        return VM_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = true;
    return VM_OK;
}

VmResult MigrateStateMapMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    size_t mapSize;
    in >> mapSize;
    if (!in.Check()) {
        return VM_ERROR;
    }
    if (mapSize > MAX_VM_NUM) {
        return VM_ERROR_DESERIALIZE_ERROR;
    }
    for (size_t i = 0; i < mapSize; ++i) {
        VMBasicInfo vmBasicInfo;
        in >> vmBasicInfo.uuid;
        in >> vmBasicInfo.pid;
        in >> vmBasicInfo.nodeId;
        in >> vmBasicInfo.hostName;
        in >> enum_v(vmBasicInfo.vmMigrateStatus);
        in >> vmBasicInfo.vmMigrateInTime;
        size_t numaMemInfoSize;
        in >> numaMemInfoSize;
        for (size_t j = 0; j < numaMemInfoSize; ++j) {
            mempooling::VmDomainNumaInfo vmDomainNumaInfo{};
            in >> vmDomainNumaInfo.socketId;
            in >> vmDomainNumaInfo.numaId;
            vmBasicInfo.numaMemInfo.emplace(vmDomainNumaInfo.numaId, vmDomainNumaInfo);
            VMNodeLocInfo numaLoc{};
            numaLoc.socketId = vmDomainNumaInfo.socketId;
            numaLoc.numaId = vmDomainNumaInfo.numaId;
            numaLoc.hostName = vmBasicInfo.hostName;
            numaLoc.hostId = vmBasicInfo.nodeId;
            migrateStateMap[numaLoc][vmBasicInfo.uuid] = vmBasicInfo;
        }
    }
    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}
} // namespace vm