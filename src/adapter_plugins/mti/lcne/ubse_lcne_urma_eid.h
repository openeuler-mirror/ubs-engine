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

#ifndef UBSE_LCNE_URMA_EID_H
#define UBSE_LCNE_URMA_EID_H

#include <memory>  // for allocator, unique_ptr
#include <mutex>   // for call_once, once_flag
#include <string>  // for string, basic_string
#include <utility> // for move

#include "ubse_common_def.h"  // for UbseResult
#include "ubse_http_common.h" // for UbseHttpMethod, UbseHttpResponse (ptr o...
#include "ubse_lcne_def.h"    // for LcneServer
#include "adapter_plugins/mti/ubse_mti_def.h"

namespace ubse::lcne {
using namespace common::def;
using namespace ubse::http;

class UbseLcneUrmaEid {
public:
    static UbseLcneUrmaEid &GetInstance()
    {
        static UbseLcneUrmaEid instance("127.0.0.1", LcneServer::realPort); // 默认服务在本地 127.0.0.1 默认端口 8799;
        return instance;
    }

    // 发送请求
    UbseResult GetUrmaEid(
        std::map<adapter_plugins::mti::UbseDevName, adapter_plugins::mti::UbseMtiEidGroup>& allSocketComEid);

private:
    UbseLcneUrmaEid(std::string host, int port) : host_(std::move(host)), port_(port) {}

    UbseResult ParseGetUrmaEidResponse(const std::string& responseStr,
        std::map<adapter_plugins::mti::UbseDevName, adapter_plugins::mti::UbseMtiEidGroup>& ss);

    std::string host_;
    int port_;

    const std::string GET_URMA_EID_URI = "/restconf/data/huawei-vbussw-service:vbussw-service/static-urma-eids";

    const std::string GET_URMA_EID_RESULT_SUCCESS = "Success";
};
bool IsValidUrmaEid(const std::string &eid);
} // namespace ubse::lcne

#endif // UBSE_LCNE_URMA_EID_H
