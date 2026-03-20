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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstdlib>
#include "framework/misc/ubse_sequence_counter.h"
#include "securec.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_pointer_process.h"

namespace ubse::mti::ctrl_q {
using namespace ubse::common::def;
using namespace ubse::log;
using namespace ubse::framework::misc;
UBSE_DEFINE_THIS_MODULE("ubse");

struct BandBridgeMbuf {
    int sendBufSize{0};
    int recvBufSize{0};
    void *sendBuf{nullptr};
    void *recvBuf{nullptr};
};

const std::string BANDBRIDGE_DEV_NAME = "/dev/bandbridge";
const size_t RECV_BUF_SIZE = 1024;
constexpr char BANDBRIDGE_IOCTL_BASE = 'x';
// SEQ_MASK: 0x8000，用于提取最高位（验证标志位）
// 计算方式：1 << (16 - 1) = 1 << 15 = 0x8000
constexpr uint16_t SEQ_MASK = static_cast<uint16_t>(1) << (sizeof(uint16_t) * 8 - 1);
#define BANDBRIDGE_SEND_REQUEST _IOWR(BANDBRIDGE_IOCTL_BASE, 0, struct BandBridgeMbuf)

static UbseResult SendCtrlQMsg(BandBridgeMbuf &msgBuf)
{
    int fd = open(BANDBRIDGE_DEV_NAME.c_str(), O_RDWR);
    if (fd < 0) {
        UBSE_LOG_ERROR << "Open bandbridge dev failed";
        return UBSE_ERROR;
    }
    auto guard = SafeMakeUniqueWithFreeFunc<int>(
        [](int *fd) {
            close(*fd);
            delete fd;
        },
        fd);
    auto ret = ioctl(fd, BANDBRIDGE_SEND_REQUEST, &msgBuf);
    if (ret != 0) {
        UBSE_LOG_ERROR << "Call ioctl failed, ret code: " << ret;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

bool CheckSeq(uint16_t sendSeq, uint16_t recvSeq)
{
    // 检查接收序列号的最高位是否为1（验证标志位）
    if (SEQ_MASK & recvSeq) {
        // 清除最高位，获取实际的序列号值（低15位）
        recvSeq &= ~SEQ_MASK;
    } else {
        // 最高位为0，表示接收序列号未经验证或无效
        return false;
    }

    // 比较发送序列号和接收序列号的实际值是否相等
    return sendSeq == recvSeq;
}

UbseResult SendMsg(const CtrlQReqMessage &msg, CtrlQRespMessage &respMsg)
{
    auto buf = SafeMakeUniqueWithFreeFunc<BandBridgeMbuf>([](BandBridgeMbuf *buf) {
        if (buf->recvBuf != nullptr) {
            free(buf->recvBuf);
            buf->recvBuf = nullptr;
            buf->recvBufSize = 0;
        }
        delete buf;
    });
    buf->sendBuf = const_cast<void *>(static_cast<const void *>(msg.blocks.data()));
    buf->sendBufSize = static_cast<int>(msg.blocks.size() * sizeof(CtrlQBasicBlock));
    buf->recvBuf = malloc(RECV_BUF_SIZE);
    if (buf->recvBuf == nullptr) {
        UBSE_LOG_ERROR << "Malloc recvBuf failed";
        return UBSE_ERROR;
    }
    buf->recvBufSize = static_cast<int>(RECV_BUF_SIZE);
    static UbseSequenceCounter<uint16_t> seqCounter(0, std::numeric_limits<uint16_t>::max() >> 1);
    auto sendSeq = seqCounter.GetNextSafe();
    reinterpret_cast<FixedHead *>(buf->sendBuf)->seq = sendSeq;

    if (SendCtrlQMsg(*buf) != UBSE_OK) {
        return UBSE_ERROR;
    }
    auto ret = reinterpret_cast<FixedHead *>(buf->recvBuf)->ret;
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Recv resp failed, seq: " << sendSeq << ", ret: " << ret;
        return UBSE_ERROR;
    }
    auto blockNums = reinterpret_cast<FixedHead *>(buf->recvBuf)->bbNum;
    // 检查接收的基本块数量是否有效，如果为0或超过接收缓冲区大小，返回错误
    if (blockNums == 0 || blockNums * BASIC_BLOCK_SIZE > RECV_BUF_SIZE) {
        UBSE_LOG_ERROR << "Block nums is invalid, blockNums: " << blockNums;
        return UBSE_ERROR;
    }
    auto recvSeq = reinterpret_cast<FixedHead *>(buf->recvBuf)->seq;
    if (!CheckSeq(sendSeq, recvSeq)) {
        UBSE_LOG_ERROR << "Seq is invalid, sendSeq: " << sendSeq << ", recvSeq: " << recvSeq;
        return UBSE_ERROR;
    }
    respMsg.blockNums = blockNums;
    auto msgSize = blockNums * BASIC_BLOCK_SIZE;
    respMsg.blocks = new (std::nothrow) CtrlQBasicBlock[blockNums];
    if (respMsg.blocks == nullptr) {
        UBSE_LOG_ERROR << "Failed to allocate memory for response blocks, size: " << msgSize;
        return UBSE_ERROR;
    }
    auto copyRet = memcpy_s(respMsg.blocks, msgSize, static_cast<CtrlQBasicBlock *>(buf->recvBuf), msgSize);
    if (copyRet != EOK) {
        UBSE_LOG_ERROR << "Memcpy resp failed, ret: " << copyRet;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

struct RespReader {
    FixedHead head;
} __attribute__((packed));

bool CheckRespValidation(const CtrlQRespMessage &msg, uint8_t bbNum, uint8_t opCode)
{
    auto &reader = *reinterpret_cast<const RespReader *>(msg.blocks);
    // bbNum 为0时，不检查bbNum
    if (reader.head.serviceType != DEFAULT_SERVICE_TYPE || reader.head.opCode != opCode ||
        (reader.head.bbNum != bbNum && reader.head.bbNum != 0)) {
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