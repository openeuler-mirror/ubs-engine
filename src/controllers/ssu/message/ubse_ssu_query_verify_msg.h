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

} // namespace ubse::ssu::message

#endif // UBSE_SSU_QUERY_VERIFY_MSG_H
