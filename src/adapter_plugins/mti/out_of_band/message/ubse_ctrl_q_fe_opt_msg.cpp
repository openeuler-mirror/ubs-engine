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

#include "ubse_ctrl_q_fe_opt_msg.h"
#include "securec.h"
#include "ubse_ctrl_q_message.h"
#include "ubse_ctrl_q_msg_helper.h"
#include "ubse_error.h"
#include "ubse_logger.h"
namespace ubse::mti::ctrl_q {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");
static const uint8_t REG_DEV_OP_CODE = 0x7;
static const uint8_t UNREG_DEV_OP_CODE = 0x8;

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

static uint32_t CalculateTotalSize(uint32_t regInfoNum)
{
    uint32_t reSize = sizeof(UbseCtrlQRegDevReqMsg) + regInfoNum * sizeof(RegInfo);
    return (reSize + BASIC_BLOCK_SIZE - 1) / BASIC_BLOCK_SIZE;
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
      vfeList_(vfeList),
      ICtrlQReqMsg(REG_DEV_OP_CODE, CalculateTotalSize(vfeList_.size()))
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

UbseResult UbseCtrlQRegDavidFeToBusInstanceReqMsg::EncodeReqMsg()
{
    std::vector<RegInfo> regInfoList;
    IdevVfeListToRegInfoList(vfeList_, regInfoList);
    return WriteReqMsg(busInstance_, regInfoList, reqMsg_);
}

UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg::UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg(
    const UbseMtiBusInst &busInstance, const std::vector<UbseMtiIdevVfe> &vfeList)
    : busInstance_(busInstance),
      vfeList_(vfeList),
      ICtrlQReqMsg(UNREG_DEV_OP_CODE, CalculateTotalSize(vfeList_.size()))
{
}

UbseResult UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg::EncodeReqMsg()
{
    std::vector<RegInfo> unRegInfoList;
    IdevVfeListToRegInfoList(vfeList_, unRegInfoList);
    return WriteReqMsg(busInstance_, unRegInfoList, reqMsg_);
}

UbseCtrlQReg1825FeToBusInstanceReqMsg::UbseCtrlQReg1825FeToBusInstanceReqMsg(const UbseMtiBusInst &busInstance,
                                                                             const std::vector<UbseMti1825Vf> &vfList)
    : busInstance_(busInstance),
      vfList_(vfList),
      ICtrlQReqMsg(REG_DEV_OP_CODE, CalculateTotalSize(vfList_.size()))
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

UbseResult UbseCtrlQReg1825FeToBusInstanceReqMsg::EncodeReqMsg()
{
    std::vector<RegInfo> regInfoList;
    Mti1825VfListToRegInfoList(vfList_, regInfoList);
    return WriteReqMsg(busInstance_, regInfoList, reqMsg_);
}

UbseCtrlQUnReg1825FeFromBusInstanceReqMsg::UbseCtrlQUnReg1825FeFromBusInstanceReqMsg(
    const UbseMtiBusInst &busInstance, const std::vector<UbseMti1825Vf> &vfList)
    : busInstance_(busInstance),
      vfList_(vfList),
      ICtrlQReqMsg(UNREG_DEV_OP_CODE, CalculateTotalSize(vfList_.size()))
{
}

UbseResult UbseCtrlQUnReg1825FeFromBusInstanceReqMsg::EncodeReqMsg()
{
    std::vector<RegInfo> unRegInfoList;
    Mti1825VfListToRegInfoList(vfList_, unRegInfoList);
    return WriteReqMsg(busInstance_, unRegInfoList, reqMsg_);
}

UbseResult UbseCtrlQRegDavidFeToBusInstanceRespMsg::DecodeRespMsg(const CtrlQRespMessage &msg)
{
    // bbNum 为0时，不检查bbNum
    if (!CheckRespValidation(msg, 0, REG_DEV_OP_CODE)) {
        return UBSE_ERROR;
    }
    return GetBatchOptRespResult(msg, REG_DEV_OP_CODE, retList_);
}

const std::vector<bool> &UbseCtrlQRegDavidFeToBusInstanceRespMsg::GetRetList() const
{
    return retList_;
}

UbseResult UbseCtrlQUnRegDavidFeFromBusInstanceRespMsg::DecodeRespMsg(const CtrlQRespMessage &msg)
{
    // bbNum 为0时，不检查bbNum
    if (!CheckRespValidation(msg, 0, UNREG_DEV_OP_CODE)) {
        return UBSE_ERROR;
    }
    return GetBatchOptRespResult(msg, UNREG_DEV_OP_CODE, retList_);
}

const std::vector<bool> &UbseCtrlQUnRegDavidFeFromBusInstanceRespMsg::GetRetList() const
{
    return retList_;
}

} // namespace ubse::mti::ctrl_q