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

#ifndef UBSE_SSU_QUERY_VERIFY_MSG_H
#define UBSE_SSU_QUERY_VERIFY_MSG_H

#include <cstdint>
#include <string>
#include <vector>
#include "ubse_com.h"
#include "ubse_ssu_service.h"

namespace ubse::ssu::message {

using namespace ubse::plugin::service::ssu;

// ====== GetNsStats 查询命名空间统计信息 ======
struct UbseSsuGetNsStatsReq {
    std::string requestId;
    std::string requestNodeId;
    std::string name;
    UbseSsuAllocIdentityInfo identityInfo;
};

struct UbseSsuGetNsStatsResp {
    std::string requestId;
    uint32_t errorCode{0};
    std::vector<UbseSsuNsStats> statsList;
};

class UbseSsuGetNsStatsReqMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuGetNsStatsReqMsg() = default;

    UbseSsuGetNsStatsReqMsg(const std::string &requestId, const std::string &requestNodeId,
                            const std::string &name, const UbseSsuAllocIdentityInfo &identity);

    const UbseSsuGetNsStatsReq &GetGetNsStatsReq() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuGetNsStatsReq req_{};
};

class UbseSsuGetNsStatsRespMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuGetNsStatsRespMsg() = default;

    explicit UbseSsuGetNsStatsRespMsg(const UbseSsuGetNsStatsResp &resp);
    const UbseSsuGetNsStatsResp &GetGetNsStatsResp() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuGetNsStatsResp resp_{};
};

// ====== ListAllocInfo 查询分配信息列表 ======
struct UbseSsuListAllocInfoReq {
    std::string requestId;
    std::string requestNodeId;
    UbseSsuAllocIdentityInfo identityInfo;
};

struct UbseSsuListAllocInfoResp {
    std::string requestId;
    uint32_t errorCode{0};
    std::vector<UbseSsuAllocResult> results; // master端直接返回完整分配结果列表
};

class UbseSsuListAllocInfoReqMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuListAllocInfoReqMsg() = default;

    UbseSsuListAllocInfoReqMsg(const std::string &requestId, const std::string &requestNodeId,
                               const UbseSsuAllocIdentityInfo &identity);

    const UbseSsuListAllocInfoReq &GetListAllocInfoReq() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuListAllocInfoReq req_{};
};

class UbseSsuListAllocInfoRespMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuListAllocInfoRespMsg() = default;

    explicit UbseSsuListAllocInfoRespMsg(const UbseSsuListAllocInfoResp &resp);
    const UbseSsuListAllocInfoResp &GetListAllocInfoResp() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuListAllocInfoResp resp_{};
};

// ====== GetAllocInfoByName 根据名称查询分配信息 ======
struct UbseSsuGetAllocInfoReq {
    std::string requestId;
    std::string requestNodeId;
    std::string name;
    UbseSsuAllocIdentityInfo identityInfo;
};

struct UbseSsuGetAllocInfoResp {
    std::string requestId;
    uint32_t errorCode{0};
    UbseSsuAllocResult result; // master端直接返回完整的分配结果
};

class UbseSsuGetAllocInfoReqMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuGetAllocInfoReqMsg() = default;

    UbseSsuGetAllocInfoReqMsg(const std::string &requestId, const std::string &requestNodeId, const std::string &name,
                              const UbseSsuAllocIdentityInfo &identity);

    const UbseSsuGetAllocInfoReq &GetGetAllocInfoReq() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuGetAllocInfoReq req_{};
};

class UbseSsuGetAllocInfoRespMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuGetAllocInfoRespMsg() = default;

    explicit UbseSsuGetAllocInfoRespMsg(const UbseSsuGetAllocInfoResp &resp);
    const UbseSsuGetAllocInfoResp &GetGetAllocInfoResp() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuGetAllocInfoResp resp_{};
};

// ====== GetConnectInfo 查询连接信息 ======
struct UbseSsuGetConnectInfoReq {
    std::string requestId;
    std::string requestNodeId;
    std::string name;
    UbseSsuAllocIdentityInfo identityInfo;
    bool hasVfe{false};
    UbseSsuVfe vfe;
};

struct UbseSsuGetConnectInfoResp {
    std::string requestId;
    uint32_t errorCode{0};
    std::vector<UbseSsuConnectInfo> connectInfoList;
};

class UbseSsuGetConnectInfoReqMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuGetConnectInfoReqMsg() = default;

    UbseSsuGetConnectInfoReqMsg(const std::string &requestId, const std::string &requestNodeId, const std::string &name,
                                const UbseSsuAllocIdentityInfo &identity, const UbseSsuVfe *vfe);

    const UbseSsuGetConnectInfoReq &GetGetConnectInfoReq() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuGetConnectInfoReq req_{};
};

class UbseSsuGetConnectInfoRespMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuGetConnectInfoRespMsg() = default;

    explicit UbseSsuGetConnectInfoRespMsg(const UbseSsuGetConnectInfoResp &resp);
    const UbseSsuGetConnectInfoResp &GetGetConnectInfoResp() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuGetConnectInfoResp resp_{};
};

} // namespace ubse::ssu::message

#endif // UBSE_SSU_QUERY_VERIFY_MSG_H
