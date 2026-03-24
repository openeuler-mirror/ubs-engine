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
#include "ubse_mti_urma_out_of_band.h"
#include "../ctrl_q/protocol/ubse_ctrl_q_businstance_opt_proxy.h"
#include "../ctrl_q/protocol/ubse_ctrl_q_dev_opt_proxy.h"
#include "../ctrl_q/protocol/ubse_ctrl_q_get_idev_fe_david_mapping_proxy.h"
#include "../ctrl_q/protocol/ubse_ctrl_q_get_idev_fe_proxy.h"
#include "../ctrl_q/protocol/ubse_ctrl_q_vfe_david_proxy.h"
#include "../ctrl_q/ubse_ictrl_q_req_msg.h"
#include "ubse_error.h"
#include "ubse_logger.h"
namespace ubse::mti::urma {
using namespace ubse::mti::ctrl_q;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseMtiUrmaOutOfBand::GetIdevFeList(std::vector<UbseMtiIdevPfe> &feList)
{
    UbseCtrlQGetIdevFeReqMsg reqMsg;
    UbseCtrlQGetIdevFeProxy proxy;
    auto ret = proxy.SendRequest(reqMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetIdevFeList failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    feList = proxy.GetResponse();
    return UBSE_OK;
}

UbseResult UbseMtiUrmaOutOfBand::GetIdevFeDavidMapping(UbseMtiIdevFeDavidMapping &mapping)
{
    UbseCtrlQGetIdevFeDavidMappingReqMsg reqMsg;
    UbseCtrlQGetIdevFeDavidMappingProxy proxy;
    auto ret = proxy.SendRequest(reqMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetIdevFeDavidMapping failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    mapping = proxy.GetResponse();
    return UBSE_OK;
}

UbseResult UbseMtiUrmaOutOfBand::BindDavid(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair> &vfeDavidList,
                                           std::vector<bool> &resList)
{
    UbseCtrlQBindVfeDavidReqMsg reqMsg(upi, vfeDavidList);
    UbseCtrlQBindVfeDavidProxy proxy;
    auto ret = proxy.SendRequest(reqMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "BindDavid failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    resList = proxy.GetResponse();
    return UBSE_OK;
}

UbseResult UbseMtiUrmaOutOfBand::UnBindDavid(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair> &vfeDavidList,
                                             std::vector<bool> &resList)
{
    UbseCtrlQUnBindVfeDavidReqMsg reqMsg(upi, vfeDavidList);
    UbseCtrlQUnBindVfeDavidProxy proxy;
    auto ret = proxy.SendRequest(reqMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UnBindDavid failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    resList = proxy.GetResponse();
    return UBSE_OK;
}

UbseResult UbseMtiUrmaOutOfBand::RegDavidFeToVmBusInstance(const UbseMtiBusInst &busInstance,
                                                           const std::vector<UbseMtiIdevVfe> &vfeList,
                                                           std::vector<bool> &resList)
{
    UbseCtrlQRegDavidFeToBusInstanceReqMsg reqMsg(busInstance, vfeList);
    UbseCtrlQRegDevProxy proxy;
    auto ret = proxy.SendRequest(reqMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegDavidFeToVmBusInstance failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    resList = proxy.GetResponse();
    return UBSE_OK;
}

UbseResult UbseMtiUrmaOutOfBand::UnRegDavidFeFromVmBusInstance(const UbseMtiBusInst &busInstance,
                                                               const std::vector<UbseMtiIdevVfe> &vfeList,
                                                               std::vector<bool> &resList)
{
    UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg reqMsg(busInstance, vfeList);
    UbseCtrlQUnRegDevProxy proxy;
    auto ret = proxy.SendRequest(reqMsg);
    if (ret != UBSE_OK) {
        return ret;
    }
    resList = proxy.GetResponse();
    return UBSE_OK;
}
} // namespace ubse::mti::urma
