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

#include "ubse_ssu_free_msg.h"

#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_serial_util.h"

namespace ubse::ssu::message {

using namespace ubse::log;
using namespace ubse::serial;

UBSE_DEFINE_THIS_MODULE("ubse");

void UbseSsuFreeReqMsg::SetSsuFreeRequest(const std::string &requestId, const std::string &requestNodeId,
                                           const std::string &name, const UbseSsuAllocIdentityInfo &identity)
{
    req_.requestId = requestId;
    req_.requestNodeId = requestNodeId;
    req_.name = name;
    req_.identityInfo = identity;
}

const UbseSsuFreeReq &UbseSsuFreeReqMsg::GetSsuFreeRequest() const
{
    return req_;
}

void UbseSsuFreeRespMsg::SetSsuFreeResponse(const UbseSsuFreeResp &resp)
{
    resp_ = resp;
}

const UbseSsuFreeResp &UbseSsuFreeRespMsg::GetSsuFreeResponse() const
{
    return resp_;
}

uint32_t UbseSsuFreeReqMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    UbseSerialization out;
    out << req_.requestId << req_.requestNodeId << req_.name << req_.identityInfo.uid << req_.identityInfo.userName;
    
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU free req serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuFreeReqMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    UbseDeSerialization in(data, size);
    in >> req_.requestId >> req_.requestNodeId >> req_.name >> req_.identityInfo.uid >> req_.identityInfo.userName;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "SSU free req deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseSsuFreeRespMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    UbseSerialization out;
    out << resp_.requestId << resp_.errorCode;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU free resp serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuFreeRespMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    UbseDeSerialization in(data, size);
    in >> resp_.requestId >> resp_.errorCode;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "SSU free resp deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

} // namespace ubse::ssu::message
