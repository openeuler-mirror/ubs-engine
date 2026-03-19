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
#include "ubse_ctrl_q_businstance_opt_proxy.h"
#include "../ubse_ctrl_q_message.h"
#include "../ubse_ctrl_q_msg_helper.h"
#include "securec.h"
#include "ubse_error.h"
#include "ubse_logger.h"
namespace ubse::mti::ctrl_q {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

struct UbseCtrlQCreateBusInstanceReq {
    FixedHead head;
    uint16_t upi;
    uint16_t vendor;
} __attribute__((packed));

struct UbseCtrlQCreateBusInstanceRespReader {
    FixedHead head;
    struct {
        uint32_t eid : 20;
        uint32_t resv : 12;
    } __attribute__((packed)) eid;
    Guid guid;
} __attribute__((packed));

UbseCtrlQCreateBusInstanceReqMsg::UbseCtrlQCreateBusInstanceReqMsg(uint16_t upi, uint16_t vendor)
    : upi_(upi),
      vendor_(vendor)
{
}

UbseResult UbseCtrlQCreateBusInstanceReqMsg::GetReqMsg(CtrlQReqMessage &msg)
{
    SetOpCode(UbseCtrlQCreateBusInstanceProxy::OP_CODE, msg);
    SetServiceType(DEFAULT_SERVICE_TYPE, msg);
    auto &ref = *reinterpret_cast<UbseCtrlQCreateBusInstanceReq *>(&msg.blocks.front());
    ref.upi = upi_;
    ref.vendor = vendor_;
    return UBSE_OK;
}

uint16_t UbseCtrlQCreateBusInstanceReqMsg::GetUpi()
{
    return upi_;
}

uint16_t UbseCtrlQCreateBusInstanceReqMsg::GetVendor()
{
    return vendor_;
}

bool UbseCtrlQCreateBusInstanceProxy::CheckReqValidation(const CtrlQReqMessage &msg)
{
    return !msg.blocks.empty() && msg.blocks.front().head.opCode == UbseCtrlQCreateBusInstanceProxy::OP_CODE;
}

UbseResult UbseCtrlQCreateBusInstanceProxy::ConvertRespMsgToUserData(const ICtrlQReqMsg &reqMsg,
                                                                     const CtrlQRespMessage &msg)
{
    // Check resp validation, bbNum is 1.
    if (!CheckRespValidation(msg, 1, UbseCtrlQCreateBusInstanceProxy::OP_CODE)) {
        return UBSE_ERROR;
    }
    try {
        auto req = dynamic_cast<const UbseCtrlQCreateBusInstanceReqMsg &>(reqMsg);
        resp_.upi = req.GetUpi();
        resp_.vendor = req.GetVendor();
    } catch (const std::bad_cast &e) {
        UBSE_LOG_ERROR << "Req msg not UbseCtrlQCreateBusInstanceReqMsg";
        return UBSE_ERROR;
    }
    auto &body = *reinterpret_cast<UbseCtrlQCreateBusInstanceRespReader *>(msg.blocks);
    uint32_t tmpEid = body.eid.eid;
    auto ret = memcpy_s(resp_.eid.data(), resp_.eid.size(), &tmpEid, sizeof(tmpEid));
    if (ret != EOK) {
        UBSE_LOG_ERROR << "Copy eid failed, ret: " << ret;
        return UBSE_ERROR;
    }
    ret = memcpy_s(resp_.guid.data(), resp_.guid.size(), &body.guid.content, sizeof(body.guid.content));
    if (ret != EOK) {
        UBSE_LOG_ERROR << "Copy guid failed, ret: " << ret;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

struct UbseCtrlQDestroyBusInstanceReq {
    FixedHead head;
    struct {
        uint32_t eid : 20;
        uint32_t resv : 12;
    } __attribute__((packed)) eid;
} __attribute__((packed));

UbseCtrlQDestroyBusInstanceReqMsg::UbseCtrlQDestroyBusInstanceReqMsg(const UbseMtiBusInst &busInstance)
    : busInstance_(busInstance)
{
}

UbseResult UbseCtrlQDestroyBusInstanceReqMsg::GetReqMsg(CtrlQReqMessage &msg)
{
    SetOpCode(UbseCtrlQDestroyBusInstanceProxy::OP_CODE, msg);
    SetServiceType(DEFAULT_SERVICE_TYPE, msg);
    auto &ref = *reinterpret_cast<UbseCtrlQDestroyBusInstanceReq *>(&msg.blocks.front());
    // 从EID数组的前4个字节提取32位整数
    uint32_t eidValue = 0;
    auto ret = memcpy_s(&eidValue, sizeof(eidValue), busInstance_.eid.data(), sizeof(uint32_t));
    if (ret != EOK) {
        UBSE_LOG_ERROR << "Copy eid failed, ret: " << ret;
        return UBSE_ERROR;
    }
    // 提取低20位作为EID值
    ref.eid.eid = eidValue & 0x000FFFFF; // 20位掩码
    ref.eid.resv = 0;                    // 保留位设为0
    return UBSE_OK;
}

bool UbseCtrlQDestroyBusInstanceProxy::CheckReqValidation(const CtrlQReqMessage &msg)
{
    return !msg.blocks.empty() && msg.blocks.front().head.opCode == UbseCtrlQDestroyBusInstanceProxy::OP_CODE;
}

UbseResult UbseCtrlQDestroyBusInstanceProxy::ConvertRespMsgToUserData(const ICtrlQReqMsg &reqMsg,
                                                                      const CtrlQRespMessage &msg)
{
    // Check resp validation, bbNum is 1.
    if (!CheckRespValidation(msg, 1, UbseCtrlQDestroyBusInstanceProxy::OP_CODE)) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::mti::ctrl_q