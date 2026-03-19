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
#include "ubse_ctrl_q_get_idev_fe_david_mapping_proxy.h"
#include "../ubse_ctrl_q_message.h"
#include "../ubse_ctrl_q_msg_helper.h"
#include "securec.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
namespace ubse::mti::ctrl_q {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_MTI_MID)

struct RespReader {
    FixedHead head;
} __attribute__((packed));

UbseResult UbseCtrlQGetIdevFeDavidMappingReqMsg::GetReqMsg(CtrlQReqMessage &msg)
{
    SetOpCode(UbseCtrlQGetIdevFeDavidMappingProxy::OP_CODE, msg);
    SetServiceType(DEFAULT_SERVICE_TYPE, msg);
    return UBSE_OK;
}

bool UbseCtrlQGetIdevFeDavidMappingProxy::CheckReqValidation(const CtrlQReqMessage &msg)
{
    return !msg.blocks.empty() && msg.blocks.front().head.opCode == UbseCtrlQGetIdevFeDavidMappingProxy::OP_CODE;
}

UbseResult UbseCtrlQGetIdevFeDavidMappingProxy::ConvertRespMsgToUserData(const ICtrlQReqMsg &reqMsg,
                                                                         const CtrlQRespMessage &msg)
{
    // bbNum 为0时，不检查bbNum
    if (!CheckRespValidation(msg, 0, UbseCtrlQGetIdevFeDavidMappingProxy::OP_CODE)) {
        return UBSE_ERROR;
    }
    auto pos = reinterpret_cast<uint8_t *>(msg.blocks) + sizeof(RespReader);
    auto end = reinterpret_cast<uint8_t *>(msg.blocks) + sizeof(BasicBlock) * msg.blockNums;
    UbseCtrlQMsgReadHelper readHelper(pos, end);
    try {
        auto pfeNum = readHelper.Read<uint8_t>();
        while (pfeNum--) {
            auto chipId = readHelper.Read<uint8_t>();
            auto dieId = readHelper.Read<uint8_t>();
            auto pfeId = readHelper.Read<uint8_t>();
            auto davidSlotId = readHelper.Read<uint8_t>();
            auto davidChipId = readHelper.Read<uint8_t>();
            // 最后一个PFE，davidSlotId和davidChipId必须为0xff
            if (pfeNum == 0 && (davidSlotId != 0xff || davidChipId != 0xff)) {
                UBSE_LOG_ERROR << "Last pfe, davidSlotId and davidChipId must be 0xff";
                return UBSE_ERROR;
            }
            UbseMtiUbController ubController(chipId, dieId);
            UbseMtiIdevPfe pfe(ubController, pfeId);
            UbseMtiDavid david(davidSlotId, davidChipId);
            resp_.emplace(david, pfe);
        }
        return UBSE_OK;
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Read get idev fe opt resp failed";
        return UBSE_ERROR;
    }
}
} // namespace ubse::mti::ctrl_q