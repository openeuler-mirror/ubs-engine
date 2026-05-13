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

#include "ubse_election_utils.h"
#include <sstream>
#include <string>
#include <vector>

namespace ubse::election::utils {

std::vector<std::string> Split(const std::string& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

const size_t FIELDS_COUNT = 4;
const size_t PAIR_SIZE = 2;
std::vector<NodeLinkInfo> ParseEventMessage(const std::string& eventMessage)
{
    std::vector<NodeLinkInfo> linkList;
    auto linkBlocks = Split(eventMessage, ';');
    for (const auto& block : linkBlocks) {
        if (block.empty()) {
            continue;
        }
        auto fields = Split(block, ',');
        if (fields.size() != FIELDS_COUNT) {
            continue; // 字段数量不匹配，跳过
        }
        std::string nodeId, changeChType;
        uint32_t state, timestamp;
        bool valid = true;
        auto nid = Split(fields[0], ':');
        if (nid.size() == PAIR_SIZE) {
            nodeId = nid[1];
        } else {
            valid = false;
        }
        auto st = Split(fields[1], ':');
        if (valid && st.size() == PAIR_SIZE) {
            try {
                state = std::stoi(st[1]);
            } catch (...) {
                valid = false;
            }
        }
        auto ts = Split(fields[2], ':');
        if (valid && ts.size() == PAIR_SIZE) {
            try {
                timestamp = std::stoll(ts[1]);
            } catch (...) {
                valid = false;
            }
        }
        auto ct = Split(fields[3], ':');
        if (valid && ct.size() == PAIR_SIZE) {
            changeChType = ct[1];
        } else {
            valid = false;
        }
        if (valid) {
            NodeLinkInfo nodeLinkInfo{nodeId, state, timestamp, changeChType};
            linkList.emplace_back(nodeLinkInfo);
        }
    }
    return linkList;
}

bool parseFaultEventMsg(const std::string& msg, std::string& faultNodeId, std::string& faultType)
{
    size_t pos = msg.find('_');
    if (pos == std::string::npos) {
        return false;
    }

    faultNodeId = msg.substr(0, pos);
    faultType = msg.substr(pos + 1);
    return true;
}
} // namespace ubse::election::utils