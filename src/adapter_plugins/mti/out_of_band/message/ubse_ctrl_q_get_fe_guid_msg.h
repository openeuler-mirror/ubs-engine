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

#ifndef UBSE_CTRL_Q_GET_FE_GUID_MSG_H
#define UBSE_CTRL_Q_GET_FE_GUID_MSG_H
#include "ubse_ictrl_q_req_msg.h"
#include "ubse_ictrl_q_resp_msg.h"
#include "adapter_plugins/mti/ubse_mti_urma.h"
namespace ubse::mti::ctrl_q {
using namespace mti::urma;
class UbseCtrlQGetIdevPfeGuidReqMsg : public ICtrlQReqMsg {
public:
    explicit UbseCtrlQGetIdevPfeGuidReqMsg(const UbseMtiIdevPfe &pfe);

    UbseResult EncodeReqMsg() override;

private:
    UbseMtiIdevPfe pfe_;
};

class UbseCtrlQGetIdevPfeGuidRespMsg : public ICtrlQRespMsg {
public:
    UbseCtrlQGetIdevPfeGuidRespMsg() = default;

    UbseResult DecodeRespMsg(const CtrlQRespMessage &msg) override;

    const UbseMtiGuid &GetGuid() const;

private:
    UbseMtiGuid guid_;
};

class UbseCtrlQGetIdevVfeGuidReqMsg : public ICtrlQReqMsg {
public:
    explicit UbseCtrlQGetIdevVfeGuidReqMsg(const UbseMtiIdevVfe &vfe);

    UbseResult EncodeReqMsg() override;

private:
    UbseMtiIdevVfe vfe_;
};

class UbseCtrlQGetIdevVfeGuidRespMsg : public UbseCtrlQGetIdevPfeGuidRespMsg {
public:
    UbseCtrlQGetIdevVfeGuidRespMsg() = default;
};
} // namespace ubse::mti::ctrl_q
#endif // UBSE_CTRL_Q_GET_FE_GUID_MSG_H