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

#ifndef UBSE_LCNE_NODE_INFO_H
#define UBSE_LCNE_NODE_INFO_H

#include <string>             // for string, basic_string
#include <utility>            // for move
#include "ubse_common_def.h"  // for UbseResult
#include "ubse_http_common.h" // for UbseHttpMethod, UbseHttpResponse (ptr o...
#include "ubse_lcne_def.h"    // for LcneServer
#include "adapter_plugins/mti/ubse_mti_def.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"
namespace ubse::lcne {
using common::def::UbseResult;
using ubse::adapter_plugins::mti::UbseMtiIouInfo;
using ubse::mti::UbseLcneIODieInfo;

using UbseLcneIODieInfoMap = std::map<UbseMtiIouInfo, UbseLcneIODieInfo>;

class UbseLcneNodeInfo {
public:
    static UbseLcneNodeInfo& GetGetInstance()
    {
        static UbseLcneNodeInfo instance("127.0.0.1", LcneServer::realPort); // 默认服务在本地 127.0.0.1 默认端口 8088;
        return instance;
    }

    // 查询全量节点（IO DIE）信息  查询范围：本板
    UbseResult QueryAllLcneIODieInfo(UbseLcneIODieInfoMap& ubseLcneIODieInfoMap);

private:
    UbseLcneNodeInfo(std::string host, int port) : host(std::move(host)), port(port) {}

    UbseResult ParseIODieInfoQueryAllResponse(const std::string& responseStr,
                                              UbseLcneIODieInfoMap& ubseLcneIODieInfoMap);

    std::string host;
    int port;

    const std::string QUERY_ALL_URI = "/restconf/data/huawei-vbussw-service:vbussw-service/iou-infos";
};
} // namespace ubse::lcne

#endif // UBSE_LCNE_NODE_INFO_H
