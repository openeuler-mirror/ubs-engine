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

#ifndef UBSE_CTRL_Q_FE_OPT_MSG_H
#define UBSE_CTRL_Q_FE_OPT_MSG_H
#include "adapter_plugins/mti/ubse_mti_1825.h"
#include "adapter_plugins/mti/ubse_mti_bus_instance.h"
#include "adapter_plugins/mti/ubse_mti_urma.h"
#include "ubse_ictrl_q_req_msg.h"
#include "ubse_ictrl_q_resp_msg.h"

namespace ubse::mti::ctrl_q {
using namespace mti::urma;
using namespace mti::_1825;
using namespace mti::bus_instance;

class UbseCtrlQRegDavidFeToBusInstanceReqMsg : public ICtrlQReqMsg {
public:
    explicit UbseCtrlQRegDavidFeToBusInstanceReqMsg(const UbseMtiBusInst &busInstance,
                                                    const std::vector<UbseMtiIdevVfe> &vfeList);

    UbseResult EncodeReqMsg() override;

private:
    UbseMtiBusInst busInstance_;
    std::vector<UbseMtiIdevVfe> vfeList_;
};

class UbseCtrlQRegDavidFeToBusInstanceRespMsg : public ICtrlQRespMsg {
public:
    UbseCtrlQRegDavidFeToBusInstanceRespMsg() = default;

    UbseResult DecodeRespMsg(const CtrlQRespMessage &msg) override;

    const std::vector<bool> &GetRetList() const;

private:
    std::vector<bool> retList_;
};

class UbseCtrlQReg1825FeToBusInstanceReqMsg : public ICtrlQReqMsg {
public:
    explicit UbseCtrlQReg1825FeToBusInstanceReqMsg(const UbseMtiBusInst &busInstance,
                                                   const std::vector<UbseMti1825Vf> &vfList);

    UbseResult EncodeReqMsg() override;

private:
    UbseMtiBusInst busInstance_;
    std::vector<UbseMti1825Vf> vfList_;
};

class UbseCtrlQReg1825FeToBusInstanceRespMsg : public ICtrlQRespMsg {
public:
    UbseCtrlQReg1825FeToBusInstanceRespMsg() = default;

    UbseResult DecodeRespMsg(const CtrlQRespMessage &msg) override;

    const std::vector<bool> &GetRetList() const;

private:
    std::vector<bool> retList_;
};

class UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg : public ICtrlQReqMsg {
public:
    explicit UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg(const UbseMtiBusInst &busInstance,
                                                        const std::vector<UbseMtiIdevVfe> &vfeList);

    UbseResult EncodeReqMsg() override;

private:
    UbseMtiBusInst busInstance_;
    std::vector<UbseMtiIdevVfe> vfeList_;
};

class UbseCtrlQUnRegDavidFeFromBusInstanceRespMsg : public ICtrlQRespMsg {
public:
    UbseCtrlQUnRegDavidFeFromBusInstanceRespMsg() = default;

    UbseResult DecodeRespMsg(const CtrlQRespMessage &msg) override;

    const std::vector<bool> &GetRetList() const;

private:
    std::vector<bool> retList_;
};

class UbseCtrlQUnReg1825FeFromBusInstanceReqMsg : public ICtrlQReqMsg {
public:
    explicit UbseCtrlQUnReg1825FeFromBusInstanceReqMsg(const UbseMtiBusInst &busInstance,
                                                       const std::vector<UbseMti1825Vf> &vfList);

    UbseResult EncodeReqMsg() override;

private:
    UbseMtiBusInst busInstance_;
    std::vector<UbseMti1825Vf> vfList_;
};

class UbseCtrlQUnReg1825FeFromBusInstanceRespMsg : public ICtrlQRespMsg {
public:
    UbseCtrlQUnReg1825FeFromBusInstanceRespMsg() = default;

    UbseResult DecodeRespMsg(const CtrlQRespMessage &msg) override;

    const std::vector<bool> &GetRetList() const;

private:
    std::vector<bool> retList_;
};
} // namespace ubse::mti::ctrl_q
#endif // UBSE_CTRL_Q_FE_OPT_MSG_H