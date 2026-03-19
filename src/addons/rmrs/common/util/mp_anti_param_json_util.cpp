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

#include "mp_anti_param_json_util.h"

#include <string>

#include "mp_configuration.h"
#include "mp_json_util.h"
#include "ubse_node_controller.h"
#include "ubse_node.h"

namespace mempooling {
using namespace ubse::log;
using namespace ubse::nodeController;

bool MpUpdateAntiNodeParam::FromJson(const std::string &jsonString)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemBorrow][AntiAffinity] Start get anti data from jsonString : " << jsonString;
    JSON_MAP MpUpdateAntiNodeParamMap;
    std::vector<std::string> nodeList = MpConfiguration::GetInstance().GetNodeIds();
    for (auto &node : nodeList) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][AntiAffinity] nodeId=" << node;
        MpUpdateAntiNodeParamMap.emplace(node, "");
    }

    if (!JsonUtil::RackMemConvertJsonStr2Map(jsonString, MpUpdateAntiNodeParamMap)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][AntiAffinity] RackMemConvertJsonStr2Map error.";
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][AntiAffinity] Json = " << jsonString.c_str();
        return false;
    }

    for (const auto &[key, param] : MpUpdateAntiNodeParamMap) {
        JSON_VEC antiNodeVec;
        if (!JsonUtil::RackMemConvertJsonStr2Vec(param, antiNodeVec)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][AntiAffinity] RackMemConvertJsonStr2Vec error.";
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][AntiAffinity] Json = " << jsonString.c_str();
            return false;
        }
        for (auto it = antiNodeVec.begin(); it != antiNodeVec.end();) {
            if (it->length() == 0) {
                it = antiNodeVec.erase(it);
                continue;
            }
            it++;
        }
        for (size_t i = 0; i < antiNodeVec.size(); ++i) {
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][AntiAffinity] srcNodeId=" << key << ", antiNodeId=" << antiNodeVec[i];
            this->nodeAntiAffinityMap[key].emplace_back(antiNodeVec[i]);
        }
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemBorrow][AntiAffinity] Get anti data from jsonString success.";

    return true;
}

std::string MpUpdateAntiNodeParam::ToJson() const
{
    JSON_MAP MpUpdateAntiNodeMap;
    for (auto &item : this->nodeAntiAffinityMap) {
        JSON_VEC AntiNodeVec;
        for (auto &numa : item.second) {
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][AntiAffinity] node = " << item.first << " ,antiNode = " << numa;
            (void)AntiNodeVec.emplace_back(numa);
        }
        JSON_STR AntiNodeStr;
        if (!JsonUtil::RackMemConvertVector2JsonStr(AntiNodeVec, AntiNodeStr)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][AntiAffinity] RackMemConvertVector2JsonStr error.";
            return "";
        }
        (void)MpUpdateAntiNodeMap.emplace(item.first, AntiNodeStr);
    }
    std::vector<std::string> nodeList = MpConfiguration::GetInstance().GetNodeIds();
    for (auto &node : nodeList) {
        if (MpUpdateAntiNodeMap.find(node) != MpUpdateAntiNodeMap.end()) {
            continue;
        }
        JSON_VEC AntiNodeVec;
        (void)AntiNodeVec.emplace_back("");
        JSON_STR AntiNodeStr;
        if (!JsonUtil::RackMemConvertVector2JsonStr(AntiNodeVec, AntiNodeStr)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][AntiAffinity] RackMemConvertVector2JsonStr error.";
            return "";
        }
        (void)MpUpdateAntiNodeMap.emplace(node, AntiNodeStr);
    }
    JSON_STR res;
    if (!JsonUtil::RackMemConvertMap2JsonStr(MpUpdateAntiNodeMap, res)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][AntiAffinity] RackMemConvertMap2JsonStr error.";
        return "";
    }
    return res;
}

} // namespace mempooling
