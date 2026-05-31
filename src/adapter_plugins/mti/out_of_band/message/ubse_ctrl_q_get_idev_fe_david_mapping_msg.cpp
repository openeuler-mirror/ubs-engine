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
#include "ubse_ctrl_q_get_idev_fe_david_mapping_msg.h"
#include "ubse_ctrl_q_message.h"
#include "ubse_ctrl_q_msg_helper.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "securec.h"
namespace ubse::mti::ctrl_q {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");
static const uint8_t GET_IDEV_FE_DAVID_MAPPING_OP_CODE = 0x3;

struct RespReader {
    FixedHead head;
} __attribute__((packed));

UbseCtrlQGetIdevFeDavidMappingReqMsg::UbseCtrlQGetIdevFeDavidMappingReqMsg()
    : ICtrlQReqMsg(GET_IDEV_FE_DAVID_MAPPING_OP_CODE)
{
}

UbseResult UbseCtrlQGetIdevFeDavidMappingReqMsg::EncodeReqMsg()
{
    return UBSE_OK;
}

const UbseMtiIdevFeDavidMapping& UbseCtrlQGetIdevFeDavidMappingRespMsg::GetMapping() const
{
    return mapping_;
}

UbseResult UbseCtrlQGetIdevFeDavidMappingRespMsg::DecodeRespMsg(const CtrlQRespMessage& msg)
{
    // bbNum 为0时，不检查bbNum
    if (!CheckRespValidation(msg, 0, GET_IDEV_FE_DAVID_MAPPING_OP_CODE)) {
        return UBSE_ERROR;
    }
    auto pos = reinterpret_cast<uint8_t*>(msg.blocks) + sizeof(RespReader);
    auto end = reinterpret_cast<uint8_t*>(msg.blocks) + sizeof(BasicBlock) * msg.blockNums;
    UbseCtrlQMsgReadHelper readHelper(pos, end);
    try {
        auto pfeNum = readHelper.Read<uint8_t>();
        while (pfeNum > 0) {
            pfeNum--;
            auto chipId = readHelper.Read<uint8_t>();
            auto dieId = readHelper.Read<uint8_t>();
            auto pfeId = readHelper.Read<uint8_t>();
            auto davidSlotId = readHelper.Read<uint8_t>();
            auto davidChipId = readHelper.Read<uint8_t>();
            UbseMtiUbController ubController(chipId, dieId);
            UbseMtiIdevPfe pfe(ubController, pfeId);
            UbseMtiDavid david(davidSlotId, davidChipId);
            mapping_.emplace(david, pfe);
        }
        return UBSE_OK;
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Read get idev fe opt resp failed";
        return UBSE_ERROR;
    }
}
} // namespace ubse::mti::ctrl_q