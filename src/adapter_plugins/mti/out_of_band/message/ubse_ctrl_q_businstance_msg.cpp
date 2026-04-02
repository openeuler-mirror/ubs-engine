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
#include "ubse_ctrl_q_businstance_msg.h"
#include "securec.h"
#include "ubse_ctrl_q_message.h"
#include "ubse_ctrl_q_msg_helper.h"
#include "ubse_error.h"
#include "ubse_logger.h"
namespace ubse::mti::ctrl_q {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");
static const uint8_t CREATE_BUSINSTANCE_OP_CODE = 0x4;
static const uint8_t DESTROY_BUSINSTANCE_OP_CODE = 0x5;

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
      vendor_(vendor),
      ICtrlQReqMsg(CREATE_BUSINSTANCE_OP_CODE)
{
}

UbseResult UbseCtrlQCreateBusInstanceReqMsg::EncodeReqMsg()
{
    auto &ref = *reinterpret_cast<UbseCtrlQCreateBusInstanceReq *>(&reqMsg_.blocks.front());
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

UbseResult UbseCtrlQCreateBusInstanceRespMsg::DecodeRespMsg(const CtrlQRespMessage &msg)
{
    // Check resp validation, bbNum is 1.
    if (!CheckRespValidation(msg, 1, CREATE_BUSINSTANCE_OP_CODE)) {
        return UBSE_ERROR;
    }
    auto &body = *reinterpret_cast<UbseCtrlQCreateBusInstanceRespReader *>(msg.blocks);
    uint32_t tmpEid = body.eid.eid;
    auto ret = memcpy_s(busInstance_.eid.data(), busInstance_.eid.size(), &tmpEid, sizeof(tmpEid));
    if (ret != EOK) {
        UBSE_LOG_ERROR << "Copy eid failed, ret: " << ret;
        return UBSE_ERROR;
    }
    ret = memcpy_s(busInstance_.guid.data(), busInstance_.guid.size(), &body.guid.content, sizeof(body.guid.content));
    if (ret != EOK) {
        UBSE_LOG_ERROR << "Copy guid failed, ret: " << ret;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

const UbseMtiBusInst &UbseCtrlQCreateBusInstanceRespMsg::GetBusInstance() const
{
    return busInstance_;
}

struct UbseCtrlQDestroyBusInstanceReq {
    FixedHead head;
    struct {
        uint32_t eid : 20;
        uint32_t resv : 12;
    } __attribute__((packed)) eid;
} __attribute__((packed));

UbseCtrlQDestroyBusInstanceReqMsg::UbseCtrlQDestroyBusInstanceReqMsg(const UbseMtiBusInst &busInstance)
    : busInstance_(busInstance),
      ICtrlQReqMsg(DESTROY_BUSINSTANCE_OP_CODE)
{
}

UbseResult UbseCtrlQDestroyBusInstanceReqMsg::EncodeReqMsg()
{
    auto &ref = *reinterpret_cast<UbseCtrlQDestroyBusInstanceReq *>(&reqMsg_.blocks.front());
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

UbseResult UbseCtrlQDestroyBusInstanceRespMsg::DecodeRespMsg(const CtrlQRespMessage &msg)
{
    // Check resp validation, bbNum is 1.
    if (!CheckRespValidation(msg, 1, DESTROY_BUSINSTANCE_OP_CODE)) {
        return UBSE_ERROR;
    }
    ret_ = true;
    return UBSE_OK;
}

const bool &UbseCtrlQDestroyBusInstanceRespMsg::GetRet() const
{
    return ret_;
}

} // namespace ubse::mti::ctrl_q