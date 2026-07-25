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

#ifndef UBSE_TOPO_CNA_H
#define UBSE_TOPO_CNA_H

#include <string>
#include <utility>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_http_common.h"
#include "ubse_lcne_def.h"
#include "ubse_lcne_topology.h"

namespace ubse::lcne {
using common::def::UbseResult;
using ubse::mti::LcneNodeCnaInfo;

class UbseTopoCna {
public:
    static UbseTopoCna& GetInstance()
    {
        static UbseTopoCna instance("127.0.0.1", LcneServer::realPort); // 默认服务在本地 127.0.0.1 默认端口 8799;
        return instance;
    }
    // 全量查询CNA
    UbseResult QueryTopoCna(std::vector<LcneNodeCnaInfo>& lcneNodeCnaInfos);

private:
    UbseTopoCna(std::string host, int port) : host_(std::move(host)), port_(port) {}
    UbseResult ParseTopoCnaRsp(std::string& resBody, std::vector<LcneNodeCnaInfo>& lcneNodeCnaInfos);
    std::string host_;
    int port_;

    const std::string QUERY_URI = "/restconf/data/huawei-lingqu-topology:lingqu-topology/addresses";
};
} // namespace ubse::lcne

#endif // UBSE_TOPO_CNA_H
