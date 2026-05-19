/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "mp_borrow_conf_util.h"
#include "ubse_logger.h"
#include "mp_configuration.h"

namespace mempooling {
using namespace ubse::log;

MpResult MpParseGroupProviderConf::ParseBorrowConf(UbseMemGroupNodeList& groupList,
                                                   UbseMemProviderNodeList& providerList)
{
    uint32_t ret = UbseNodeController::GetInstance().GetMemGroupNodeList(groupList);
    if (ret != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Get node groups failed";
        return ret;
    }
    if (groupList.empty()) {
        std::unordered_map<std::string, UbseNodeInfo> allNodes = UbseNodeController::GetInstance().GetAllNodes();
        if (allNodes.empty()) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Get all nodes empty.";
            return MEM_POOLING_ERROR;
        }
        std::vector<UbseNodeInfo> firstGroup;
        firstGroup.reserve(allNodes.size());
        for (const auto& kv : allNodes) {
            firstGroup.push_back(kv.second);
        }
        groupList.push_back(std::move(firstGroup));
    }
    ret = UbseNodeController::GetInstance().GetMemProviderNodeList(providerList);
    if (ret != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Get node providers failed";
        return ret;
    }
    return MEM_POOLING_OK;
}

MpResult MpParseGroupProviderConf::BuildBorrowMap(std::map<std::string, std::unordered_set<std::string>> &borrowMap)
{
    UbseMemGroupNodeList groupList;
    UbseMemProviderNodeList providerList;
    MpResult ret = ParseBorrowConf(groupList, providerList);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Parse borrow conf failed";
        return ret;
    }
    borrowMap.clear();
    std::unordered_set<std::string> providerSet;
    for (const auto& p : providerList) {
        (void)providerSet.insert(p.nodeId);
    }
    bool providerEmpty = providerSet.empty();
    for (size_t i = 0; i < groupList.size(); ++i) {
        for (const auto& node : groupList[i]) {
            auto& borrowable = borrowMap[node.nodeId];
            if (!providerEmpty && providerSet.count(node.nodeId)) {
                continue;
            }
            for (const auto& candidate : groupList[i]) {
                if (candidate.nodeId == node.nodeId) {
                    continue;
                }
                if (providerEmpty) {
                    (void)borrowable.insert(candidate.nodeId);
                } else {
                    if (providerSet.count(candidate.nodeId)) {
                        (void)borrowable.insert(candidate.nodeId);
                    }
                }
            }
        }
    }
    return MEM_POOLING_OK;
}

MpResult MpParseGroupProviderConf::GetBorrowableList(const std::string& curNid,
                                                     std::unordered_set<std::string>& borrowableNidSet) // 改成引用
{
    std::map<std::string, std::unordered_set<std::string>> borrowMap;
    uint32_t ret = BuildBorrowMap(borrowMap);
    if (ret != 0) {
        return ret;
    }
    for (const auto& kv : borrowMap) {
        for (const auto& target : kv.second) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "srcNid=" << kv.first << ", targetNid=" << target;
        }
    }
    auto it = borrowMap.find(curNid);
    if (it == borrowMap.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Input nodeId = " << curNid << " not exist.";
        return MEM_POOLING_ERROR;
    }
    borrowableNidSet = it->second;
    return MEM_POOLING_OK;
}
} // namespace mempooling