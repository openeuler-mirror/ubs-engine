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

#ifndef UBSE_SSU_MSG_ALLOC_H
#define UBSE_SSU_MSG_ALLOC_H

#include <string>
#include "ubse_com.h"
#include "ubse_ssu_service.h"

namespace ubse::ssu::message {

using namespace ubse::plugin::service::ssu;

struct UbseSsuAllocReq {
    std::string requestId;
    std::string requestNodeId;
    UbseSsuAllocIdentityInfo identityInfo;
    UbseSsuAllocSpaceReq allocReq{};
};

struct UbseSsuAllocResp {
    std::string requestId;
    uint32_t errorCode{0};
    UbseSsuNsState state{UbseSsuNsState::IDLE};
    UbseSsuAllocResult allocResult;
};

class UbseSsuAllocReqMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuAllocReqMsg() = default;

    UbseSsuAllocReqMsg(const std::string &requestId, const std::string &requestNodeId,
                       const UbseSsuAllocIdentityInfo &identity, const UbseSsuAllocSpaceReq &req);

    const UbseSsuAllocReq &GetAllocRequest() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuAllocReq req_{};
};

class UbseSsuAllocRespMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuAllocRespMsg() = default;

    explicit UbseSsuAllocRespMsg(const UbseSsuAllocResp &resp);
    const UbseSsuAllocResp &GetSsuAllocResp() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuAllocResp resp_{};
};

} // namespace ubse::ssu::message

#endif // UBSE_SSU_MSG_ALLOC_H
