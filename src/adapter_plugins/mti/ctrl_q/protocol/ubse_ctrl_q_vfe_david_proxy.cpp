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
#include "ubse_ctrl_q_vfe_david_proxy.h"
#include "../ubse_ctrl_q_message.h"
#include "../ubse_ctrl_q_msg_helper.h"
#include "ubse_error.h"
#include "ubse_logger.h"
namespace ubse::mti::ctrl_q {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

struct CtrlQBindVfeDavidReqMsg {
    FixedHead head;
    uint16_t upi;
} __attribute__((packed));
static_assert(sizeof(CtrlQBindVfeDavidReqMsg) <= BASIC_BLOCK_SIZE,
              "The size of CtrlQBindVfeDavidReqMsg is larger than 32");

struct RespReader {
    FixedHead head;
} __attribute__((packed));

struct BindInfo {
    FeLoc fe;
    DavidLoc david;
} __attribute__((packed));

static uint32_t CalculateTotalSize(const std::vector<UbseMtiIdevVfeDavidPair> &vfeDavidList)
{
    return sizeof(CtrlQBindVfeDavidReqMsg) + static_cast<uint32_t>(vfeDavidList.size()) * sizeof(BindInfo);
}

static void SetUpi(uint16_t upi, CtrlQReqMessage &msg)
{
    auto &ref = *reinterpret_cast<CtrlQBindVfeDavidReqMsg *>(&msg.blocks.front());
    ref.upi = upi;
}

static BindInfo VfeDavidPairToBindInfo(const UbseMtiIdevVfeDavidPair &pair)
{
    BindInfo info;
    info.fe.slotId = pair.first.ubController.slotId;
    info.fe.chipId = pair.first.ubController.chipId;
    info.fe.dieId = pair.first.ubController.dieId;
    info.fe.pfeId = pair.first.pfeId;
    info.fe.vfeId = pair.first.vfeId;
    info.david.slotId = pair.second.slotId;
    info.david.chipId = pair.second.chipId;
    info.david.channelId = pair.second.channelId;
    return info;
}

UbseResult WriteReqMsg(CtrlQReqMessage &msg, const std::vector<UbseMtiIdevVfeDavidPair> &vfeDavidList)
{
    auto blockStartPtr = reinterpret_cast<uint8_t *>(msg.blocks.data());
    auto start = blockStartPtr + sizeof(CtrlQBindVfeDavidReqMsg);
    auto end = blockStartPtr + msg.blocks.size() * BASIC_BLOCK_SIZE;
    UbseCtrlQMsgWriteHelper writeHelper(start, end);
    try {
        writeHelper.Write<uint8_t>(vfeDavidList.size());
        for (const auto &vfeDavid : vfeDavidList) {
            writeHelper.Write<BindInfo>(VfeDavidPairToBindInfo(vfeDavid));
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Write vfe david req failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseCtrlQBindVfeDavidReqMsg::UbseCtrlQBindVfeDavidReqMsg(uint16_t upi,
                                                         const std::vector<UbseMtiIdevVfeDavidPair> &vfeDavidList)
    : upi_(upi),
      vfeDavidList_(vfeDavidList)
{
}

UbseResult UbseCtrlQBindVfeDavidReqMsg::GetReqMsg(CtrlQReqMessage &msg)
{
    SetOpCode(UbseCtrlQBindVfeDavidProxy::OP_CODE, msg);
    SetUpi(upi_, msg);
    SetServiceType(DEFAULT_SERVICE_TYPE, msg);
    auto reSize = CalculateTotalSize(vfeDavidList_);
    if (reSize > BASIC_BLOCK_SIZE) {
        ResizeReqMsg((reSize + BASIC_BLOCK_SIZE - 1) / BASIC_BLOCK_SIZE, msg);
    }
    return WriteReqMsg(msg, vfeDavidList_);
}

UbseCtrlQUnBindVfeDavidReqMsg::UbseCtrlQUnBindVfeDavidReqMsg(uint16_t upi,
                                                             const std::vector<UbseMtiIdevVfeDavidPair> &vfeDavidList)
    : UbseCtrlQBindVfeDavidReqMsg(upi, vfeDavidList)
{
}

UbseResult UbseCtrlQUnBindVfeDavidReqMsg::GetReqMsg(CtrlQReqMessage &msg)
{
    SetOpCode(UbseCtrlQUnBindVfeDavidProxy::OP_CODE, msg);
    SetUpi(upi_, msg);
    SetServiceType(DEFAULT_SERVICE_TYPE, msg);
    auto reSize = CalculateTotalSize(vfeDavidList_);
    if (reSize > BASIC_BLOCK_SIZE) {
        ResizeReqMsg((reSize + BASIC_BLOCK_SIZE - 1) / BASIC_BLOCK_SIZE, msg);
    }
    return WriteReqMsg(msg, vfeDavidList_);
}

bool UbseCtrlQBindVfeDavidProxy::CheckReqValidation(const CtrlQReqMessage &msg)
{
    return !msg.blocks.empty() && msg.blocks.front().head.opCode == UbseCtrlQBindVfeDavidProxy::OP_CODE;
}

UbseResult UbseCtrlQBindVfeDavidProxy::ConvertRespMsgToUserData(const ICtrlQReqMsg &reqMsg, const CtrlQRespMessage &msg)
{
    // bbNum 为0时，不检查bbNum
    if (!CheckRespValidation(msg, 0, UbseCtrlQBindVfeDavidProxy::OP_CODE)) {
        return UBSE_ERROR;
    }
    return GetBatchOptRespResult(msg, UbseCtrlQBindVfeDavidProxy::OP_CODE, resp_);
}

bool UbseCtrlQUnBindVfeDavidProxy::CheckReqValidation(const CtrlQReqMessage &msg)
{
    return !msg.blocks.empty() && msg.blocks.front().head.opCode == UbseCtrlQUnBindVfeDavidProxy::OP_CODE;
}

UbseResult UbseCtrlQUnBindVfeDavidProxy::ConvertRespMsgToUserData(const ICtrlQReqMsg &reqMsg,
                                                                  const CtrlQRespMessage &msg)
{
    // bbNum 为0时，不检查bbNum
    if (!CheckRespValidation(msg, 0, UbseCtrlQUnBindVfeDavidProxy::OP_CODE)) {
        return UBSE_ERROR;
    }
    return GetBatchOptRespResult(msg, UbseCtrlQUnBindVfeDavidProxy::OP_CODE, resp_);
}
} // namespace ubse::mti::ctrl_q