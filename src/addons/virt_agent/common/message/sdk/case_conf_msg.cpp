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

#include "case_conf_msg.h"

#include "msg_utils.h"
#include "vm_serial_util.h"

namespace vm {

VmResult CaseConfGetMsg::Serialize()
{
    VmSerialization out;

    out << caseConf_.curCase;
    out << caseConf_.overCommitmentRatio;
    out << caseConf_.migrateWaterLine;
    out << caseConf_.index;
    out << caseConf_.host_id;

    if (!out.Check()) {
        return VM_ERROR;
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = true;
    return VM_OK;
}

VmResult CaseConfGetMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    if (!in.Check()) {
        return VM_ERROR;
    }
    in >> caseConf_.curCase;
    in >> caseConf_.overCommitmentRatio;
    in >> caseConf_.migrateWaterLine;
    in >> caseConf_.index;
    in >> caseConf_.host_id;

    if (!in.Check()) {
        return VM_ERROR;
    }

    return VM_OK;
}

case_conf_info_t CaseConfGetMsg::GetCaseConf()
{
    case_conf_info_t caseConfInfo{};
    auto ret = StringToC(caseConfInfo.cur_case, caseConf_.curCase, VIRT_MAX_CUR_CASE_LENGTH);
    ret |= StringToC(caseConfInfo.over_commitment_ratio, caseConf_.overCommitmentRatio,
                     VIRT_MAX_OVERCOMMITMENT_RATIO_LENGTH);
    ret |= StringToC(caseConfInfo.migrate_waterLine, caseConf_.migrateWaterLine, VIRT_MAX_MIGRATE_WATER_LINE_LENGTH);
    caseConfInfo.index = caseConf_.index;
    ret |= StringToC(caseConfInfo.host_id, caseConf_.host_id, VIRT_MAX_NODE_ID_LENGTH);
    if (ret != VM_OK) {
        return {};
    }
    return caseConfInfo;
}

VmResult CaseConfSetMsg::Serialize()
{
    VmSerialization out;

    out << caseConf_.caseType;
    out << caseConf_.overCommitmentRatio;
    out << caseConf_.msg;
    out << caseConf_.ret;

    if (!out.Check()) {
        return VM_ERROR;
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = true;
    return VM_OK;
}

VmResult CaseConfSetMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    if (!in.Check()) {
        return VM_ERROR;
    }
    in >> caseConf_.caseType;
    in >> caseConf_.overCommitmentRatio;
    in >> caseConf_.msg;
    in >> caseConf_.ret;

    if (!in.Check()) {
        return VM_ERROR;
    }

    return VM_OK;
}

case_conf_set_info_t CaseConfSetMsg::GetCaseConf()
{
    case_conf_set_info_t caseConfInfo{};
    caseConfInfo.ret = caseConf_.ret;
    return caseConfInfo;
}

} // namespace vm
