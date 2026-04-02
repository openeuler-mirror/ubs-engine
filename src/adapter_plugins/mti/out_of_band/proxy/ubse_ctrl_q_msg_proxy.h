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

#ifndef UBSE_CTRL_Q_MSG_PROXY_H
#define UBSE_CTRL_Q_MSG_PROXY_H
#include "../message/ubse_ctrl_q_message.h"
#include "../message/ubse_ictrl_q_req_msg.h"
#include "../message/ubse_ictrl_q_resp_msg.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_pointer_process.h"

namespace ubse::mti::ctrl_q {
using namespace ubse::common::def;
class CtrlQMsgProxy {
public:
    virtual ~CtrlQMsgProxy() = default;

    CtrlQMsgProxy() = default;

    static CtrlQMsgProxy &GetInstance();

    UbseResult SendRequest(ICtrlQReqMsg &reqMsg, ICtrlQRespMsg &respMsg);
};
} // namespace ubse::mti::ctrl_q
#endif // UBSE_CTRL_Q_MSG_PROXY_H
