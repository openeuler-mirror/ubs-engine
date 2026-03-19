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

#ifndef UBSE_LCNE_BUSINSTANCE_H
#define UBSE_LCNE_BUSINSTANCE_H

#include <memory>             // for allocator, unique_ptr
#include <mutex>              // for call_once, once_flag
#include <string>             // for string, basic_string
#include <utility>            // for move

#include "ubse_common_def.h"  // for UbseResult
#include "ubse_http_common.h" // for UbseHttpMethod, UbseHttpResponse (ptr o...

 // for LcneServer
#include "ubse_lcne_def.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"

namespace ubse::lcne {
using namespace common::def;
using namespace ubse::http;
using namespace ubse::mti;

class UbseLcneBusInstance {
public:
    static UbseLcneBusInstance &GetInstance()
    {
        static UbseLcneBusInstance instance("127.0.0.1", LcneServer::realPort); // 默认服务在本地 127.0.0.1 默认端口 8799;
        return instance;
    }

    // 查询节点物理上bus instance信息
    UbseResult QueryBusinstance(UbseLcneBusInstanceInfo &busInstanceInfo);

private:
    UbseLcneBusInstance(std::string host, int port) : host_(std::move(host)), port_(port) {}
    static std::unique_ptr<UbseLcneBusInstance> instance;
    std::string host_;
    int port_;

    UbseResult ParseQueryBusinstanceResponse(const std::string &responseStr, UbseLcneBusInstanceInfo &responseObj);
};
} // namespace ubse::lcne
#endif // UBSE_LCNE_BUSINSTANCE_H
