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

#ifndef MIGRATION_STRATEGY_H
#define MIGRATION_STRATEGY_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>
#include "bottleneck_strategy.h"
#include "mem_borrow.h"
#include "ucache_error.h"

namespace ucache::master {
namespace migration {
using namespace ucache::mem_borrow;
using ucache::master::bottleneck::PageCacheSensitiveTag;

struct MigrationAction {
    std::string fromNode;
    std::vector<std::string> dockerIds;
    std::string toNode;
    int32_t dstNumaId;
    uint64_t startWatermark;
    uint64_t stopWatermark;
};

std::vector<MigrationAction> MemoryMigrationStrategy(
    const std::map<std::string, std::vector<NodeMemBorrowInfo>>& borrowMap,
    const std::map<std::string, NodeMemoryInfo>& nodes,
    const std::map<std::string, std::map<std::string, PageCacheSensitiveTag>>& nodeTags);
std::string PrintMigrationAction(const std::vector<MigrationAction>& actions);
} // namespace migration
} // namespace ucache::master

#endif