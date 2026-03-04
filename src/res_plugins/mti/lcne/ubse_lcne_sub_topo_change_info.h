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

#ifndef UBSE_LCNE_LINK_INFO_H
#define UBSE_LCNE_LINK_INFO_H
#include "ubse_http_module.h"
#include "ubse_lcne_def.h"

namespace ubse::lcne {
using namespace ubse::http;

class UbseLcneLinkInfo {
public:
    static UbseLcneLinkInfo &GetInstance()
    {
        static UbseLcneLinkInfo instance("127.0.0.1",
                                         LcneServer::realPort); // 默认服务在本地 127.0.0.1 默认端口 8799;
        return instance;
    }

    // 初始化时订阅拓扑信息
    uint32_t SubLcneLinkInfo();

private:
    UbseLcneLinkInfo(const std::string &host, int port) : host(host), port(port) {}
    uint32_t ParseMonitorData(std::string &resBody);
    std::string host;
    int port;
};
} // namespace ubse::lcne
#endif // UBSE_LCNE_LINK_INFO_H
