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

#ifndef UBSE_CTRL_Q_GET_1825_FE_MSG_H
#define UBSE_CTRL_Q_GET_1825_FE_MSG_H
#include "adapter_plugins/mti/ubse_mti_1825.h"
#include "ubse_ictrl_q_req_msg.h"
#include "ubse_ictrl_q_resp_msg.h"
namespace ubse::mti::ctrl_q {
using namespace mti::_1825;
class UbseCtrlQGet1825FeReqMsg : public ICtrlQReqMsg {
public:
    UbseCtrlQGet1825FeReqMsg();

    UbseResult EncodeReqMsg() override;
};

class UbseCtrlQGet1825PfeRespMsg : public ICtrlQRespMsg {
public:
    UbseCtrlQGet1825PfeRespMsg() = default;

    UbseResult DecodeRespMsg(const CtrlQRespMessage &msg) override;

    const std::vector<UbseMti1825Pf> &GetPfList() const;

private:
    std::vector<UbseMti1825Pf> pfList_;
};

} // namespace ubse::mti::ctrl_q
#endif // UBSE_CTRL_Q_GET_1825_FE_MSG_H
