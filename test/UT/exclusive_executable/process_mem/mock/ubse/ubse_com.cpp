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

#include "ubse_com.h"
#include "mock_control.h"

namespace ubse::com {

static std::vector<MockRpcSendRecord> g_mockRpcSendRecords;

void MockResetRpcState()
{
    g_mockRpcSendRecords.clear();
}

const std::vector<MockRpcSendRecord>& MockGetRpcSendRecords()
{
    return g_mockRpcSendRecords;
}

uint32_t UbseRegRpcService(const UbseComEndpoint& endpoint, const UbseComServiceHandler& handler)
{
    return 0;
}

uint32_t UbseRpcSend(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                     const UbseComRespHandler& handler)
{
    MockRpcSendRecord record;
    record.address = endpoint.address;
    if (reqData.data != nullptr && reqData.len > 0) {
        record.payload.assign(reinterpret_cast<const char*>(reqData.data), reqData.len);
    }
    g_mockRpcSendRecords.push_back(std::move(record));
    return 0;
}

uint32_t UbseRpcAsyncSend(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                          const UbseComRespHandler& handler)
{
    return 0;
}
} // namespace ubse::com
