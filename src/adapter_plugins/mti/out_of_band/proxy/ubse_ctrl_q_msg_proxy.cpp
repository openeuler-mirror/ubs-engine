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
#include "ubse_ctrl_q_msg_proxy.h"
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

CtrlQMsgProxy &CtrlQMsgProxy::GetInstance()
{
    static CtrlQMsgProxy instance;
    return instance;
}

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

UbseResult SendMsg(BandBridgeMbuf &buf, CtrlQRespMessage &respMsg)
{
    static UbseSequenceCounter<uint16_t> seqCounter(0, std::numeric_limits<uint16_t>::max() >> 1);
    auto sendSeq = seqCounter.GetNextSafe();
    reinterpret_cast<FixedHead *>(buf.sendBuf)->seq = sendSeq;

    if (SendCtrlQMsg(buf) != UBSE_OK) {
        return UBSE_ERROR;
    }
    auto ret = reinterpret_cast<FixedHead *>(buf.recvBuf)->ret;
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Recv resp failed, seq: " << sendSeq << ", ret: " << ret;
        return UBSE_ERROR;
    }
    auto blockNums = reinterpret_cast<FixedHead *>(buf.recvBuf)->bbNum;
    // 检查接收的基本块数量是否有效，如果为0或超过接收缓冲区大小，返回错误
    if (blockNums == 0 || blockNums * BASIC_BLOCK_SIZE > RECV_BUF_SIZE) {
        UBSE_LOG_ERROR << "Block nums is invalid, blockNums: " << blockNums;
        return UBSE_ERROR;
    }
    auto recvSeq = reinterpret_cast<FixedHead *>(buf.recvBuf)->seq;
    if (!CheckSeq(sendSeq, recvSeq)) {
        UBSE_LOG_ERROR << "Seq is invalid, sendSeq: " << sendSeq << ", recvSeq: " << recvSeq;
        return UBSE_ERROR;
    }
    respMsg.blockNums = blockNums;
    auto msgSize = blockNums * BASIC_BLOCK_SIZE;
    respMsg.blocks = reinterpret_cast<CtrlQBasicBlock *>(buf.recvBuf);
    return UBSE_OK;
}

UbseResult CtrlQMsgProxy::SendRequest(ICtrlQReqMsg &reqMsg, ICtrlQRespMsg &respMsg)
{
    auto ret = reqMsg.EncodeReqMsg();
    if (ret != UBSE_OK) {
        return ret;
    }
    auto &msg = reqMsg.GetReqMsg();
    if (msg.blocks.size() > MAX_BASIC_BLOCK_NUM || msg.blocks.empty()) {
        UBSE_LOG_ERROR << "Block nums is invalid, blockNums: " << msg.blocks.size();
        return UBSE_ERROR;
    }
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
    CtrlQRespMessage respMsgBuf;
    auto res = SendMsg(*buf, respMsgBuf);
    if (res != UBSE_OK) {
        return res;
    }
    return respMsg.DecodeRespMsg(respMsgBuf);
}
} // namespace ubse::mti::ctrl_q