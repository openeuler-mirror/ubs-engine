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

#ifndef UBSE_ICTRL_Q_REQ_MSG_H
#define UBSE_ICTRL_Q_REQ_MSG_H
#include "ubse_common_def.h"
#include "ubse_ctrl_q_message.h"

namespace ubse::mti::ctrl_q {
using namespace ubse::common::def;
class ICtrlQReqMsg {
public:
    virtual ~ICtrlQReqMsg() = default;

    virtual UbseResult GetReqMsg(CtrlQReqMessage &msg) = 0;

protected:
    void SetVersion(uint8_t version, CtrlQReqMessage &msg);

    void SetOpCode(uint8_t opcode, CtrlQReqMessage &msg);

    void SetRet(uint8_t ret, CtrlQReqMessage &msg);

    void SetSeq(uint16_t seq, CtrlQReqMessage &msg);

    void SetResv(uint8_t resv, CtrlQReqMessage &msg);

    void SetServiceType(uint8_t serviceType, CtrlQReqMessage &msg);

    void SetBBNum(uint8_t bbNum, CtrlQReqMessage &msg);

    void ResizeReqMsg(uint8_t size, CtrlQReqMessage &msg);
};
} // namespace ubse::mti::ctrl_q
#endif // UBSE_ICTRL_Q_REQ_MSG_H