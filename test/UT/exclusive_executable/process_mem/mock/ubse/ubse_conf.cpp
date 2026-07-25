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

#include "ubse_conf.h"
namespace ubse::config {

uint32_t UbseGetUInt(const std::string& section, const std::string& configKey, uint32_t& configValue)
{
    if (configKey == "log.max.fileSize") {
        configValue = 30;
    } else if (configKey == "log.fileNums") {
        configValue = 20;
    } else if (configKey == "log.queue.maxItem") {
        configValue = 1024;
    }
    return 0;
}

uint32_t UbseGetFloat(const std::string& section, const std::string& configKey, float& configValue)
{
    return 0;
}

uint32_t UbseGetStr(const std::string& section, const std::string& configKey, std::string& configValue)
{
    if (configKey == "nodeId") {
        configValue = "NODE0";
    } else if (configKey == "log.level") {
        configValue = "DEBUG";
    }
    return 0;
}

uint32_t UbseGetBool(const std::string& section, const std::string& configKey, bool& configValue)
{
    return 0;
}

uint32_t UbseGetULong(const std::string& section, const std::string& configKey, uint64_t& configValue)
{
    return 0;
}

bool UbseIsUbFeatureSupported(uint64_t featureMask)
{
    return false;
}

bool UbseIsUrmaSupported()
{
    return false;
}

bool UbseIsMemBorrowNcSupported()
{
    return false;
}

bool UbseIsMemBorrowCcSupported()
{
    return false;
}

bool UbseIsMemShareNcSupported()
{
    return false;
}

bool UbseIsMemShareCcSupported()
{
    return false;
}

bool UbseIsMemBorrowSupported()
{
    return false;
}

bool UbseIsMemShareSupported()
{
    return false;
}

bool UbseIsMemSupported()
{
    return false;
}

uint32_t UbseGetUBEnable(bool& ubEnable)
{
    ubEnable = false;
    return 0;
}

void RackGetConfigEventId(const std::string& section, std::string& eventId)
{
    return;
}
} // namespace ubse::config
