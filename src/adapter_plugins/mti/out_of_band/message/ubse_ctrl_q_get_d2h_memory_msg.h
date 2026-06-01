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

#ifndef UBSE_CTRL_Q_GET_UBA_TID_SIZE_MSG_H
#define UBSE_CTRL_Q_GET_UBA_TID_SIZE_MSG_H
#include "ubse_ictrl_q_req_msg.h"
#include "ubse_ictrl_q_resp_msg.h"
#include "adapter_plugins/mti/ubse_mti_bus_instance.h"
namespace ubse::mti::ctrl_q {
using namespace mti::bus_instance;
class UbseCtrlQGetD2hMemoryReqMsg : public ICtrlQReqMsg {
public:
    explicit UbseCtrlQGetD2hMemoryReqMsg(const UbseMtiBusInst& busInstance);

    UbseResult EncodeReqMsg() override;

private:
    UbseMtiBusInst busInstance_;
};

class UbseCtrlQGetD2hMemoryRespMsg : public ICtrlQRespMsg {
public:
    UbseCtrlQGetD2hMemoryRespMsg() = default;

    UbseResult DecodeRespMsg(const CtrlQRespMessage& msg) override;

    uint64_t GetUba() const;

    uint64_t GetTid() const;

    uint64_t GetSize() const;

private:
    uint64_t uba_;
    uint64_t tid_;
    uint64_t size_;
};

} // namespace ubse::mti::ctrl_q
#endif // UBSE_CTRL_Q_GET_UBA_TID_SIZE_MSG_H
