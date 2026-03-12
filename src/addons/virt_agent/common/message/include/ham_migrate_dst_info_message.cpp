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

#include "ham_migrate_dst_info_message.h"

#include <ubse_logger.h>
#include "vm_serial_util.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("vm_plugin");
using namespace ubse::log;
VmResult HamMigrateDstInfoMessage::Serialize()
{
    VmSerialization out;
    out << hamMigrateDstInfo.dstPid;
    out << hamMigrateDstInfo.dstNodeId;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed, hamMigrateDstInfo.dstPid=" << hamMigrateDstInfo.dstPid
            << ", hamMigrateDstInfo.dstNodeId=" << hamMigrateDstInfo.dstNodeId;
        return VM_ERROR;
    };
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = true;
    return VM_OK;
}

VmResult HamMigrateDstInfoMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR  << "Deserialize failed, mInputRawData is null.";
        return VM_ERROR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    in >> hamMigrateDstInfo.dstPid;
    in >> hamMigrateDstInfo.dstNodeId;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Deserialize failed, mInputRawDataSize=" << mInputRawDataSize
            << ", hamMigrateDstInfo.dstPid=" << hamMigrateDstInfo.dstPid << ", hamMigrateDstInfo.dstNodeId="
            << hamMigrateDstInfo.dstNodeId;
        return VM_ERROR;
    }
    return VM_OK;
}
} // namespace vm
