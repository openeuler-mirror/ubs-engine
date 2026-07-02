/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of the Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_SSU_STATUS_REQ_MSG_H
#define UBSE_SSU_STATUS_REQ_MSG_H

#include "ubse_com.h"
#include "ubse_ssu_service.h"

namespace ubse::ssu::message {

using namespace ubse::plugin::service::ssu;

struct UbseSsuStatusUpdateReq {
    std::string requestName;
    UbseSsuNsState state{UbseSsuNsState::IDLE};
};

struct UbseSsuStatusUpdateRsp {
    uint32_t errorCode{0};
};

class UbseSsuStatusReqMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuStatusReqMsg() = default;

    void SetStatusUpdateReq(const UbseSsuStatusUpdateReq &req);
    const UbseSsuStatusUpdateReq &GetStatusUpdateReq() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuStatusUpdateReq req_{};
};

class UbseSsuStatusRspMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuStatusRspMsg() = default;

    void SetStatusUpdateRsp(const UbseSsuStatusUpdateRsp &rsp);
    const UbseSsuStatusUpdateRsp &GetStatusUpdateRsp() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuStatusUpdateRsp rsp_{};
};

} // namespace ubse::ssu::message

#endif // UBSE_SSU_STATUS_REQ_MSG_H
