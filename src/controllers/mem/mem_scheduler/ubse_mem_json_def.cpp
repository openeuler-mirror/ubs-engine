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
#include "ubse_mem_json_def.h"

#include <regex>
#include "src/framework/misc/ubse_str_util.h"
#include "ubse_json_util.h"
#include "ubse_logger.h"

namespace ubse::mem::strategy {
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
using namespace ubse::utils;

std::string UbseMemEventNotifyBorrowItem::ToJson()
{
    std::vector<std::string> strVec;
    for (const auto &item : this->exportLocInfo) {
        strVec.emplace_back(item.nodeId + "/" + std::to_string(item.socketId) + "/" + std::to_string(item.numaId));
    }
    std::string jLocVec2Str;
    if (!UbseJsonUtil::ConvertVector2JsonStr(strVec, jLocVec2Str)) {
        UBSE_LOG_ERROR << "ConvertVector2JsonStr fail";
        return "";
    }
    std::vector<std::string> strVec1;
    for (auto item : this->requestSize) {
        strVec1.emplace_back(std::to_string(item));
    }
    std::string jSizeVec2Str;
    if (!UbseJsonUtil::ConvertVector2JsonStr(strVec1, jSizeVec2Str)) {
        UBSE_LOG_ERROR << "ConvertVector2JsonStr fail";
        return "";
    }
    std::map<std::string, std::string> strMap;
    strMap.emplace("name", this->name);
    strMap.emplace("exportLocNum", std::to_string(exportLocNum));
    strMap.emplace("exportLocInfo", jLocVec2Str);
    strMap.emplace("requestSize", jSizeVec2Str);
    strMap.emplace("exportMemId", std::to_string(exportMemId));
    strMap.emplace("importMemId", std::to_string(importMemId));
    std::string rsStr;
    if (!UbseJsonUtil::ConvertMap2JsonStr(strMap, rsStr)) {
        UBSE_LOG_ERROR << "ConvertVector2JsonStr fail";
        return "";
    }
    return rsStr;
}

std::string UbseMemEventNotifyMessage::ToJson()
{
    std::vector<std::string> strVec;
    for (auto i : borrowItem) {
        strVec.emplace_back(i.ToJson());
    }
    std::string jItemVecStr;
    if (!UbseJsonUtil::ConvertVector2JsonStr(strVec, jItemVecStr)) {
        UBSE_LOG_ERROR << "ConvertVector2JsonStr fail";
        return "";
    }
    std::map<std::string, std::string> strMap;
    strMap.emplace("borrowItemNum", std::to_string(borrowItemNum));
    strMap.emplace("notifyNumaLoc", notifyNumaLoc.nodeId + "/" + std::to_string(notifyNumaLoc.socketId) + "/" +
                                        std::to_string(notifyNumaLoc.numaId));
    strMap.emplace("borrowItem", jItemVecStr);
    strMap.emplace("allNumaInfo", allNumaInfo);
    strMap.emplace("oomEventFlag", std::to_string(oomEventFlag));
    std::string rsStr;
    if (!UbseJsonUtil::ConvertMap2JsonStr(strMap, rsStr)) {
        UBSE_LOG_ERROR << "ConvertVector2JsonStr fail";
        return "";
    }
    return rsStr;
}
} // namespace ubse::mem::strategy