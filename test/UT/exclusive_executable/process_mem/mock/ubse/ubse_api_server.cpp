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

#include "ubse_api_server.h"

namespace api::server {

uint32_t RegisterIpcHandler(uint16_t moduleCode, uint16_t opCode, UbseIpcHandler handler, const std::string& object)
{
    return 0;
}

uint32_t SendResponse(uint32_t statusCode, uint64_t requestId, UbseIpcMessage& response)
{
    return 0;
}
} // namespace api::server
