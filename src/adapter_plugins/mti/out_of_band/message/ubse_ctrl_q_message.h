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

#ifndef UBSE_CTRL_Q_MESSAGE_H
#define UBSE_CTRL_Q_MESSAGE_H
#include <cstdint>
#include <vector>
namespace ubse::mti::ctrl_q {
constexpr uint8_t BASIC_BLOCK_SIZE = 32;
constexpr uint8_t MAX_BASIC_BLOCK_NUM = 32;
constexpr uint8_t DEFAULT_SERVICE_TYPE = 0x1;
struct BasicBlock {
    uint8_t data[BASIC_BLOCK_SIZE];
} __attribute__((packed));

struct FixedHead {
    uint8_t version;
    uint8_t serviceType;
    uint8_t bbNum;
    uint8_t opCode;
    uint8_t ret;
    uint16_t seq;
    uint8_t resv;
} __attribute__((packed));

union Eid {
    struct {
        uint32_t eid : 20;
        uint32_t resv : 12;
    } validInfo;
    uint8_t content[16];
} __attribute__((packed));

struct Guid {
    uint8_t content[16];
} __attribute__((packed));

struct FeLoc {
    uint8_t slotId;
    uint8_t chipId;
    uint8_t dieId;
    uint8_t pfeId;
    uint8_t vfeId;
} __attribute__((packed));

struct FeLoc1825 {
    uint8_t slotId;
    uint8_t chipId;
    uint8_t dieId;
    uint16_t feId;
} __attribute__((packed));

struct UbController {
    uint8_t slotId;
    uint8_t chipId;
    uint8_t dieId;
} __attribute__((packed));

struct DavidLoc {
    uint8_t slotId;
    uint8_t chipId;
    uint8_t channelId;
} __attribute__((packed));

struct CtrlQBasicBlock {
    FixedHead head;
    uint8_t cmdData[BASIC_BLOCK_SIZE - sizeof(FixedHead)];
} __attribute__((packed));

static_assert(sizeof(CtrlQBasicBlock) == BASIC_BLOCK_SIZE, "The size of CtrlQBasicBlock is not 32");

struct CtrlQReqMessage {
    std::vector<CtrlQBasicBlock> blocks;
    CtrlQReqMessage()
    {
        blocks.resize(1); // default size 1
    }

    explicit CtrlQReqMessage(uint8_t bbNum)
    {
        blocks.resize(bbNum);
    }
};

struct CtrlQRespMessage {
    CtrlQBasicBlock* blocks{nullptr};
    uint32_t blockNums{0};
};
} // namespace ubse::mti::ctrl_q
#endif // UBSE_CTRL_Q_MESSAGE_H
