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

#include "ham_make_decision_msg.h"

#include "vm_serial_util.h"

namespace vm {
VmResult HamMakeDecisionMsg::Serialize()
{
    VmSerialization out;
    out << inputParams_.vmMemoryMB;
    out << inputParams_.uuid;
    out << inputParams_.destHostName;
    out << inputParams_.destNumaId;

    if (!out.Check()) {
        return VM_ERROR;
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = false;
    return VM_OK;
}

VmResult HamMakeDecisionMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);

    if (!in.Check()) {
        return VM_ERROR;
    }

    in >> inputParams_.vmMemoryMB;
    in >> inputParams_.uuid;
    in >> inputParams_.destHostName;
    in >> inputParams_.destNumaId;

    if (!in.Check()) {
        return VM_ERROR;
    }

    return VM_OK;
}
} // namespace vm