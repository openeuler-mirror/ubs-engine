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

#include "ubse_ctrl_q_get_d2h_memory_msg.h"
#include "securec.h"
#include "ubse_ctrl_q_message.h"
#include "ubse_ctrl_q_msg_helper.h"
#include "ubse_error.h"
#include "ubse_logger.h"

namespace ubse::mti::ctrl_q {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");
static const uint8_t GET_UBA_TID_SIZE_OP_CODE = 0xD;

struct GetUbaTidSizeReqMsg {
    FixedHead fixedHead;
    struct {
        uint32_t eid : 20;
        uint32_t cmdData : 12;
    } __attribute__((packed)) eid;
} __attribute__((packed));

struct GetUbaTidSizeRespMsg {
    FixedHead fixedHead;
    struct {
        uint32_t eid : 20;
        uint32_t tid : 12;
    } __attribute__((packed)) eidTid;
    uint32_t ubaHigh;
    uint32_t ubaLow;
    uint32_t size;
} __attribute__((packed));

UbseCtrlQGetD2hMemoryReqMsg::UbseCtrlQGetD2hMemoryReqMsg(const UbseMtiBusInst &busInstance)
    : ICtrlQReqMsg(GET_UBA_TID_SIZE_OP_CODE),
      busInstance_(busInstance)
{
}

UbseResult UbseCtrlQGetD2hMemoryReqMsg::EncodeReqMsg()
{
    auto &ref = *reinterpret_cast<GetUbaTidSizeReqMsg *>(&reqMsg_.blocks.front());
    // 从EID数组的前4个字节提取32位整数
    uint32_t eidValue = 0;
    auto ret = memcpy_s(&eidValue, sizeof(eidValue), busInstance_.eid.data(), sizeof(uint32_t));
    if (ret != EOK) {
        UBSE_LOG_ERROR << "Copy eid failed, ret: " << ret;
        return UBSE_ERROR;
    }
    // 提取低20位作为EID值
    ref.eid.eid = eidValue & 0x000FFFFF; // 20位掩码
    ref.eid.cmdData = 0;                 // 保留位设为0
    return UBSE_OK;
}

UbseResult UbseCtrlQGetD2hMemoryRespMsg::DecodeRespMsg(const CtrlQRespMessage &msg)
{
    // Check resp validation, bbNum is 1.
    if (!CheckRespValidation(msg, 1, GET_UBA_TID_SIZE_OP_CODE)) {
        return UBSE_ERROR;
    }
    auto &ref = *reinterpret_cast<GetUbaTidSizeRespMsg *>(msg.blocks);
    tid_ = ref.eidTid.tid;
    // 合并高32位和低32位形成64位UBA值
    uba_ = (static_cast<uint64_t>(ref.ubaHigh) << 32) | ref.ubaLow;
    size_ = ref.size;
    return UBSE_OK;
}

uint64_t UbseCtrlQGetD2hMemoryRespMsg::GetUba() const
{
    return uba_;
}

uint64_t UbseCtrlQGetD2hMemoryRespMsg::GetTid() const
{
    return tid_;
}

uint64_t UbseCtrlQGetD2hMemoryRespMsg::GetSize() const
{
    return size_;
}

} // namespace ubse::mti::ctrl_q