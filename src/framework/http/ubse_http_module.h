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

#ifndef UBSE_HTTP_MODULE_H
#define UBSE_HTTP_MODULE_H

#include <httplib.h>
#include <cstdint>            // for uint32_t, uint8_t
#include <cstring>            // for size_t
#include <shared_mutex>
#include <string>             // for string, allocator, basic_string
#include <unordered_map>      // for hash, unordered_map
#include <utility>            // for pair

#include "ubse_common_def.h"  // for UbseResult
#include "ubse_http_common.h" // for UbseHttpResponse (ptr only), UbseHttpRe...
#include "ubse_module.h"      // for UbseModule
namespace ubse::http {
using namespace ubse::module;
const size_t MAX_TOTAL_HEADERS_SIZE = 8 * 1024; // 8KB

class UbseHttpModule : public UbseModule {
public:
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    static void RegHttpTcpService(UbseHttpMethod method, const std::string &url, UbseHttpHandlerFunc func);

    static uint32_t HttpSend(const std::string &host, int port, UbseHttpRequest &req, UbseHttpResponse &rsp);

private:
    static UbseResult MakeError(uint32_t code);
};
} // namespace ubse::http

#endif // UBSE_HTTP_MODULE_H