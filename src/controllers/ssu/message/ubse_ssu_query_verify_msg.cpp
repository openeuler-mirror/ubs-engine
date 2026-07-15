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

#include "ubse_ssu_query_verify_msg.h"

#include "ubse_ssu_alloc_msg.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_serial_util.h"

namespace ubse::ssu::message {

using namespace ubse::log;
using namespace ubse::serial;

UBSE_DEFINE_THIS_MODULE("ubse");

// UbseSsuNsStats 序列化/反序列化
UbseSerialization &operator<<(UbseSerialization &serializer, const UbseSsuNsStats &stats)
{
    serializer << stats.nsUuid << stats.nsId << stats.totalSize << stats.usedSize;
    return serializer;
}

UbseDeSerialization &operator>>(UbseDeSerialization &deserializer, UbseSsuNsStats &stats)
{
    deserializer >> stats.nsUuid >> stats.nsId >> stats.totalSize >> stats.usedSize;
    return deserializer;
}

// ====== GetNsStats ======

UbseSsuGetNsStatsReqMsg::UbseSsuGetNsStatsReqMsg(const std::string &requestId,
                                                 const std::string &requestNodeId,
                                                 const std::string &name,
                                                 const UbseSsuAllocIdentityInfo &identity)
    : req_{requestId, requestNodeId, name, identity}
{
}

const UbseSsuGetNsStatsReq &UbseSsuGetNsStatsReqMsg::GetGetNsStatsReq() const
{
    return req_;
}

uint32_t UbseSsuGetNsStatsReqMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    UbseSerialization out;
    out << req_.requestId << req_.requestNodeId << req_.name;
    out << req_.identityInfo.uid << req_.identityInfo.userName;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU get ns stats req serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuGetNsStatsReqMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    UbseDeSerialization in(data, size);
    in >> req_.requestId >> req_.requestNodeId >> req_.name;
    in >> req_.identityInfo.uid >> req_.identityInfo.userName;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "SSU get ns stats req deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseSsuGetNsStatsRespMsg::UbseSsuGetNsStatsRespMsg(const UbseSsuGetNsStatsResp &resp) : resp_(resp) {}

const UbseSsuGetNsStatsResp &UbseSsuGetNsStatsRespMsg::GetGetNsStatsResp() const
{
    return resp_;
}

uint32_t UbseSsuGetNsStatsRespMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    UbseSerialization out;
    out << resp_.requestId << resp_.errorCode;
    uint32_t cnt = resp_.statsList.size();
    out << cnt;
    for (const auto &stats : resp_.statsList) {
        out << stats;
    }
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU get ns stats resp serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuGetNsStatsRespMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    UbseDeSerialization in(data, size);
    in >> resp_.requestId >> resp_.errorCode;
    uint32_t cnt = 0;
    in >> cnt;
    resp_.statsList.clear();
    constexpr uint32_t MAX_NS_STATS_NUM = 1024;
    if (cnt > MAX_NS_STATS_NUM) {
        UBSE_LOG_ERROR << "SSU get ns stats resp stats num exceeds max limit, size=" << cnt;
        in.SetFail();
        return UBSE_ERROR;
    }
    for (uint32_t i = 0; i < cnt; ++i) {
        UbseSsuNsStats stats;
        in >> stats;
        if (!in.Check()) {
            break;
        }
        resp_.statsList.emplace_back(std::move(stats));
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "SSU get ns stats resp deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

} // namespace ubse::ssu::message
