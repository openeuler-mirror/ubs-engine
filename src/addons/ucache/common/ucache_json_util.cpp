/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ucache_json_util.h"

#include <map>
#include <string>
#include <vector>

#include "ubse_logger.h"

namespace ucache {
/* 获取doc子节点中key对应的string类型value,类型检查失败返回UCACHE_ERR */
uint32_t JsonUtil::GetJsonStringValue(const rapidjson::Document& doc, const char* key, std::string& output)
{
    if (key == nullptr) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Input key is nullptr.";
        return UCACHE_ERR;
    }
    if (!doc.HasMember(key)) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Json have no member named" << key << ".";
        return UCACHE_ERR;
    }
    if (!doc[key].IsString()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Json value is not string, value=" << PrintJsonString(doc[key]) << ".";
        return UCACHE_ERR;
    }
    output = doc[key].GetString();
    return UCACHE_OK;
}

/* 将Value对象打印成json字符串 */
JSON_STR JsonUtil::PrintJsonString(const rapidjson::Value& value)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    value.Accept(writer);
    JSON_STR jsonStr = buffer.GetString();
    return jsonStr;
}
} // namespace ucache