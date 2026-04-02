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

#ifndef UBSE_CTRL_Q_BUSINSTANCE_MSG_H
#define UBSE_CTRL_Q_BUSINSTANCE_MSG_H
#include "adapter_plugins/mti/ubse_mti_bus_instance.h"
#include "ubse_ictrl_q_req_msg.h"
#include "ubse_ictrl_q_resp_msg.h"

namespace ubse::mti::ctrl_q {
using namespace mti::bus_instance;
class UbseCtrlQCreateBusInstanceReqMsg : public ICtrlQReqMsg {
public:
    explicit UbseCtrlQCreateBusInstanceReqMsg(uint16_t upi, uint16_t vendor);

    UbseResult EncodeReqMsg() override;

    uint16_t GetUpi();

    uint16_t GetVendor();

private:
    uint16_t upi_;
    uint16_t vendor_;
};

class UbseCtrlQCreateBusInstanceRespMsg : public ICtrlQRespMsg {
public:
    UbseCtrlQCreateBusInstanceRespMsg() = default;

    UbseResult DecodeRespMsg(const CtrlQRespMessage &msg) override;

    const UbseMtiBusInst &GetBusInstance() const;

private:
    UbseMtiBusInst busInstance_;
};

class UbseCtrlQDestroyBusInstanceReqMsg : public ICtrlQReqMsg {
public:
    explicit UbseCtrlQDestroyBusInstanceReqMsg(const UbseMtiBusInst &busInstance);

    UbseResult EncodeReqMsg() override;

private:
    UbseMtiBusInst busInstance_;
};

class UbseCtrlQDestroyBusInstanceRespMsg : public ICtrlQRespMsg {
public:
    UbseCtrlQDestroyBusInstanceRespMsg() = default;

    UbseResult DecodeRespMsg(const CtrlQRespMessage &msg) override;

    const bool &GetRet() const;

private:
    bool ret_ = false;
};
} // namespace ubse::mti::ctrl_q
#endif // UBSE_CTRL_Q_BUSINSTANCE_MSG_H