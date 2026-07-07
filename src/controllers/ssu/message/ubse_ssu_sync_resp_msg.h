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

#ifndef UBSE_SSU_SYNC_RESP_MSG_H
#define UBSE_SSU_SYNC_RESP_MSG_H

#include <string>
#include "ubse_com.h"

namespace ubse::ssu::message {

class UbseSsuSyncRespMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuSyncRespMsg() = default;
    explicit UbseSsuSyncRespMsg(uint32_t errCode);

    uint32_t GetErrorCode() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    uint32_t errorCode{0};
};

} // namespace ubse::ssu::message

#endif // UBSE_SSU_SYNC_RESP_MSG_H
