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
static const uint8_t REG_IDEV_OP_CODE = 0x7;
static const uint8_t UNREG_IDEV_OP_CODE = 0x8;
static const uint8_t REG_DEV_OP_CODE = 0xE;
static const uint8_t UNREG_DEV_OP_CODE = 0xF;

struct UbseCtrlQRegIdevReqMsg {
    FixedHead head;
    struct {
        uint8_t content[3];
    } __attribute__((packed)) eid;
    uint8_t feCount;
} __attribute__((packed));

struct UbseCtrlQRegDevReqMsg {
    FixedHead head;
    struct {
        uint8_t content[3];
    } __attribute__((packed)) eid;
    uint8_t slotId;
    uint8_t feCount;
} __attribute__((packed));

struct IdevRegInfo {
    FeLoc fe;
    UbController ubController;
} __attribute__((packed));

struct DevFeLoc {
    uint8_t chipId;
    uint8_t dieId;
    uint16_t feId;
} __attribute__((packed));

struct DevRegInfo {
    DevFeLoc fe;
} __attribute__((packed));

struct RespReader {
    FixedHead head;
} __attribute__((packed));

static uint32_t CalculateIdevTotalSize(uint32_t regInfoNum)
{
    uint32_t reSize = sizeof(UbseCtrlQRegIdevReqMsg) + regInfoNum * sizeof(IdevRegInfo);
    return (reSize + BASIC_BLOCK_SIZE - 1) / BASIC_BLOCK_SIZE;
}

static uint32_t CalculateDevTotalSize(uint32_t regInfoNum)
{
    uint32_t reSize = sizeof(UbseCtrlQRegDevReqMsg) + regInfoNum * sizeof(DevRegInfo);
    return (reSize + BASIC_BLOCK_SIZE - 1) / BASIC_BLOCK_SIZE;
}

UbseResult SetIdevBusInstance(const UbseMtiBusInst &busInstance, CtrlQReqMessage &msg)
{
    auto &ref = *reinterpret_cast<UbseCtrlQRegIdevReqMsg *>(&msg.blocks.front());
    auto ret = memcpy_s(&ref.eid, sizeof(ref.eid), busInstance.eid.data(), sizeof(ref.eid));
    if (ret != EOK) {
        UBSE_LOG_ERROR << "Mem copy businstance eid failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult SetDevBusInstance(const UbseMtiBusInst &busInstance, CtrlQReqMessage &msg)
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
      ICtrlQReqMsg(REG_IDEV_OP_CODE, CalculateIdevTotalSize(vfeList_.size()))
{
}

static UbseResult WriteIdevReqMsg(const UbseMtiBusInst &busInstance, const std::vector<IdevRegInfo> &regInfoList,
                                  CtrlQReqMessage &msg)
{
    if (SetIdevBusInstance(busInstance, msg) != UBSE_OK) {
        UBSE_LOG_ERROR << "Set businstance failed";
        return UBSE_ERROR;
    }
    auto blockStartPtr = reinterpret_cast<uint8_t *>(msg.blocks.data());
    auto start = blockStartPtr + sizeof(UbseCtrlQRegIdevReqMsg);
    auto end = blockStartPtr + msg.blocks.size() * BASIC_BLOCK_SIZE;
    UbseCtrlQMsgWriteHelper writeHelper(start, end);
    try {
        writeHelper.Write<uint8_t>(regInfoList.size());
        for (const auto &regInfo : regInfoList) {
            writeHelper.Write<IdevRegInfo>(regInfo);
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Write vfe david req failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

static UbseResult WriteDevReqMsg(const UbseMtiBusInst &busInstance, uint8_t slotId,
                                 const std::vector<DevRegInfo> &regInfoList, CtrlQReqMessage &msg)
{
    if (SetDevBusInstance(busInstance, msg) != UBSE_OK) {
        UBSE_LOG_ERROR << "Set businstance failed";
        return UBSE_ERROR;
    }
    auto blockStartPtr = reinterpret_cast<uint8_t *>(msg.blocks.data());
    auto start = blockStartPtr + sizeof(UbseCtrlQRegIdevReqMsg);
    auto end = blockStartPtr + msg.blocks.size() * BASIC_BLOCK_SIZE;
    UbseCtrlQMsgWriteHelper writeHelper(start, end);
    try {
        writeHelper.Write<uint8_t>(slotId);
        writeHelper.Write<uint8_t>(regInfoList.size() & 0x7F);
        for (const auto &regInfo : regInfoList) {
            writeHelper.Write<DevRegInfo>(regInfo);
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Write 1825 req failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

static void IdevVfeListToRegInfoList(const std::vector<UbseMtiIdevVfe> &vfeList, std::vector<IdevRegInfo> &regInfoList)
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
        regInfoList.emplace_back(IdevRegInfo{fe, ubController});
    }
}

UbseResult UbseCtrlQRegDavidFeToBusInstanceReqMsg::EncodeReqMsg()
{
    std::vector<IdevRegInfo> regInfoList;
    IdevVfeListToRegInfoList(vfeList_, regInfoList);
    return WriteIdevReqMsg(busInstance_, regInfoList, reqMsg_);
}

UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg::UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg(
    const UbseMtiBusInst &busInstance, const std::vector<UbseMtiIdevVfe> &vfeList)
    : busInstance_(busInstance),
      vfeList_(vfeList),
      ICtrlQReqMsg(UNREG_IDEV_OP_CODE, CalculateIdevTotalSize(vfeList_.size()))
{
}

UbseResult UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg::EncodeReqMsg()
{
    std::vector<IdevRegInfo> unRegInfoList;
    IdevVfeListToRegInfoList(vfeList_, unRegInfoList);
    return WriteIdevReqMsg(busInstance_, unRegInfoList, reqMsg_);
}

UbseCtrlQReg1825FeToBusInstanceReqMsg::UbseCtrlQReg1825FeToBusInstanceReqMsg(const UbseMtiBusInst &busInstance,
                                                                             const std::vector<UbseMti1825Vf> &vfList)
    : busInstance_(busInstance),
      vfList_(vfList),
      ICtrlQReqMsg(REG_DEV_OP_CODE, CalculateDevTotalSize(vfList_.size()))
{
}

static void Mti1825VfListToRegInfoList(const std::vector<UbseMti1825Vf> &vfList, std::vector<DevRegInfo> &regInfoList)
{
    for (const auto &vf : vfList) {
        DevFeLoc fe;
        fe.chipId = vf.chipId;
        fe.dieId = vf.dieId;
        if (vf.vfId == 0xff) {
            fe.feId = vf.pfId;
        } else {
            fe.feId = vf.vfId;
        }
        regInfoList.emplace_back(DevRegInfo{fe});
    }
}

UbseResult UbseCtrlQReg1825FeToBusInstanceReqMsg::EncodeReqMsg()
{
    std::vector<DevRegInfo> regInfoList;
    if (vfList_.empty()) {
        UBSE_LOG_ERROR << "Encode request message failed, the vfList is empty";
        return UBSE_ERROR;
    }
    uint8_t slotId = vfList_[0].slotId;
    for (size_t i = 1; i < vfList_.size(); ++i) {
        if (vfList_[i].slotId != slotId) {
            UBSE_LOG_ERROR << "Inconsistent slotId detected";
            return UBSE_ERROR;
        }
    }
    Mti1825VfListToRegInfoList(vfList_, regInfoList);
    return WriteDevReqMsg(busInstance_, slotId, regInfoList, reqMsg_);
}

UbseCtrlQUnReg1825FeFromBusInstanceReqMsg::UbseCtrlQUnReg1825FeFromBusInstanceReqMsg(
    const UbseMtiBusInst &busInstance, const std::vector<UbseMti1825Vf> &vfList)
    : busInstance_(busInstance),
      vfList_(vfList),
      ICtrlQReqMsg(UNREG_DEV_OP_CODE, CalculateDevTotalSize(vfList_.size()))
{
}

UbseResult UbseCtrlQUnReg1825FeFromBusInstanceReqMsg::EncodeReqMsg()
{
    std::vector<DevRegInfo> unRegInfoList;
    if (vfList_.empty()) {
        UBSE_LOG_ERROR << "Encode request message failed, the vfList is empty";
        return UBSE_ERROR;
    }
    uint8_t slotId = vfList_[0].slotId;
    for (size_t i = 1; i < vfList_.size(); ++i) {
        if (vfList_[i].slotId != slotId) {
            UBSE_LOG_ERROR << "Inconsistent slotId detected";
            return UBSE_ERROR;
        }
    }
    Mti1825VfListToRegInfoList(vfList_, unRegInfoList);
    return WriteDevReqMsg(busInstance_, slotId, unRegInfoList, reqMsg_);
}

UbseResult UbseCtrlQRegDavidFeToBusInstanceRespMsg::DecodeRespMsg(const CtrlQRespMessage &msg)
{
    // bbNum 为0时，不检查bbNum
    if (!CheckRespValidation(msg, 0, REG_IDEV_OP_CODE)) {
        return UBSE_ERROR;
    }
    return GetBatchOptRespResult(msg, REG_IDEV_OP_CODE, retList_);
}

const std::vector<bool> &UbseCtrlQRegDavidFeToBusInstanceRespMsg::GetRetList() const
{
    return retList_;
}

UbseResult UbseCtrlQReg1825FeToBusInstanceRespMsg::DecodeRespMsg(const CtrlQRespMessage &msg)
{
    // bbNum 为0时，不检查bbNum
    if (!CheckRespValidation(msg, 0, REG_DEV_OP_CODE)) {
        return UBSE_ERROR;
    }
    return GetBatchOptRespResult(msg, REG_DEV_OP_CODE, retList_);
}

const std::vector<bool> &UbseCtrlQReg1825FeToBusInstanceRespMsg::GetRetList() const
{
    return retList_;
}

UbseResult UbseCtrlQUnRegDavidFeFromBusInstanceRespMsg::DecodeRespMsg(const CtrlQRespMessage &msg)
{
    // bbNum 为0时，不检查bbNum
    if (!CheckRespValidation(msg, 0, UNREG_IDEV_OP_CODE)) {
        return UBSE_ERROR;
    }
    return GetBatchOptRespResult(msg, UNREG_IDEV_OP_CODE, retList_);
}

const std::vector<bool> &UbseCtrlQUnRegDavidFeFromBusInstanceRespMsg::GetRetList() const
{
    return retList_;
}

UbseResult UbseCtrlQUnReg1825FeFromBusInstanceRespMsg::DecodeRespMsg(const CtrlQRespMessage &msg)
{
    // bbNum 为0时，不检查bbNum
    if (!CheckRespValidation(msg, 0, UNREG_DEV_OP_CODE)) {
        return UBSE_ERROR;
    }
    return GetBatchOptRespResult(msg, UNREG_DEV_OP_CODE, retList_);
}

const std::vector<bool> &UbseCtrlQUnReg1825FeFromBusInstanceRespMsg::GetRetList() const
{
    return retList_;
}

} // namespace ubse::mti::ctrl_q