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

#include "ubse_ssu_attach_detach_verify_msg.h"

#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_serial_util.h"

namespace ubse::ssu::message {

using namespace ubse::log;
using namespace ubse::serial;

UBSE_DEFINE_THIS_MODULE("ubse");

UbseSsuAttachDetachVerifyReqMsg::UbseSsuAttachDetachVerifyReqMsg(const std::string &requestId,
                                                                 const std::string &requestNodeId,
                                                                 const std::string &name,
                                                                 const UbseSsuAllocIdentityInfo &identity)
    : req_{requestId, requestNodeId, name, identity}
{
}

const UbseSsuAttachDetachVerifyReq &UbseSsuAttachDetachVerifyReqMsg::GetAttachDetachVerifyReq() const
{
    return req_;
}

const UbseSsuAttachDetachVerifyResp &UbseSsuAttachDetachVerifyRespMsg::GetAttachDetachVerifyResp() const
{
    return resp_;
}

UbseSerialization &operator<<(UbseSerialization &serializer, const UbseSsuNsVerifyInfo &info)
{
    serializer << info.defaultNqn << info.jettyId << info.guid;
    return serializer;
}

UbseDeSerialization &operator>>(UbseDeSerialization &deserializer, UbseSsuNsVerifyInfo &info)
{
    deserializer >> info.defaultNqn >> info.jettyId >> info.guid;
    return deserializer;
}

uint32_t UbseSsuAttachDetachVerifyReqMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    UbseSerialization out;
    out << req_.requestId << req_.requestNodeId << req_.name;
    out << req_.identityInfo.uid << req_.identityInfo.userName;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU attach verify req serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuAttachDetachVerifyReqMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    UbseDeSerialization in(data, size);
    in >> req_.requestId >> req_.requestNodeId >> req_.name;
    in >> req_.identityInfo.uid >> req_.identityInfo.userName;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "SSU attach verify req deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseSsuAttachDetachVerifyRespMsg::UbseSsuAttachDetachVerifyRespMsg(const UbseSsuAttachDetachVerifyResp &resp)
    : resp_(resp)
{
}

uint32_t UbseSsuAttachDetachVerifyRespMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    UbseSerialization out;
    out << resp_.requestId << resp_.errorCode;
    uint32_t nsCnt = resp_.nsVerifyList.size();
    out << nsCnt;
    for (const auto &ns : resp_.nsVerifyList) {
        out << ns;
    }
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU attach verify resp serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuAttachDetachVerifyRespMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    UbseDeSerialization in(data, size);
    in >> resp_.requestId >> resp_.errorCode;
    uint32_t nsCnt = 0;
    in >> nsCnt;
    resp_.nsVerifyList.clear();
    constexpr uint32_t MAX_VERIFY_NS_NUM = 1024;
    if (nsCnt > MAX_VERIFY_NS_NUM) {
        UBSE_LOG_ERROR << "SSU attach verify resp ns num exceeds max limit, size=" << nsCnt;
        in.SetFail();
        return UBSE_ERROR;
    }
    for (uint32_t i = 0; i < nsCnt; ++i) {
        UbseSsuNsVerifyInfo ns;
        in >> ns;
        if (!in.Check()) {
            break;
        }
        resp_.nsVerifyList.emplace_back(std::move(ns));
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "SSU attach verify resp deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

} // namespace ubse::ssu::message
