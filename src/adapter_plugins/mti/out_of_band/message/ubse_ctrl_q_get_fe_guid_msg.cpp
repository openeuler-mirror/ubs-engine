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
#include "ubse_ctrl_q_get_fe_guid_msg.h"
#include "securec.h"
#include "ubse_ctrl_q_message.h"
#include "ubse_ctrl_q_msg_helper.h"
#include "ubse_error.h"
#include "ubse_logger.h"
namespace ubse::mti::ctrl_q {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");
const uint8_t DEFAULT_RESP_BBNUM = 1;
static const uint8_t GRT_FE_GUID_OP_CODE = 0x2;

struct CtrlQGetFeGuidReqMsg {
    FixedHead head;
    FeLoc fe;
} __attribute__((packed));

struct RespReader {
    FixedHead head;
    Guid guid;
} __attribute__((packed));

UbseCtrlQGetIdevPfeGuidReqMsg::UbseCtrlQGetIdevPfeGuidReqMsg(const UbseMtiIdevPfe &pfe)
    : pfe_(pfe),
      ICtrlQReqMsg(GRT_FE_GUID_OP_CODE)
{
}

UbseResult UbseCtrlQGetIdevPfeGuidReqMsg::EncodeReqMsg()
{
    auto &ref = *reinterpret_cast<CtrlQGetFeGuidReqMsg *>(&reqMsg_.blocks.front());
    FeLoc feloc;
    feloc.slotId = pfe_.ubController.slotId;
    feloc.chipId = pfe_.ubController.chipId;
    feloc.dieId = pfe_.ubController.dieId;
    feloc.pfeId = pfe_.pfeId;
    feloc.vfeId = 0xff;
    ref.fe = feloc;
    return UBSE_OK;
}

UbseCtrlQGetIdevVfeGuidReqMsg::UbseCtrlQGetIdevVfeGuidReqMsg(const UbseMtiIdevVfe &vfe)
    : vfe_(vfe),
      ICtrlQReqMsg(GRT_FE_GUID_OP_CODE)
{
}

UbseResult UbseCtrlQGetIdevVfeGuidReqMsg::EncodeReqMsg()
{
    auto &ref = *reinterpret_cast<CtrlQGetFeGuidReqMsg *>(&reqMsg_.blocks.front());
    FeLoc feloc;
    feloc.slotId = vfe_.ubController.slotId;
    feloc.chipId = vfe_.ubController.chipId;
    feloc.dieId = vfe_.ubController.dieId;
    feloc.pfeId = vfe_.pfeId;
    feloc.vfeId = vfe_.vfeId;
    ref.fe = feloc;
    return UBSE_OK;
}

UbseCtrlQGet1825PfGuidReqMsg::UbseCtrlQGet1825PfGuidReqMsg(const UbseMti1825Pf &pf)
    : pf_(pf),
      ICtrlQReqMsg(GRT_FE_GUID_OP_CODE)
{
}

UbseResult UbseCtrlQGet1825PfGuidReqMsg::EncodeReqMsg()
{
    auto &ref = *reinterpret_cast<CtrlQGetFeGuidReqMsg *>(&reqMsg_.blocks.front());
    FeLoc feloc;
    feloc.slotId = pf_.slotId;
    feloc.chipId = pf_.chipId;
    feloc.dieId = pf_.dieId;
    feloc.pfeId = pf_.pfId;
    feloc.vfeId = 0xff;
    ref.fe = feloc;
    return UBSE_OK;
}

UbseCtrlQGet1825VfGuidReqMsg::UbseCtrlQGet1825VfGuidReqMsg(const UbseMti1825Vf &vf)
    : vf_(vf),
      ICtrlQReqMsg(GRT_FE_GUID_OP_CODE)
{
}

UbseResult UbseCtrlQGet1825VfGuidReqMsg::EncodeReqMsg()
{
    auto &ref = *reinterpret_cast<CtrlQGetFeGuidReqMsg *>(&reqMsg_.blocks.front());
    FeLoc feloc;
    feloc.slotId = vf_.slotId;
    feloc.chipId = vf_.chipId;
    feloc.dieId = vf_.dieId;
    feloc.pfeId = vf_.pfId;
    feloc.vfeId = vf_.vfId;
    ref.fe = feloc;
    return UBSE_OK;
}

UbseResult UbseCtrlQGetIdevPfeGuidRespMsg::DecodeRespMsg(const CtrlQRespMessage &msg)
{
    // bbNum 需要为1
    if (!CheckRespValidation(msg, 1, GRT_FE_GUID_OP_CODE)) {
        return UBSE_ERROR;
    }
    auto &reader = *reinterpret_cast<RespReader *>(msg.blocks);
    auto ret = memcpy_s(guid_.data(), guid_.size(), &reader.guid, guid_.size());
    if (ret != EOK) {
        UBSE_LOG_ERROR << "Memcpy guid failed, ret: " << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

const UbseMtiGuid &UbseCtrlQGetIdevPfeGuidRespMsg::GetGuid() const
{
    return guid_;
}

} // namespace ubse::mti::ctrl_q