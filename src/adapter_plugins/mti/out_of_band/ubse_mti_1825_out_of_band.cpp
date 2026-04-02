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
#include "ubse_mti_1825_out_of_band.h"
#include "./message/ubse_ctrl_q_fe_opt_msg.h"
#include "./message/ubse_ctrl_q_get_1825_fe_msg.h"
#include "./proxy/ubse_ctrl_q_msg_proxy.h"
#include "ubse_error.h"
#include "ubse_logger.h"
namespace ubse::mti::_1825 {
using namespace ubse::log;
using namespace ubse::mti::ctrl_q;
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult Reg1825FeToBusInstance(const UbseMtiBusInst &busInstance, UbseMtiBusInstanceType type,
                                  const std::vector<UbseMti1825Vf> &vfList, std::vector<bool> &resList)
{
    if (busInstance.type != type) {
        UBSE_LOG_ERROR << "Invalid bus instance type";
        return UBSE_ERROR;
    }
    UbseCtrlQReg1825FeToBusInstanceReqMsg reqMsg(busInstance, vfList);
    UbseCtrlQReg1825FeToBusInstanceRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Reg 1825 fe to bus instance failed";
        return ret;
    }
    resList = respMsg.GetRetList();
    return UBSE_OK;
}

UbseResult UnReg1825FeFromBusInstance(const UbseMtiBusInst &busInstance, UbseMtiBusInstanceType type,
                                      const std::vector<UbseMti1825Vf> &vfList, std::vector<bool> &resList)
{
    if (busInstance.type != type) {
        UBSE_LOG_ERROR << "Invalid bus instance type";
        return UBSE_ERROR;
    }
    UbseCtrlQUnReg1825FeFromBusInstanceReqMsg reqMsg(busInstance, vfList);
    UbseCtrlQUnReg1825FeFromBusInstanceRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unreg 1825 fe from bus instance failed";
        return ret;
    }
    resList = respMsg.GetRetList();
    return UBSE_OK;
}

UbseResult UbseMti1825OutOfBand::Get1825FeList(std::vector<UbseMti1825Pf> &pfList)
{
    UbseCtrlQGet1825FeReqMsg reqMsg;
    UbseCtrlQGet1825PfeRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get 1825 fe list failed";
        return ret;
    }
    pfList = respMsg.GetPfList();
    return UBSE_OK;
}

UbseResult UbseMti1825OutOfBand::Reg1825FeToHostBusInstance(const UbseMtiBusInst &busInstance,
                                                            const std::vector<UbseMti1825Vf> &vfList,
                                                            std::vector<bool> &resList)
{
    return Reg1825FeToBusInstance(busInstance, UbseMtiBusInstanceType::HOST, vfList, resList);
}

UbseResult UbseMti1825OutOfBand::UnReg1825FeFromHostBusInstance(const UbseMtiBusInst &busInstance,
                                                                const std::vector<UbseMti1825Vf> &vfList,
                                                                std::vector<bool> &resList)
{
    return UnReg1825FeFromBusInstance(busInstance, UbseMtiBusInstanceType::HOST, vfList, resList);
}

UbseResult UbseMti1825OutOfBand::Reg1825FeToVmBusInstance(const UbseMtiBusInst &busInstance,
                                                          const std::vector<UbseMti1825Vf> &vfList,
                                                          std::vector<bool> &resList)
{
    return Reg1825FeToBusInstance(busInstance, UbseMtiBusInstanceType::VM, vfList, resList);
}

UbseResult UbseMti1825OutOfBand::UnReg1825FeFromVmBusInstance(const UbseMtiBusInst &busInstance,
                                                              const std::vector<UbseMti1825Vf> &vfList,
                                                              std::vector<bool> &resList)
{
    return UnReg1825FeFromBusInstance(busInstance, UbseMtiBusInstanceType::VM, vfList, resList);
}
} // namespace ubse::mti::_1825
