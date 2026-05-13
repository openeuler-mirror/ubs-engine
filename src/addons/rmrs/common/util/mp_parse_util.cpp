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

#include "mp_parse_util.h"
#include <rapidjson/document.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>
#include "mp_json_util.h"

namespace mempooling {

bool CheckSrcParamSuccess(const rapidjson::Value& srcParam, std::vector<std::string>& needPraseParam)
{
    bool checkSuccess = true;
    for (auto param : needPraseParam) {
        if (!srcParam.HasMember(param.c_str())) {
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "Json parse failed, dont have member " << param << ".";
            checkSuccess = false;
        }
    }
    return checkSuccess;
}

} // namespace mempooling