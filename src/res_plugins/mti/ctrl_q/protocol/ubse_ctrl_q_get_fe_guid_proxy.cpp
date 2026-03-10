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
#include "ubse_ctrl_q_get_fe_guid_proxy.h"
#include "../ubse_ctrl_q_message.h"
#include "../ubse_ctrl_q_msg_helper.h"
#include "securec.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
namespace ubse::mti::ctrl_q {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_MTI_MID)
const uint8_t UbseCtrlQGetFeGuidProxy::OP_CODE = 0x2;
const uint8_t DEFAULT_RESP_BBNUM = 1;

struct CtrlQGetFeGuidReqMsg {
    FixedHead head;
    FeLoc fe;
} __attribute__((packed));

struct RespReader {
    FixedHead head;
    Guid guid;
} __attribute__((packed));

UbseCtrlQGetIdevPfeGuidReqMsg::UbseCtrlQGetIdevPfeGuidReqMsg(const UbseMtiIdevPfe &pfe) : pfe_(pfe) {}

UbseResult UbseCtrlQGetIdevPfeGuidReqMsg::GetReqMsg(CtrlQReqMessage &msg)
{
    SetOpCode(UbseCtrlQGetFeGuidProxy::OP_CODE, msg);
    SetServiceType(DEFAULT_SERVICE_TYPE, msg);
    auto &ref = *reinterpret_cast<CtrlQGetFeGuidReqMsg *>(&msg.blocks.front());
    FeLoc feloc;
    feloc.slotId = pfe_.ubController.slotId;
    feloc.chipId = pfe_.ubController.chipId;
    feloc.dieId = pfe_.ubController.dieId;
    feloc.pfeId = pfe_.pfeId;
    feloc.vfeId = 0xff;
    ref.fe = feloc;
    return UBSE_OK;
}

UbseCtrlQGetIdevVfeGuidReqMsg::UbseCtrlQGetIdevVfeGuidReqMsg(const UbseMtiIdevVfe &vfe) : vfe_(vfe) {}

UbseResult UbseCtrlQGetIdevVfeGuidReqMsg::GetReqMsg(CtrlQReqMessage &msg)
{
    SetOpCode(UbseCtrlQGetFeGuidProxy::OP_CODE, msg);
    SetServiceType(DEFAULT_SERVICE_TYPE, msg);
    auto &ref = *reinterpret_cast<CtrlQGetFeGuidReqMsg *>(&msg.blocks.front());
    FeLoc feloc;
    feloc.slotId = vfe_.ubController.slotId;
    feloc.chipId = vfe_.ubController.chipId;
    feloc.dieId = vfe_.ubController.dieId;
    feloc.pfeId = vfe_.pfeId;
    feloc.vfeId = vfe_.vfeId;
    ref.fe = feloc;
    return UBSE_OK;
}

bool UbseCtrlQGetFeGuidProxy::CheckReqValidation(const CtrlQReqMessage &msg)
{
    return !msg.blocks.empty() && msg.blocks.front().head.opCode == UbseCtrlQGetFeGuidProxy::OP_CODE;
}

UbseResult UbseCtrlQGetFeGuidProxy::ConvertRespMsgToUserData(const ICtrlQReqMsg &reqMsg, const CtrlQRespMessage &msg)
{
    // bbNum 需要为1
    if (!CheckRespValidation(msg, 1, UbseCtrlQGetFeGuidProxy::OP_CODE)) {
        return UBSE_ERROR;
    }
    auto &reader = *reinterpret_cast<RespReader *>(msg.blocks);
    auto ret = memcpy_s(resp_.data(), resp_.size(), &reader.guid, resp_.size());
    if (ret != EOK) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::mti::ctrl_q