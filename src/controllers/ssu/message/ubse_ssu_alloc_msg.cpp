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

#include "ubse_ssu_alloc_msg.h"

#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_serial_util.h"

namespace ubse::ssu::message {

using namespace ubse::log;
using namespace ubse::serial;

UBSE_DEFINE_THIS_MODULE("ubse");

UbseSsuAllocReqMsg::UbseSsuAllocReqMsg(const std::string &requestId, const std::string &requestNodeId,
    const UbseSsuAllocIdentityInfo &identity, const UbseSsuAllocSpaceReq &req)
    : req_{requestId, requestNodeId, identity, req}
{
}

const UbseSsuAllocReq &UbseSsuAllocReqMsg::GetAllocRequest() const
{
    return req_;
}

const UbseSsuAllocResp &UbseSsuAllocRespMsg::GetSsuAllocResp() const
{
    return resp_;
}

UbseSerialization &operator<<(UbseSerialization &serializer, const UbseSsuNameSpaceInfo &info)
{
    serializer << info.tgtEid << info.tgtNqn << info.nsUuid << info.namespaceId
               << info.nsDevPath << info.nsSize << enum_v(info.lbaFormat)
               << info.allowHostNqnList;
    return serializer;
}

UbseDeSerialization &operator>>(UbseDeSerialization &deserializer, UbseSsuNameSpaceInfo &info)
{
    deserializer >> info.tgtEid >> info.tgtNqn >> info.nsUuid >> info.namespaceId
                 >> info.nsDevPath >> info.nsSize >> enum_v(info.lbaFormat)
                 >> info.allowHostNqnList;
    return deserializer;
}

UbseSerialization &operator<<(UbseSerialization &serializer, const UbseSsuAllocResult &result)
{
    serializer << result.name;
    uint32_t size = result.nameSpaceList.size();
    serializer << size << enum_v(result.strategy);
    for (const auto &ns : result.nameSpaceList) {
        serializer << ns;
    }
    return serializer;
}

UbseDeSerialization &operator>>(UbseDeSerialization &deserializer, UbseSsuAllocResult &result)
{
    deserializer >> result.name;
    uint32_t size = 0;
    deserializer >> size >> enum_v(result.strategy);
    result.nameSpaceList.clear();
    
    constexpr uint32_t MAX_ALLOC_RESULT_NS_NUM = 1024;
    if (size > MAX_ALLOC_RESULT_NS_NUM) {
        UBSE_LOG_ERROR << "SSU alloc result ns num exceeds max limit, size=" << size;
        // 主动标记反序列化失败， Deserialize 将返回 UBSE_ERROR，
        deserializer.SetFail();
        return deserializer;
    }
    // 循环内检查deserializer.Check()，防止攻击者构造极大 size 字段
    for (uint32_t i = 0; i < size; ++i) {
        UbseSsuNameSpaceInfo ns;
        deserializer >> ns;
        if (!deserializer.Check()) {
            break;
        }
        result.nameSpaceList.emplace_back(std::move(ns));
    }
    return deserializer;
}

uint32_t UbseSsuAllocReqMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    UbseSerialization out;
    out << req_.requestId << req_.requestNodeId;
    out << req_.identityInfo.uid << req_.identityInfo.userName;
    out << req_.allocReq.name << req_.allocReq.nsSize << req_.allocReq.nsNum;
    out << enum_v(req_.allocReq.lbaFormat);
    out << enum_v(req_.allocReq.strategy);
    out << req_.allocReq.tenant;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU alloc req serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuAllocReqMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    UbseDeSerialization in(data, size);
    in >> req_.requestId >> req_.requestNodeId;
    in >> req_.identityInfo.uid >> req_.identityInfo.userName;
    in >> req_.allocReq.name >> req_.allocReq.nsSize >> req_.allocReq.nsNum;
    in >> enum_v(req_.allocReq.lbaFormat);
    in >> enum_v(req_.allocReq.strategy);
    in >> req_.allocReq.tenant;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "SSU alloc req deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseSsuAllocRespMsg::UbseSsuAllocRespMsg(const UbseSsuAllocResp &resp) : resp_(resp)
{
}

uint32_t UbseSsuAllocRespMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    UbseSerialization out;
    out << resp_.requestId;
    out << resp_.errorCode;
    out << enum_v(resp_.state);
    out << resp_.allocResult;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU alloc resp serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuAllocRespMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    UbseDeSerialization in(data, size);
    in >> resp_.requestId >> resp_.errorCode;
    in >> enum_v(resp_.state);
    in >> resp_.allocResult;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "SSU alloc resp deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

} // namespace ubse::ssu::message
