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

#include "ubse_ssu_perm_msg.h"

#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_serial_util.h"

namespace ubse::ssu::message {

using namespace ubse::log;
using namespace ubse::serial;

UBSE_DEFINE_THIS_MODULE("ubse");

UbseSsuPermReqMsg::UbseSsuPermReqMsg(const std::string &requestId, const std::string &requestNodeId,
                                     const std::string &name, const std::string &nqn,
                                     const UbseSsuAllocIdentityInfo &identity)
    : req_{requestId, requestNodeId, name, nqn, identity}
{
}

const UbseSsuPermReq &UbseSsuPermReqMsg::GetSsuPermRequest() const
{
    return req_;
}

UbseSsuPermRespMsg::UbseSsuPermRespMsg(const UbseSsuPermResp &resp) : resp_(resp) {}

const UbseSsuPermResp &UbseSsuPermRespMsg::GetSsuPermResponse() const
{
    return resp_;
}

uint32_t UbseSsuPermReqMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    UbseSerialization out;
    out << req_.requestId << req_.requestNodeId << req_.name << req_.nqn << req_.identityInfo.uid
        << req_.identityInfo.userName;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU perm req serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuPermReqMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    UbseDeSerialization in(data, size);
    in >> req_.requestId >> req_.requestNodeId >> req_.name >> req_.nqn >> req_.identityInfo.uid >>
        req_.identityInfo.userName;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "SSU perm req deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseSsuPermRespMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    UbseSerialization out;
    out << resp_.requestId << resp_.errorCode;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU perm resp serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuPermRespMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    UbseDeSerialization in(data, size);
    in >> resp_.requestId >> resp_.errorCode;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "SSU perm resp deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

} // namespace ubse::ssu::message
