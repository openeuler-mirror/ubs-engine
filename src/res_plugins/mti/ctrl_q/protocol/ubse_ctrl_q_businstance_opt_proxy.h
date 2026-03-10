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

#ifndef UBSE_CTRL_Q_CREATE_BUSINSTANCE_PROXY_H
#define UBSE_CTRL_Q_CREATE_BUSINSTANCE_PROXY_H
#include "../ubse_ictrl_q_msg_operation_proxy.h"
#include "../ubse_ictrl_q_req_msg.h"
#include "adapter_plugins/mti/ubse_mti_bus_instance.h"

namespace ubse::mti::ctrl_q {
using namespace mti::bus_instance;
class UbseCtrlQCreateBusInstanceReqMsg : public ICtrlQReqMsg {
public:
    UbseCtrlQCreateBusInstanceReqMsg(uint16_t upi, uint16_t vendor);

    UbseResult GetReqMsg(CtrlQReqMessage &msg) override;

    uint16_t GetUpi();

    uint16_t GetVendor();

private:
    uint16_t upi_;
    uint16_t vendor_;
};

class UbseCtrlQCreateBusInstanceProxy : public ICtrlQMsgOperationProxy<UbseMtiBusInst> {
public:
    static const uint8_t OP_CODE;
    UbseCtrlQCreateBusInstanceProxy() = default;
    ~UbseCtrlQCreateBusInstanceProxy() override = default;

private:
    bool CheckReqValidation(const CtrlQReqMessage &msg) override;

    UbseResult ConvertRespMsgToUserData(const ICtrlQReqMsg &reqMsg, const CtrlQRespMessage &msg) override;
};

class UbseCtrlQDestroyBusInstanceReqMsg : public ICtrlQReqMsg {
public:
    UbseCtrlQDestroyBusInstanceReqMsg(const UbseMtiBusInst &busInstance);

    UbseResult GetReqMsg(CtrlQReqMessage &msg) override;

private:
    UbseMtiBusInst busInstance_;
};

class UbseCtrlQDestroyBusInstanceProxy : public ICtrlQMsgOperationProxy<bool> {
public:
    static const uint8_t OP_CODE;
    UbseCtrlQDestroyBusInstanceProxy() = default;
    ~UbseCtrlQDestroyBusInstanceProxy() override = default;

private:
    bool CheckReqValidation(const CtrlQReqMessage &msg) override;

    UbseResult ConvertRespMsgToUserData(const ICtrlQReqMsg &reqMsg, const CtrlQRespMessage &msg) override;
};
} // namespace ubse::mti::ctrl_q
#endif // UBSE_CTRL_Q_CREATE_BUSINSTANCE_PROXY_H