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

#include "ubse_ctrl_q_dev_opt_proxy.h"
#include "../ubse_ctrl_q_message.h"
#include "../ubse_ctrl_q_msg_helper.h"
#include "securec.h"
#include "ubse_error.h"
#include "ubse_logger.h"
namespace ubse::mti::ctrl_q {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

struct UbseCtrlQRegDevReqMsg {
    FixedHead head;
    struct {
        uint8_t content[3];
    } __attribute__((packed)) eid;
} __attribute__((packed));

struct RegInfo {
    FeLoc fe;
    UbController ubController;
} __attribute__((packed));

struct RespReader {
    FixedHead head;
} __attribute__((packed));

static uint32_t CalculateTotalSize(const std::vector<RegInfo> &regInfoList)
{
    return sizeof(UbseCtrlQRegDevReqMsg) + static_cast<uint32_t>(regInfoList.size()) * sizeof(RegInfo);
}

UbseResult SetBusInstance(const UbseMtiBusInst &busInstance, CtrlQReqMessage &msg)
{
    auto &ref = *reinterpret_cast<UbseCtrlQRegDevReqMsg *>(&msg.blocks.front());
    auto ret = memcpy_s(&ref.eid, sizeof(ref.eid), busInstance.eid.data(), sizeof(ref.eid));
    if (ret != EOK) {
        UBSE_LOG_ERROR << "Mem copy businstance eid failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseCtrlQRegDavidFeToBusInstanceReqMsg::UbseCtrlQRegDavidFeToBusInstanceReqMsg(
    const UbseMtiBusInst &busInstance, const std::vector<UbseMtiIdevVfe> &vfeList)
    : busInstance_(busInstance),
      vfeList_(vfeList)
{
}

static UbseResult WriteReqMsg(const UbseMtiBusInst &busInstance, const std::vector<RegInfo> &regInfoList,
                              CtrlQReqMessage &msg)
{
    if (SetBusInstance(busInstance, msg) != UBSE_OK) {
        UBSE_LOG_ERROR << "Set businstance failed";
        return UBSE_ERROR;
    }
    auto blockStartPtr = reinterpret_cast<uint8_t *>(msg.blocks.data());
    auto start = blockStartPtr + sizeof(UbseCtrlQRegDevReqMsg);
    auto end = blockStartPtr + msg.blocks.size() * BASIC_BLOCK_SIZE;
    UbseCtrlQMsgWriteHelper writeHelper(start, end);
    try {
        writeHelper.Write<uint8_t>(regInfoList.size());
        for (const auto &regInfo : regInfoList) {
            writeHelper.Write<RegInfo>(regInfo);
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Write vfe david req failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

static void IdevVfeListToRegInfoList(const std::vector<UbseMtiIdevVfe> &vfeList, std::vector<RegInfo> &regInfoList)
{
    for (const auto &vfe : vfeList) {
        FeLoc fe;
        fe.slotId = vfe.ubController.slotId;
        fe.chipId = vfe.ubController.chipId;
        fe.dieId = vfe.ubController.dieId;
        fe.pfeId = vfe.pfeId;
        fe.vfeId = vfe.vfeId;
        UbController ubController;
        ubController.slotId = 0xff;
        ubController.chipId = 0xff;
        ubController.dieId = 0xff;
        regInfoList.emplace_back(RegInfo{fe, ubController});
    }
}

UbseResult UbseCtrlQRegDavidFeToBusInstanceReqMsg::GetReqMsg(CtrlQReqMessage &msg)
{
    SetOpCode(UbseCtrlQRegDevProxy::OP_CODE, msg);
    SetServiceType(DEFAULT_SERVICE_TYPE, msg);
    std::vector<RegInfo> regInfoList;
    IdevVfeListToRegInfoList(vfeList_, regInfoList);
    auto reSize = CalculateTotalSize(regInfoList);
    if (reSize > BASIC_BLOCK_SIZE) {
        ResizeReqMsg((reSize + BASIC_BLOCK_SIZE - 1) / BASIC_BLOCK_SIZE, msg);
    }
    return WriteReqMsg(busInstance_, regInfoList, msg);
}

UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg::UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg(
    const UbseMtiBusInst &busInstance, const std::vector<UbseMtiIdevVfe> &vfeList)
    : busInstance_(busInstance),
      vfeList_(vfeList)
{
}

UbseResult UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg::GetReqMsg(CtrlQReqMessage &msg)
{
    SetOpCode(UbseCtrlQUnRegDevProxy::OP_CODE, msg);
    SetServiceType(DEFAULT_SERVICE_TYPE, msg);
    std::vector<RegInfo> unRegInfoList;
    IdevVfeListToRegInfoList(vfeList_, unRegInfoList);
    auto reSize = CalculateTotalSize(unRegInfoList);
    if (reSize > BASIC_BLOCK_SIZE) {
        ResizeReqMsg((reSize + BASIC_BLOCK_SIZE - 1) / BASIC_BLOCK_SIZE, msg);
    }
    return WriteReqMsg(busInstance_, unRegInfoList, msg);
}

UbseCtrlQReg1825FeToBusInstanceReqMsg::UbseCtrlQReg1825FeToBusInstanceReqMsg(const UbseMtiBusInst &busInstance,
                                                                             const std::vector<UbseMti1825Vf> &vfList)
    : busInstance_(busInstance),
      vfList_(vfList)
{
}

static void Mti1825VfListToRegInfoList(const std::vector<UbseMti1825Vf> &vfList, std::vector<RegInfo> &regInfoList)
{
    for (const auto &vf : vfList) {
        FeLoc fe;
        fe.slotId = vf.slotId;
        fe.chipId = vf.chipId;
        fe.dieId = vf.dieId;
        fe.pfeId = vf.pfId;
        fe.vfeId = vf.vfId;
        UbController ubController;
        ubController.slotId = vf.affinityUbController.slotId;
        ubController.chipId = vf.affinityUbController.chipId;
        ubController.dieId = vf.affinityUbController.dieId;
        regInfoList.emplace_back(RegInfo{fe, ubController});
    }
}

UbseResult UbseCtrlQReg1825FeToBusInstanceReqMsg::GetReqMsg(CtrlQReqMessage &msg)
{
    SetOpCode(UbseCtrlQRegDevProxy::OP_CODE, msg);
    SetServiceType(DEFAULT_SERVICE_TYPE, msg);
    std::vector<RegInfo> regInfoList;
    Mti1825VfListToRegInfoList(vfList_, regInfoList);
    auto reSize = CalculateTotalSize(regInfoList);
    if (reSize > BASIC_BLOCK_SIZE) {
        ResizeReqMsg((reSize + BASIC_BLOCK_SIZE - 1) / BASIC_BLOCK_SIZE, msg);
    }
    return WriteReqMsg(busInstance_, regInfoList, msg);
}

UbseCtrlQUnReg1825FeFromBusInstanceReqMsg::UbseCtrlQUnReg1825FeFromBusInstanceReqMsg(
    const UbseMtiBusInst &busInstance, const std::vector<UbseMti1825Vf> &vfList)
    : busInstance_(busInstance),
      vfList_(vfList)
{
}

UbseResult UbseCtrlQUnReg1825FeFromBusInstanceReqMsg::GetReqMsg(CtrlQReqMessage &msg)
{
    SetOpCode(UbseCtrlQUnRegDevProxy::OP_CODE, msg);
    SetServiceType(DEFAULT_SERVICE_TYPE, msg);
    std::vector<RegInfo> unRegInfoList;
    Mti1825VfListToRegInfoList(vfList_, unRegInfoList);
    auto reSize = CalculateTotalSize(unRegInfoList);
    if (reSize > BASIC_BLOCK_SIZE) {
        ResizeReqMsg((reSize + BASIC_BLOCK_SIZE - 1) / BASIC_BLOCK_SIZE, msg);
    }
    return WriteReqMsg(busInstance_, unRegInfoList, msg);
}

bool UbseCtrlQRegDevProxy::CheckReqValidation(const CtrlQReqMessage &msg)
{
    return !msg.blocks.empty() && msg.blocks.front().head.opCode == UbseCtrlQRegDevProxy::OP_CODE;
}

UbseResult UbseCtrlQRegDevProxy::ConvertRespMsgToUserData(const ICtrlQReqMsg &reqMsg, const CtrlQRespMessage &msg)
{
    // bbNum 为0时，不检查bbNum
    if (!CheckRespValidation(msg, 0, UbseCtrlQRegDevProxy::OP_CODE)) {
        return UBSE_ERROR;
    }
    return GetBatchOptRespResult(msg, UbseCtrlQRegDevProxy::OP_CODE, resp_);
}

bool UbseCtrlQUnRegDevProxy::CheckReqValidation(const CtrlQReqMessage &msg)
{
    return !msg.blocks.empty() && msg.blocks.front().head.opCode == UbseCtrlQUnRegDevProxy::OP_CODE;
}

UbseResult UbseCtrlQUnRegDevProxy::ConvertRespMsgToUserData(const ICtrlQReqMsg &reqMsg, const CtrlQRespMessage &msg)
{
    // bbNum 为0时，不检查bbNum
    if (!CheckRespValidation(msg, 0, UbseCtrlQUnRegDevProxy::OP_CODE)) {
        return UBSE_ERROR;
    }
    return GetBatchOptRespResult(msg, UbseCtrlQUnRegDevProxy::OP_CODE, resp_);
}
} // namespace ubse::mti::ctrl_q