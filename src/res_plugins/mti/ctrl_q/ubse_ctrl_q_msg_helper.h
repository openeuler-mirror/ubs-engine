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

#ifndef UBSE_CTRL_Q_MSG_HELPER_H
#define UBSE_CTRL_Q_MSG_HELPER_H
#include <stdexcept>
#include "ubse_common_def.h"
#include "ubse_ctrl_q_message.h"

namespace ubse::mti::ctrl_q {
using namespace ubse::common::def;
class UbseCtrlQMsgWriteHelper {
public:
    UbseCtrlQMsgWriteHelper() = delete;
    UbseCtrlQMsgWriteHelper(uint8_t *s, uint8_t *e) : start_(s), end_(e){};
    UbseCtrlQMsgWriteHelper(const UbseCtrlQMsgWriteHelper &) = delete;
    UbseCtrlQMsgWriteHelper(UbseCtrlQMsgWriteHelper &&) = delete;
    UbseCtrlQMsgWriteHelper &operator=(const UbseCtrlQMsgWriteHelper &) = delete;
    UbseCtrlQMsgWriteHelper &operator=(UbseCtrlQMsgWriteHelper &&) noexcept = delete;

    template <typename T>
    void Write(T val)
    {
        if (start_ + sizeof(T) > end_) {
            throw std::out_of_range("out of range while writing request");
        }
        *reinterpret_cast<T *>(start_) = val;
        start_ += sizeof(T);
    }

private:
    uint8_t *start_;
    uint8_t *end_;
};

class UbseCtrlQMsgReadHelper {
public:
    UbseCtrlQMsgReadHelper() = delete;
    UbseCtrlQMsgReadHelper(uint8_t *s, uint8_t *e) : start_(s), end_(e){};
    UbseCtrlQMsgReadHelper(const UbseCtrlQMsgReadHelper &) = delete;
    UbseCtrlQMsgReadHelper(UbseCtrlQMsgReadHelper &&) = delete;
    UbseCtrlQMsgReadHelper &operator=(const UbseCtrlQMsgReadHelper &) = delete;
    UbseCtrlQMsgReadHelper &operator=(UbseCtrlQMsgReadHelper &&) noexcept = delete;

    template <typename T>
    T Read()
    {
        if (start_ + sizeof(T) > end_) {
            throw std::out_of_range("out of range while parsing response");
        }
        auto p = start_;
        start_ += sizeof(T);
        return *reinterpret_cast<T *>(p);
    }

private:
    uint8_t *start_;
    uint8_t *end_;
};

UbseResult SendMsg(const CtrlQReqMessage &msg, CtrlQRespMessage &respMsg);

UbseResult GetBatchOptRespResult(const CtrlQRespMessage &msg, uint8_t opCode, std::vector<bool> &resList);

bool CheckRespValidation(const CtrlQRespMessage &msg, uint8_t opCode);
} // namespace ubse::mti::ctrl_q
#endif // UBSE_CTRL_Q_MSG_HELPER_H