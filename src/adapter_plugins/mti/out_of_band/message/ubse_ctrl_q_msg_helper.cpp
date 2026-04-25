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
#include "ubse_ctrl_q_msg_helper.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_logger.h"

namespace ubse::mti::ctrl_q {
using namespace ubse::common::def;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

struct RespReader {
    FixedHead head;
} __attribute__((packed));

bool CheckRespValidation(const CtrlQRespMessage &msg, uint8_t bbNum, uint8_t opCode)
{
    if (msg.blocks == nullptr) {
        UBSE_LOG_ERROR << "Resp blocks is nullptr, opCode: " << opCode;
        return false;
    }
    auto &reader = *reinterpret_cast<const RespReader *>(msg.blocks);
    // bbNum 为0时，不检查bbNum
    if (reader.head.serviceType != DEFAULT_SERVICE_TYPE || reader.head.opCode != opCode ||
        (bbNum != 0 && reader.head.bbNum != bbNum)) {
        UBSE_LOG_ERROR << "Check resp failed, serviceType: " << reader.head.serviceType
                       << ", bbNum: " << reader.head.bbNum << ", opCode: " << reader.head.opCode;
        return false;
    }
    return true;
}

UbseResult GetBatchOptRespResult(const CtrlQRespMessage &msg, uint8_t opCode, std::vector<bool> &resList)
{
    auto pos = reinterpret_cast<uint8_t *>(msg.blocks) + sizeof(RespReader);
    auto end = reinterpret_cast<uint8_t *>(msg.blocks) + sizeof(BasicBlock) * msg.blockNums;

    UbseCtrlQMsgReadHelper readHelper(pos, end);
    try {
        auto cnt = readHelper.Read<uint8_t>();
        for (uint8_t i = 0; i < cnt; i++) {
            uint8_t res = readHelper.Read<uint8_t>();
            if (res != UBSE_OK) {
                UBSE_LOG_ERROR << "Opt failed, resp idx: " << i << ", res: " << res << ", opCode: " << opCode;
                resList.emplace_back(false);
            } else {
                resList.emplace_back(true);
            }
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Read opt resp failed, opCode: " << opCode;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::mti::ctrl_q