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
#include "ubse_ctrl_q_get_idev_fe_msg.h"
#include <set>
#include "ubse_ctrl_q_get_fe_guid_msg.h"
#include "ubse_ctrl_q_get_idev_fe_david_mapping_msg.h"
#include "ubse_ctrl_q_message.h"
#include "ubse_ctrl_q_msg_helper.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "../proxy/ubse_ctrl_q_msg_proxy.h"
namespace ubse::mti::ctrl_q {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");
static const uint8_t GET_IDEV_FE_OP_CODE = 0x1;

struct RespReader {
    FixedHead head;
} __attribute__((packed));

UbseCtrlQGetIdevFeReqMsg::UbseCtrlQGetIdevFeReqMsg() : ICtrlQReqMsg(GET_IDEV_FE_OP_CODE) {}

UbseResult UbseCtrlQGetIdevFeReqMsg::EncodeReqMsg()
{
    return UBSE_OK;
}

static UbseResult GetGuid(ICtrlQReqMsg& reqMsg, UbseCtrlQGetIdevPfeGuidRespMsg& respMsg, UbseMtiGuid& guid)
{
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get guid failed";
        return ret;
    }
    guid = respMsg.GetGuid();
    return UBSE_OK;
}

static UbseResult AddPfe(uint8_t pfeNum, const UbseMtiUbController& ubController,
                         const std::set<UbseMtiIdevPfe>& pfeSet, std::vector<UbseMtiIdevPfe>& pfeList,
                         UbseCtrlQMsgReadHelper& readHelper)
{
    for (uint8_t pfeId = 0; pfeId < pfeNum; pfeId++) {
        UbseMtiIdevPfe pfe(ubController, pfeId);
        auto vfeNum = readHelper.Read<uint8_t>();
        auto it = pfeSet.find(pfe);
        // 只有与David绑定的pfe才添加
        if (it == pfeSet.end()) {
            continue;
        }
        UbseCtrlQGetIdevPfeGuidReqMsg getPfeGuidReq(pfe);
        UbseCtrlQGetIdevPfeGuidRespMsg getPfeGuidResp;
        if (GetGuid(getPfeGuidReq, getPfeGuidResp, pfe.guid) != UBSE_OK) {
            UBSE_LOG_ERROR << "Get pfe guid failed, chipId=  " << ubController.chipId
                           << ", dieId= " << ubController.dieId << ", pfeId=" << pfeId;
            return UBSE_ERROR;
        }
        // 只有vfe id 为0的vfe才查询Guid
        uint8_t vfeId = 0;
        UbseMtiIdevVfe vfe(ubController, pfeId, vfeId);
        UbseCtrlQGetIdevVfeGuidReqMsg getVfeGuidReq(vfe);
        UbseCtrlQGetIdevVfeGuidRespMsg getVfeGuidResp;
        if (GetGuid(getVfeGuidReq, getVfeGuidResp, vfe.guid) != UBSE_OK) {
            UBSE_LOG_ERROR << "Get vfe guid failed, chipId=  " << ubController.chipId
                           << ", dieId= " << ubController.dieId << ", pfeId=" << pfeId << ", vfeId=" << vfeId;
            return UBSE_ERROR;
        }
        pfe.AddVfe(vfe);
        pfeList.emplace_back(pfe);
    }
    return UBSE_OK;
}

static UbseResult GetRespResult(const std::set<UbseMtiIdevPfe>& pfeSet, std::vector<UbseMtiIdevPfe>& pfeList,
                                UbseCtrlQMsgReadHelper& readHelper)
{
    auto chipNum = readHelper.Read<uint8_t>();
    for (uint8_t chipIdx = 0; chipIdx < chipNum; chipIdx++) {
        auto chipId = readHelper.Read<uint8_t>();
        auto dieNum = readHelper.Read<uint8_t>();
        for (uint8_t dieIdx = 0; dieIdx < dieNum; dieIdx++) {
            auto dieId = readHelper.Read<uint8_t>();
            auto totalFeNum = readHelper.Read<uint8_t>();
            auto pfeNum = readHelper.Read<uint8_t>();
            UbseMtiUbController ubController(chipId, dieId);
            auto ret = AddPfe(pfeNum, ubController, pfeSet, pfeList, readHelper);
            if (ret != UBSE_OK) {
                return ret;
            }
        }
    }
    return UBSE_OK;
}

UbseResult GetPfeMappingDavidSet(std::set<UbseMtiIdevPfe>& pfeSet)
{
    UbseCtrlQGetIdevFeDavidMappingReqMsg reqMsg;
    UbseCtrlQGetIdevFeDavidMappingRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get fe david mapping failed";
        return ret;
    }
    auto mapping = respMsg.GetMapping();
    for (auto& pair : mapping) {
        pfeSet.emplace(pair.second);
    }
    return UBSE_OK;
}

const std::vector<UbseMtiIdevPfe>& UbseCtrlQGetIdevFeRespMsg::GetPfeList() const
{
    return pfeList_;
}

UbseResult UbseCtrlQGetIdevFeRespMsg::DecodeRespMsg(const CtrlQRespMessage& msg)
{
    // bbNum 为0时，不检查bbNum
    if (!CheckRespValidation(msg, 0, GET_IDEV_FE_OP_CODE)) {
        return UBSE_ERROR;
    }

    auto pos = reinterpret_cast<uint8_t*>(msg.blocks) + sizeof(RespReader);
    auto end = reinterpret_cast<uint8_t*>(msg.blocks) + sizeof(BasicBlock) * msg.blockNums;

    UbseCtrlQMsgReadHelper readHelper(pos, end);
    std::set<UbseMtiIdevPfe> pfeSet;
    if (GetPfeMappingDavidSet(pfeSet) != UBSE_OK) {
        UBSE_LOG_ERROR << "Get pfe david mapping failed";
        return UBSE_ERROR;
    }
    try {
        return GetRespResult(pfeSet, pfeList_, readHelper);
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Read get idev fe opt resp failed: " << e.what();
        return UBSE_ERROR;
    }
}
} // namespace ubse::mti::ctrl_q