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

#ifndef MP_ANTI_PARAM_JSON_UTIL_H
#define MP_ANTI_PARAM_JSON_UTIL_H

#include <map>
#include <string>
#include <vector>

#include "ubse_logger.h"
#include "mp_error.h"

namespace mempooling {
using namespace ubse::log;

struct MpUpdateAntiNodeParam {
    std::string ToJson() const;
    bool FromJson(const std::string& jsonString);

    std::map<std::string, std::vector<std::string>> nodeAntiAffinityMap;
};

struct SyncUpdateAntiNodeParam {
    std::string keyPrefix;
    std::string key;
    std::map<std::string, std::vector<std::string>> nodeAntiAffinityMap;
};

class SyncAntiNodeDataResult {
public:
    uint32_t errCode;
};

} // namespace mempooling

#endif // MP_ANTI_PARAM_JSON_UTIL_H