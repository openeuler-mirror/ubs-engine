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

#ifndef UBSE_CTRL_Q_GET_FE_GUID_PROXY_H
#define UBSE_CTRL_Q_GET_FE_GUID_PROXY_H
#include "../ubse_ictrl_q_msg_operation_proxy.h"
#include "../ubse_ictrl_q_req_msg.h"
#include "adapter_plugins/mti/ubse_mti_urma.h"
namespace ubse::mti::ctrl_q {
using namespace mti::urma;
class UbseCtrlQGetIdevPfeGuidReqMsg : public ICtrlQReqMsg {
public:
    UbseCtrlQGetIdevPfeGuidReqMsg(const UbseMtiIdevPfe &pfe);

    UbseResult GetReqMsg(CtrlQReqMessage &msg) override;

private:
    UbseMtiIdevPfe pfe_;
};

class UbseCtrlQGetIdevVfeGuidReqMsg : public ICtrlQReqMsg {
public:
    UbseCtrlQGetIdevVfeGuidReqMsg(const UbseMtiIdevVfe &vfe);

    UbseResult GetReqMsg(CtrlQReqMessage &msg) override;

private:
    UbseMtiIdevVfe vfe_;
};

class UbseCtrlQGetFeGuidProxy : public ICtrlQMsgOperationProxy<UbseMtiGuid> {
public:
    static const uint8_t OP_CODE;
    UbseCtrlQGetFeGuidProxy() = default;
    ~UbseCtrlQGetFeGuidProxy() override = default;

private:
    bool CheckReqValidation(const CtrlQReqMessage &msg) override;

    UbseResult ConvertRespMsgToUserData(const ICtrlQReqMsg &reqMsg, const CtrlQRespMessage &msg) override;
};
} // namespace ubse::mti::ctrl_q
#endif // UBSE_CTRL_Q_GET_FE_GUID_PROXY_H