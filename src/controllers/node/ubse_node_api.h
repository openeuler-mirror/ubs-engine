/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef USBE_NODE_API_H
#define USBE_NODE_API_H
#include "ubse_api_server_module.h"
#include "ubse_common_def.h"

namespace ubse::node::api {
using ::api::server::UbseIpcMessage;
using ::api::server::UbseRequestContext;
using ubse::common::def::UbseResult;

class UbseNodeApi {
public:
    static UbseResult Register();

private:
    static uint32_t UbseQueryTopologyInfoHandle(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t UbseServerNodeGet(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t UbseServerNodeList(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t UbseServerCpuTopoList(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t UbseServerNodeNumaMemGet(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t UbseQueryClusterInfo(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t UbseQueryCpuTopo(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t UbseQueryNodeInfo(const UbseIpcMessage& req, const UbseRequestContext& context);
};
} // namespace ubse::node::api
#endif // USBE_NODE_API_H