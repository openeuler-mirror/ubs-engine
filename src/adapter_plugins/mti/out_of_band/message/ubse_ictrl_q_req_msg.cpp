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
static void SetVersionToBuf(uint8_t version, CtrlQReqMessage &reqMsg)
{
    reqMsg.blocks.front().head.version = version;
}

static void SetOpCodeToBuf(uint8_t opcode, CtrlQReqMessage &reqMsg)
{
    reqMsg.blocks.front().head.opCode = opcode;
}

static void SetRetToBuf(uint8_t ret, CtrlQReqMessage &reqMsg)
{
    reqMsg.blocks.front().head.ret = ret;
}

static void SetSeqToBuf(uint16_t seq, CtrlQReqMessage &reqMsg)
{
    reqMsg.blocks.front().head.seq = seq;
}

static void SetResvToBuf(uint8_t resv, CtrlQReqMessage &reqMsg)
{
    reqMsg.blocks.front().head.resv = resv;
}

static void SetServiceTypeToBuf(uint8_t serviceType, CtrlQReqMessage &reqMsg)
{
    reqMsg.blocks.front().head.serviceType = serviceType;
}

static void SetBBNumToBuf(uint8_t bbNum, CtrlQReqMessage &reqMsg)
{
    reqMsg.blocks.front().head.bbNum = bbNum;
}

ICtrlQReqMsg::ICtrlQReqMsg(uint8_t opCode, uint8_t bbNum)
{
    reqMsg_ = CtrlQReqMessage(bbNum);
    SetBBNumToBuf(bbNum, reqMsg_);
    SetOpCodeToBuf(opCode, reqMsg_);
    SetServiceTypeToBuf(DEFAULT_SERVICE_TYPE, reqMsg_);
}

const CtrlQReqMessage &ICtrlQReqMsg::GetReqMsg() const
{
    return reqMsg_;
}

void ICtrlQReqMsg::SetVersion(uint8_t version)
{
    SetVersionToBuf(version, reqMsg_);
}

void ICtrlQReqMsg::SetOpCode(uint8_t opCode)
{
    SetOpCodeToBuf(opCode, reqMsg_);
}

void ICtrlQReqMsg::SetRet(uint8_t ret)
{
    SetRetToBuf(ret, reqMsg_);
}

void ICtrlQReqMsg::SetSeq(uint16_t seq)
{
    SetSeqToBuf(seq, reqMsg_);
}

void ICtrlQReqMsg::SetResv(uint8_t resv)
{
    SetResvToBuf(resv, reqMsg_);
}

void ICtrlQReqMsg::SetServiceType(uint8_t serviceType)
{
    SetServiceTypeToBuf(serviceType, reqMsg_);
}

void ICtrlQReqMsg::SetBBNum(uint8_t bbNum)
{
    SetBBNumToBuf(bbNum, reqMsg_);
}

} // namespace ubse::mti::ctrl_q