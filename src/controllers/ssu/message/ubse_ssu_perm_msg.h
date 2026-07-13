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

#ifndef UBSE_SSU_PERM_MSG_H
#define UBSE_SSU_PERM_MSG_H

#include <string>
#include "ubse_com.h"
#include "ubse_ssu_service.h"

namespace ubse::ssu::message {

using namespace ubse::plugin::service::ssu;

// SSU访问权限请求（添加/移除共用同一消息结构，通过op code区分语义）
struct UbseSsuPermReq {
    std::string requestId;
    std::string requestNodeId;
    std::string name;
    std::string nqn;
    UbseSsuAllocIdentityInfo identityInfo;
};

struct UbseSsuPermResp {
    std::string requestId;
    uint32_t errorCode{0};
};

class UbseSsuPermReqMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuPermReqMsg() = default;

    UbseSsuPermReqMsg(const std::string &requestId, const std::string &requestNodeId, const std::string &name,
                      const std::string &nqn, const UbseSsuAllocIdentityInfo &identity);
    const UbseSsuPermReq &GetSsuPermRequest() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuPermReq req_{};
};

class UbseSsuPermRespMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuPermRespMsg() = default;

    explicit UbseSsuPermRespMsg(const UbseSsuPermResp &resp);
    const UbseSsuPermResp &GetSsuPermResponse() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuPermResp resp_{};
};

} // namespace ubse::ssu::message

#endif // UBSE_SSU_PERM_MSG_H
