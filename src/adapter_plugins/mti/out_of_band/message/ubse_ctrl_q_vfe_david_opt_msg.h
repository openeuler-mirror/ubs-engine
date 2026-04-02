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

#ifndef UBSE_CTRL_Q_VFE_DAVID_OPT_MSG_H
#define UBSE_CTRL_Q_VFE_DAVID_OPT_MSG_H
#include "ubse_ictrl_q_req_msg.h"
#include "ubse_ictrl_q_resp_msg.h"
#include "adapter_plugins/mti/ubse_mti_urma.h"

namespace ubse::mti::ctrl_q {
using namespace mti::urma;

class UbseCtrlQBindVfeDavidReqMsg : public ICtrlQReqMsg {
public:
    UbseCtrlQBindVfeDavidReqMsg() = delete;

    explicit UbseCtrlQBindVfeDavidReqMsg(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair> &vfeDavidList);

    UbseResult EncodeReqMsg() override;

private:
    uint16_t upi_;
    std::vector<UbseMtiIdevVfeDavidPair> vfeDavidList_;
};

class UbseCtrlQUnBindVfeDavidReqMsg : public ICtrlQReqMsg {
public:
    UbseCtrlQUnBindVfeDavidReqMsg() = delete;

    explicit UbseCtrlQUnBindVfeDavidReqMsg(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair> &vfeDavidList);

    UbseResult EncodeReqMsg() override;

private:
    uint16_t upi_;
    std::vector<UbseMtiIdevVfeDavidPair> vfeDavidList_;
};

class UbseCtrlQBindVfeDavidRespMsg : public ICtrlQRespMsg {
public:
    UbseCtrlQBindVfeDavidRespMsg() = default;

    UbseResult DecodeRespMsg(const CtrlQRespMessage &msg) override;

    const std::vector<bool> &GetRetList() const;

private:
    std::vector<bool> retList_;
};

class UbseCtrlQUnBindVfeDavidRespMsg : public ICtrlQRespMsg {
public:
    UbseCtrlQUnBindVfeDavidRespMsg() = default;

    UbseResult DecodeRespMsg(const CtrlQRespMessage &msg) override;

    const std::vector<bool> &GetRetList() const;

private:
    std::vector<bool> retList_;
};

} // namespace ubse::mti::ctrl_q
#endif // UBSE_CTRL_Q_VFE_DAVID_OPT_MSG_H