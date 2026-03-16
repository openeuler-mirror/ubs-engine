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

#ifndef UBSE_CTRL_Q_DEV_OPT_PROXY_H
#define UBSE_CTRL_Q_DEV_OPT_PROXY_H
#include "../ubse_ictrl_q_msg_operation_proxy.h"
#include "../ubse_ictrl_q_req_msg.h"
#include "adapter_plugins/mti/ubse_mti_1825.h"
#include "adapter_plugins/mti/ubse_mti_bus_instance.h"
#include "adapter_plugins/mti/ubse_mti_urma.h"

namespace ubse::mti::ctrl_q {
using namespace mti::urma;
using namespace mti::_1825;
using namespace mti::bus_instance;

class UbseCtrlQRegDavidFeToBusInstanceReqMsg : public ICtrlQReqMsg {
public:
    explicit UbseCtrlQRegDavidFeToBusInstanceReqMsg(const UbseMtiBusInst &busInstance,
                                                    const std::vector<UbseMtiIdevVfe> &vfeList);

    UbseResult GetReqMsg(CtrlQReqMessage &msg) override;

private:
    UbseMtiBusInst busInstance_;
    std::vector<UbseMtiIdevVfe> vfeList_;
};

class UbseCtrlQReg1825FeToBusInstanceReqMsg : public ICtrlQReqMsg {
public:
    explicit UbseCtrlQReg1825FeToBusInstanceReqMsg(const UbseMtiBusInst &busInstance,
                                                   const std::vector<UbseMti1825Vf> &vfList);

    UbseResult GetReqMsg(CtrlQReqMessage &msg) override;

private:
    UbseMtiBusInst busInstance_;
    std::vector<UbseMti1825Vf> vfList_;
};

class UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg : public ICtrlQReqMsg {
public:
    explicit UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg(const UbseMtiBusInst &busInstance,
                                                        const std::vector<UbseMtiIdevVfe> &vfeList);

    UbseResult GetReqMsg(CtrlQReqMessage &msg) override;

private:
    UbseMtiBusInst busInstance_;
    std::vector<UbseMtiIdevVfe> vfeList_;
};

class UbseCtrlQUnReg1825FeFromBusInstanceReqMsg : public ICtrlQReqMsg {
public:
    explicit UbseCtrlQUnReg1825FeFromBusInstanceReqMsg(const UbseMtiBusInst &busInstance,
                                                       const std::vector<UbseMti1825Vf> &vfList);

    UbseResult GetReqMsg(CtrlQReqMessage &msg) override;

private:
    UbseMtiBusInst busInstance_;
    std::vector<UbseMti1825Vf> vfList_;
};

class UbseCtrlQRegDevProxy : public ICtrlQMsgOperationProxy<std::vector<bool>> {
public:
    static const uint8_t OP_CODE = 0x7;
    UbseCtrlQRegDevProxy() = default;
    ~UbseCtrlQRegDevProxy() override = default;

private:
    bool CheckReqValidation(const CtrlQReqMessage &msg) override;

    UbseResult ConvertRespMsgToUserData(const ICtrlQReqMsg &reqMsg, const CtrlQRespMessage &msg) override;
};

class UbseCtrlQUnRegDevProxy : public ICtrlQMsgOperationProxy<std::vector<bool>> {
public:
    static const uint8_t OP_CODE = 0x8;
    UbseCtrlQUnRegDevProxy() = default;
    ~UbseCtrlQUnRegDevProxy() override = default;

private:
    bool CheckReqValidation(const CtrlQReqMessage &msg) override;

    UbseResult ConvertRespMsgToUserData(const ICtrlQReqMsg &reqMsg, const CtrlQRespMessage &msg) override;
};
} // namespace ubse::mti::ctrl_q
#endif // UBSE_CTRL_Q_DEV_OPT_PROXY_H