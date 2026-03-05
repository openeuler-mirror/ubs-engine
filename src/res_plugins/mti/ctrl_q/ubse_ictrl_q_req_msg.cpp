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
#include "ubse_ictrl_q_req_msg.h"
namespace ubse::mti::ctrl_q {
void ICtrlQReqMsg::SetVersion(uint8_t version, CtrlQReqMessage &msg)
{
    msg.blocks.front().head.version = version;
}

void ICtrlQReqMsg::SetOpCode(uint8_t opcode, CtrlQReqMessage &msg)
{
    msg.blocks.front().head.opCode = opcode;
}

void SetRet(uint8_t ret, CtrlQReqMessage &msg)
{
    msg.blocks.front().head.ret = ret;
}

void SetSeq(uint16_t seq, CtrlQReqMessage &msg)
{
    msg.blocks.front().head.seq = seq;
}

void SetResv(uint8_t resv, CtrlQReqMessage &msg)
{
    msg.blocks.front().head.resv = resv;
}

void ICtrlQReqMsg::SetServiceType(uint8_t serviceType, CtrlQReqMessage &msg)
{
    msg.blocks.front().head.serviceType = serviceType;
}

void ICtrlQReqMsg::SetBBNum(uint8_t bbNum, CtrlQReqMessage &msg)
{
    msg.blocks.front().head.bbNum = bbNum;
}

void ICtrlQReqMsg::ResizeReqMsg(uint8_t size, CtrlQReqMessage &msg)
{
    msg.blocks.resize(size);
    SetBBNum(size, msg);
}
} // namespace ubse::mti::ctrl_q