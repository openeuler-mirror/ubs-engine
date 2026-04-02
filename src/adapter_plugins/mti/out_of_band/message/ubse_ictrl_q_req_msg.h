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

    ICtrlQReqMsg() = delete;

    explicit ICtrlQReqMsg(uint8_t opCode, uint8_t bbNum = 1);

    virtual UbseResult EncodeReqMsg() = 0;

    const CtrlQReqMessage &GetReqMsg() const;

protected:
    void SetVersion(uint8_t version);

    void SetOpCode(uint8_t opCode);

    void SetRet(uint8_t ret);

    void SetSeq(uint16_t seq);

    void SetResv(uint8_t resv);

    void SetServiceType(uint8_t serviceType);

    void SetBBNum(uint8_t bbNum);

    CtrlQReqMessage reqMsg_;
};
} // namespace ubse::mti::ctrl_q
#endif // UBSE_ICTRL_Q_REQ_MSG_H