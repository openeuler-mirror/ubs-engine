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
#include "ubse_mti_bus_instance_default_impl.h"
#include "./ctrl_q/protocol/ubse_ctrl_q_businstance_opt_proxy.h"
#include "./ctrl_q/ubse_ictrl_q_req_msg.h"
#include "ubse_error.h"
#include "ubse_logger.h"
namespace ubse::mti::bus_instance {
using namespace ubse::mti::ctrl_q;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");
const uint16_t DEFAULT_VENDOR = 0;

UbseResult UbseMtiBusInstanceDefaultImpl::GetBusInstanceList(std::vector<UbseMtiBusInst> &busInstanceList)
{
    return UBSE_OK;
}

UbseResult UbseMtiBusInstanceDefaultImpl::CreateVmBusInstance(uint16_t upi, UbseMtiBusInst &busInstance)
{
    UbseCtrlQCreateBusInstanceReqMsg reqMsg(upi, DEFAULT_VENDOR);
    UbseCtrlQCreateBusInstanceProxy proxy;
    auto ret = proxy.SendRequest(reqMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "CreateVmBusInstance failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    busInstance = proxy.GetResponse();
    return UBSE_OK;
}

UbseResult UbseMtiBusInstanceDefaultImpl::DestroyVmBusInstance(const UbseMtiBusInst &busInstance)
{
    UbseCtrlQDestroyBusInstanceReqMsg reqMsg(busInstance);
    UbseCtrlQDestroyBusInstanceProxy proxy;
    auto ret = proxy.SendRequest(reqMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "DestroyVmBusInstance failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}
} // namespace ubse::mti::bus_instance
