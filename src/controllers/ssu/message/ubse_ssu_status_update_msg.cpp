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
 
#include "ubse_ssu_status_update_msg.h"

#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_serial_util.h"

namespace ubse::ssu::message {

using namespace ubse::log;
using namespace ubse::serial;

UBSE_DEFINE_THIS_MODULE("ubse");

void UbseSsuStatusReqMsg::SetStatusUpdateReq(const UbseSsuStatusUpdateReq &req)
{
    req_ = req;
}

const UbseSsuStatusUpdateReq &UbseSsuStatusReqMsg::GetStatusUpdateReq() const
{
    return req_;
}

void UbseSsuStatusRspMsg::SetStatusUpdateRsp(const UbseSsuStatusUpdateRsp &rsp)
{
    rsp_ = rsp;
}

const UbseSsuStatusUpdateRsp &UbseSsuStatusRspMsg::GetStatusUpdateRsp() const
{
    return rsp_;
}

UbseSerialization &operator<<(UbseSerialization &serializer, const UbseSsuStatusUpdateReq &req)
{
    serializer << req.requestName << enum_v(req.state);
    return serializer;
}

UbseDeSerialization &operator>>(UbseDeSerialization &deserializer, UbseSsuStatusUpdateReq &req)
{
    deserializer >> req.requestName >> enum_v(req.state);
    return deserializer;
}

UbseSerialization &operator<<(UbseSerialization &serializer, const UbseSsuStatusUpdateRsp &rsp)
{
    serializer << rsp.errorCode;
    return serializer;
}

UbseDeSerialization &operator>>(UbseDeSerialization &deserializer, UbseSsuStatusUpdateRsp &rsp)
{
    deserializer >> rsp.errorCode;
    return deserializer;
}

uint32_t UbseSsuStatusReqMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    UbseSerialization out;
    out << req_;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU status req serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuStatusReqMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    UbseDeSerialization in(data, size);
    in >> req_;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "SSU status req deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseSsuStatusRspMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    UbseSerialization out;
    out << rsp_;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU status rsp serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuStatusRspMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    UbseDeSerialization in(data, size);
    in >> rsp_;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "SSU status rsp deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

} // namespace ubse::ssu::message
