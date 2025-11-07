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

#ifndef UBSE_LCNE_HOST_INFO_H
#define UBSE_LCNE_HOST_INFO_H

#include <string>             // for string, basic_string
#include <utility>            // for move
#include "ubse_common_def.h"  // for UbseResult
#include "ubse_http_common.h" // for UbseHttpMethod, UbseHttpResponse (ptr o...
#include "ubse_lcne_def.h"
#include "ubse_topology_interface.h"

namespace ubse::lcne {
using namespace common::def;
using namespace ubse::http;
using namespace ubse::mti;

class UbseLcneHostInfo {
public:
    static UbseLcneHostInfo &GetGetInstance()
    {
        static UbseLcneHostInfo instance("127.0.0.1", LcneServer::realPort); // 默认服务在本地 127.0.0.1 默认端口 8088;
        return instance;
    }

    // 查询节Host信息
    UbseResult QueryLcneHostInfo(UbseLcneOSInfo &ubseLcneOSInfo);

private:
    UbseLcneHostInfo(std::string host, int port) : host(std::move(host)), port(port) {}

    UbseResult ParseHostQueryResponse(const std::string &responseStr, UbseLcneOSInfo &ubseLcneOSInfo);

    std::string host;
    int port;

    const std::string QUERY_URI =
        "/restconf/data/huawei-vbussw-inventory:vbussw-inventory/logic-entities";
};
}


#endif // UBSE_LCNE_HOST_INFO_H
