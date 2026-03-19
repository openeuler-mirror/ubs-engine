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

#include "mp_mem_json_util.h"

#include <iostream>
#include <regex>
#include <sstream>
#include <string>

#include "mp_configuration.h"
#include "mp_json_util.h"
#include "mp_string_util.h"
#include "ubse_logger.h"

namespace mempooling {
using namespace ubse::log;
using namespace mempooling::migrate;

bool MpFaultMemIdParam::FromJson(const std::string &jsonString)
{
    JSON_MAP MpFaultMemIdParamMap;
    (void)MpFaultMemIdParamMap.emplace("importNodeId", "");
    (void)MpFaultMemIdParamMap.emplace("importMemId", "");

    if (!JsonUtil::RackMemConvertJsonStr2Map(jsonString, MpFaultMemIdParamMap)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "RackMemConvertJsonStr2Map error."
                                                          << "Json:" << jsonString;
        return false;
    }
    this->importNodeId = MpFaultMemIdParamMap["importNodeId"];
    this->importMemId = std::stoull(MpFaultMemIdParamMap["importMemId"]);
    log();
    return true;
}

} // namespace mempooling