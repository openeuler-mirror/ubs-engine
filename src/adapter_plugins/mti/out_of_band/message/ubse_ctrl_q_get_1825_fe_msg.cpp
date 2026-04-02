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

#include "ubse_ctrl_q_get_1825_fe_msg.h"
#include "../proxy/ubse_ctrl_q_msg_proxy.h"
#include "adapter_plugins/mti/ubse_mti_1825.h"
#include "ubse_ctrl_q_get_fe_guid_msg.h"
#include "ubse_ctrl_q_msg_helper.h"
#include "ubse_error.h"
#include "ubse_logger.h"
namespace ubse::mti::ctrl_q {
using namespace ubse::log;
using namespace ubse::mti::_1825;
UBSE_DEFINE_THIS_MODULE("ubse");
static const uint8_t GET_1825_FE_OP_CODE = 0xb;
const uint8_t INVALID_PF_ID = 0xFF;

struct RespReader {
    FixedHead head;
} __attribute__((packed));

UbseCtrlQGet1825FeReqMsg::UbseCtrlQGet1825FeReqMsg() : ICtrlQReqMsg(GET_1825_FE_OP_CODE) {}

UbseResult UbseCtrlQGet1825FeReqMsg::EncodeReqMsg()
{
    return UBSE_OK;
}

static UbseResult GetGuid(ICtrlQReqMsg &reqMsg, UbseCtrlQGetIdevPfeGuidRespMsg &respMsg, UbseMtiGuid &guid)
{
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get guid failed";
        return ret;
    }
    guid = respMsg.GetGuid();
    return UBSE_OK;
}

static UbseResult GetRespResult(std::vector<UbseMti1825Pf> &pfeList, UbseCtrlQMsgReadHelper &readHelper)
{
    std::unordered_map<uint8_t, UbseMti1825Pf> pfMap;
    std::unordered_map<uint8_t, std::vector<UbseMti1825Vf>> pfIdVfMap;
    auto feNum = readHelper.Read<uint8_t>();
    for (uint8_t feId = 0; feId < feNum; feId++) {
        auto slotId = readHelper.Read<uint8_t>();
        auto chipId = readHelper.Read<uint8_t>();
        auto dieId = readHelper.Read<uint8_t>();
        auto pfId = readHelper.Read<uint8_t>();
        auto vfId = readHelper.Read<uint8_t>();
        if (pfId == 0xFF) {
            UBSE_LOG_ERROR << "Invalid pfId";
            return UBSE_ERROR;
        }
        if (vfId == INVALID_PF_ID) {
            auto pf = UbseMti1825Pf(slotId, chipId, dieId, pfId);
            auto reqMsg = UbseCtrlQGet1825PfGuidReqMsg(pf);
            auto respMsg = UbseCtrlQGet1825PfGuidRespMsg();
            auto ret = GetGuid(reqMsg, respMsg, pf.guid);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Get pf guid for pf: " << pfId << " failed";
                return ret;
            }
            pfMap[pfId] = pf;
        } else {
            auto vf = UbseMti1825Vf(slotId, chipId, dieId, pfId, vfId);
            auto reqMsg = UbseCtrlQGet1825VfGuidReqMsg(vf);
            auto respMsg = UbseCtrlQGet1825VfGuidRespMsg();
            auto ret = GetGuid(reqMsg, respMsg, vf.guid);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Get vf guid for vf: " << vfId << " failed";
                return ret;
            }
            pfIdVfMap[pfId].emplace_back(vf);
        }
    }
    for (auto &[pfId, pf] : pfMap) {
        auto tmpPf = pf;
        if (pfIdVfMap.find(pfId) != pfIdVfMap.end()) {
            tmpPf.vfList = pfIdVfMap[pfId];
        }
        pfeList.emplace_back(tmpPf);
    }
    return UBSE_OK;
}

UbseResult UbseCtrlQGet1825PfeRespMsg::DecodeRespMsg(const CtrlQRespMessage &msg)
{
    // bbNum 为0时，不检查bbNum
    if (!CheckRespValidation(msg, 0, GET_1825_FE_OP_CODE)) {
        return UBSE_ERROR;
    }

    auto pos = reinterpret_cast<uint8_t *>(msg.blocks) + sizeof(RespReader);
    auto end = reinterpret_cast<uint8_t *>(msg.blocks) + sizeof(BasicBlock) * msg.blockNums;

    UbseCtrlQMsgReadHelper readHelper(pos, end);
    try {
        return GetRespResult(pfList_, readHelper);
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Read get 1825 pf opt resp failed: " << e.what();
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
const std::vector<UbseMti1825Pf> &UbseCtrlQGet1825PfeRespMsg::GetPfList() const
{
    return pfList_;
}
} // namespace ubse::mti::ctrl_q