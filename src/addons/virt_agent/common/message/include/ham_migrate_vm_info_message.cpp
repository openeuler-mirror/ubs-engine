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

#include "ham_migrate_vm_info_message.h"
#include "vm_def.h"
#include "vm_serial_util.h"

namespace vm {
VmResult HamMigrateVmInfoMessage::Serialize()
{
    VmSerialization out;
    auto size = hamMigrateVmInfos_.size();
    out << size;
    for (auto &hamMigrateVmInfo : hamMigrateVmInfos_) {
        int64_t timeoutMs =
            std::chrono::duration_cast<milliseconds>(hamMigrateVmInfo.timeout.time_since_epoch()).count();
        if (timeoutMs < 0) {
            return VM_ERROR_INVAL;
        }
        out << hamMigrateVmInfo.nodeId;
        out << hamMigrateVmInfo.socketId;
        out << hamMigrateVmInfo.numaId;
        out << hamMigrateVmInfo.pid;
        out << hamMigrateVmInfo.dstNodeId;
        out << hamMigrateVmInfo.uuid;
        out << hamMigrateVmInfo.borrowName;
        out << enum_v(hamMigrateVmInfo.vmState);
        out << enum_v(hamMigrateVmInfo.vmOpState);
        out << enum_v(hamMigrateVmInfo.opState);
        out << enum_v(hamMigrateVmInfo.dstNodeState);
        out << timeoutMs;
    }
    if (!out.Check()) {
        return VM_ERROR;
    };
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = true;
    return VM_OK;
}

VmResult HamMigrateVmInfoMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    size_t vecSize;
    in >> vecSize;
    if (!in.Check()) {
        return VM_ERROR;
    }
    if (vecSize > MAX_VM_NUM) {
        return VM_ERROR_DESERIALIZE_ERROR;
    }
    for (size_t i = 0; i < vecSize; ++i) {
        HamMigrateVmInfo hamMigrateVmInfo;
        int64_t timeout;
        in >> hamMigrateVmInfo.nodeId;
        in >> hamMigrateVmInfo.socketId;
        in >> hamMigrateVmInfo.numaId;
        in >> hamMigrateVmInfo.pid;
        in >> hamMigrateVmInfo.dstNodeId;
        in >> hamMigrateVmInfo.uuid;
        in >> hamMigrateVmInfo.borrowName;
        in >> enum_v(hamMigrateVmInfo.vmState);
        in >> enum_v(hamMigrateVmInfo.vmOpState);
        in >> enum_v(hamMigrateVmInfo.opState);
        in >> enum_v(hamMigrateVmInfo.dstNodeState);
        in >> timeout;
        hamMigrateVmInfo.timeout = system_clock::from_time_t(0) + milliseconds(timeout);
        hamMigrateVmInfos_.emplace_back(hamMigrateVmInfo);
    }
    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}
} // namespace vm
