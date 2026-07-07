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

#ifndef UBSE_VIP_HTTP_SERVER_H
#define UBSE_VIP_HTTP_SERVER_H

#include "ubse_common_def.h"
#include "ubse_http_common.h"

namespace ubse::vip {

/**
 * Register an HTTP route on the VIP HTTP server.
 * Routes are stored internally and applied when the VIP HTTP server starts
 * (i.e., when the node becomes master and VIP is bound).
 *
 * @param method HTTP method (GET, POST, etc.)
 * @param url    URL path for the route (e.g., "/v1/vip/status")
 * @param func   Handler function for the route
 * @return UBSE_OK on success, error code on failure
 */
ubse::common::def::UbseResult RegVipHttpService(ubse::http::UbseHttpMethod method, const std::string &url,
                                                ubse::http::UbseHttpHandlerFunc func);

} // namespace ubse::vip

#endif // UBSE_VIP_HTTP_SERVER_H
