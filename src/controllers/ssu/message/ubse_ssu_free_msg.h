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

#ifndef UBSE_SSU_FREE_MSG_H
#define UBSE_SSU_FREE_MSG_H

#include <string>
#include "ubse_com.h"

namespace ubse::ssu::message {

using namespace ubse::com;

struct UbseSsuFreeReq {
    std::string requestId;
    std::string requestNodeId;
    std::string name;
};

struct UbseSsuFreeResp {
    std::string requestId;
    uint32_t errorCode{0};
};

class UbseSsuFreeReqMsg : public UbseRpcMessage {
public:
    UbseSsuFreeReqMsg() = default;

    void SetSsuFreeRequest(const std::string &requestId, const std::string &requestNodeId, const std::string &name);
    UbseSsuFreeReq GetSsuFreeRequest() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuFreeReq req_{};
};

class UbseSsuFreeRespMsg : public UbseRpcMessage {
public:
    UbseSsuFreeRespMsg() = default;

    void SetSsuFreeResponse(const UbseSsuFreeResp &resp);
    const UbseSsuFreeResp &GetSsuFreeResponse() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuFreeResp resp_{};
};

} // namespace ubse::ssu::message

#endif // UBSE_SSU_FREE_MSG_H
