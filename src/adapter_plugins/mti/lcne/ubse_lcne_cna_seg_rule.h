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

#ifndef UBSE_LCNE_CNA_SEG_RULE_H
#define UBSE_LCNE_CNA_SEG_RULE_H

#include <cstdint> // for uint8_t
#include <string>  // for string, basic_string
#include <tuple>   // for tuple
#include <utility> // for move
#include <vector>  // for vector

#include "ubse_common_def.h"       // for UbseResult
#include "ubse_http_common.h"      // for UbseHttpResponse
#include "ubse_lcne_def.h"         // for LcneServer
#include "ubse_mti_eid_internal.h" // for CnaBitRange

namespace ubse::lcne {
using common::def::UbseResult;

class UbseLcneCnaSegRule {
public:
    static UbseLcneCnaSegRule& GetInstance()
    {
        static UbseLcneCnaSegRule instance("127.0.0.1", LcneServer::realPort);
        return instance;
    }

    // 查询并解析LCNE地址段规则（group-id、server-index的start-bit/end-bit）
    UbseResult QueryCnaRule(std::vector<utils::CnaBitRange>& cnaRules);

private:
    UbseLcneCnaSegRule(std::string host, int port) : host_(std::move(host)), port_(port) {}

    UbseResult ParseCnaRule(const std::string& responseStr, std::vector<utils::CnaBitRange>& cnaRules);

    std::string host_;
    int port_;

    const std::string QUERY_URI = "/restconf/data/huawei-lingqu-topology:lingqu-topology/address-segment-rule";
};
} // namespace ubse::lcne

#endif // UBSE_LCNE_CNA_SEG_RULE_H
