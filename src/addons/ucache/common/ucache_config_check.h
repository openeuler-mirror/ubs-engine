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

#ifndef UCACHE_CONFIG_CHECK_H
#define UCACHE_CONFIG_CHECK_H

#include <map>
#include <string>

namespace ucache {

template <typename T>
struct RangeCheck {
    T defaultValue;
    T minValue;
    T maxValue;
    bool critical = false;
};

// 配置项范围检查。具体说明，见 plugin_ucache.conf
const std::map<std::string, RangeCheck<uint32_t>> Uint32RangeCheck = {
    {"bottleneck.threshold",
     {
         .defaultValue = 10240,
         .minValue = 1,
         .maxValue = 10 * 1024 * 1024,
     }},
    {"bottleneck.short.size",
     {
         .defaultValue = 180,
         .minValue = 30,
         .maxValue = 1200,
     }},
    {"bottleneck.short.threshold",
     {
         .defaultValue = 70,
         .minValue = 0,
         .maxValue = 100,
     }},
    {"bottleneck.long.size",
     {
         .defaultValue = 600,
         .minValue = 90,
         .maxValue = 3600,
     }},
    {"bottleneck.long.threshold",
     {
         .defaultValue = 30,
         .minValue = 0,
         .maxValue = 100,
     }},
    {"strategy.maxReliableTimes",
     {
         .defaultValue = 3,
         .minValue = 1,
         .maxValue = 10,
     }},
};

} // namespace ucache
#endif
