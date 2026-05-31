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

#ifndef UBSE_ICTRL_Q_RESP_MSG_H
#define UBSE_ICTRL_Q_RESP_MSG_H
#include "ubse_common_def.h"
#include "ubse_ctrl_q_message.h"
#include "ubse_ictrl_q_req_msg.h"

namespace ubse::mti::ctrl_q {
using namespace ubse::common::def;

class ICtrlQRespMsg {
public:
    virtual ~ICtrlQRespMsg() = default;

    virtual UbseResult DecodeRespMsg(const CtrlQRespMessage& msg) = 0;
};
} // namespace ubse::mti::ctrl_q
#endif // UBSE_ICTRL_Q_RESP_MSG_H
