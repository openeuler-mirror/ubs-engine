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
#include "ubse_error.h"
#include "ubse_logger.h"
#include "./message/ubse_ctrl_q_businstance_msg.h"
#include "./message/ubse_ctrl_q_fe_opt_msg.h"
#include "./message/ubse_ctrl_q_get_idev_fe_david_mapping_msg.h"
#include "./message/ubse_ctrl_q_get_idev_fe_msg.h"
#include "./message/ubse_ctrl_q_vfe_david_opt_msg.h"
#include "./message/ubse_ictrl_q_req_msg.h"
#include "./proxy/ubse_ctrl_q_msg_proxy.h"
namespace ubse::mti::urma {
using namespace ubse::mti::ctrl_q;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseMtiUrmaOutOfBand::GetIdevFeList(std::vector<UbseMtiIdevPfe>& feList)
{
    UbseCtrlQGetIdevFeReqMsg reqMsg;
    UbseCtrlQGetIdevFeRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetIdevFeList failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    feList = respMsg.GetPfeList();
    return UBSE_OK;
}

UbseResult UbseMtiUrmaOutOfBand::GetIdevFeDavidMapping(UbseMtiIdevFeDavidMapping& mapping)
{
    UbseCtrlQGetIdevFeDavidMappingReqMsg reqMsg;
    UbseCtrlQGetIdevFeDavidMappingRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetIdevFeDavidMapping failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    mapping = respMsg.GetMapping();
    return UBSE_OK;
}

UbseResult UbseMtiUrmaOutOfBand::BindDavid(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair>& vfeDavidList,
                                           std::vector<bool>& resList)
{
    UbseCtrlQBindVfeDavidReqMsg reqMsg(upi, vfeDavidList);
    UbseCtrlQBindVfeDavidRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "BindDavid failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    resList = respMsg.GetRetList();
    return UBSE_OK;
}

UbseResult UbseMtiUrmaOutOfBand::UnBindDavid(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair>& vfeDavidList,
                                             std::vector<bool>& resList)
{
    UbseCtrlQUnBindVfeDavidReqMsg reqMsg(upi, vfeDavidList);
    UbseCtrlQUnBindVfeDavidRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UnBindDavid failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    resList = respMsg.GetRetList();
    return UBSE_OK;
}

UbseResult UbseMtiUrmaOutOfBand::RegDavidFeToVmBusInstance(const UbseMtiBusInst& busInstance,
                                                           const std::vector<UbseMtiIdevVfe>& vfeList,
                                                           std::vector<bool>& resList)
{
    UbseCtrlQRegDavidFeToBusInstanceReqMsg reqMsg(busInstance, vfeList);
    UbseCtrlQRegDavidFeToBusInstanceRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegDavidFeToVmBusInstance failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    resList = respMsg.GetRetList();
    return UBSE_OK;
}

UbseResult UbseMtiUrmaOutOfBand::UnRegDavidFeFromVmBusInstance(const UbseMtiBusInst& busInstance,
                                                               const std::vector<UbseMtiIdevVfe>& vfeList,
                                                               std::vector<bool>& resList)
{
    UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg reqMsg(busInstance, vfeList);
    UbseCtrlQUnRegDavidFeFromBusInstanceRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UnRegDavidFeFromVmBusInstance failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    resList = respMsg.GetRetList();
    return UBSE_OK;
}
} // namespace ubse::mti::urma
