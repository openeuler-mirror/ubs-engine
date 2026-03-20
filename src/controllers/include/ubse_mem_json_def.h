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

#ifndef MEM_STRATEGY_RACK_MEM_JSON_DEF_H
#define MEM_STRATEGY_RACK_MEM_JSON_DEF_H

#include <cstdint>
#include <map>
#include <string>

#include "ubse_mem_constants.h"
#include "ubse_mmi_interface.h"

namespace ubse::mem::strategy {
using namespace ubse::adapter_plugins::mmi;
// 事件id：RegisterMemEventNotifyFunc_high
// 事件id：RegisterMemEventNotifyFunc_low
struct UbseMemEventNotifyBorrowItem {
    std::string ToJson();
    uint16_t exportLocNum{};                     /* 提供内存的numa数量 */
    std::string name;                            /* 资源的name */
    std::vector<UbseMemNumaLoc> exportLocInfo{}; /* 提供内存的numa位置, 应该不超过[MEM_MAX_NUMA_NUM_PER_ITEM]
                                                  * 单次借用最多是单个节点一个P上的两个numa */
    std::vector<uint64_t> requestSize{};         /* 每个numa提供的内存大小应该不超过[MEM_MAX_NUMA_NUM_PER_ITEM]  */
    uint64_t exportMemId{};                      /* 提供内存节点导出内存obmm生成的memid */
    uint64_t importMemId{};                      /* 本地导入obmm生成的memid */
};

// 高低水线后，发现的消息结构如下，需要转成json格式
struct UbseMemEventNotifyMessage {
    std::string ToJson();
    int oomEventFlag{};                          /* oomEventFlag为1时说明发生了OOM事件 */
    int borrowItemNum{};                         /* 该numa借入账目的实际条数 */
    std::string allNumaInfo{};                   /* 所有numa的统计信息 */
    UbseMemNumaLoc notifyNumaLoc{};              /* 借入numa位置，如果是高水线，需要增加账本，低水线，需要减少账本 */
    std::vector<UbseMemEventNotifyBorrowItem>
        borrowItem; /* 如果是低水线事件，可以通过选择一条账本归还，告警numa层级借入的所有账本 */
};


} // namespace ubse::mem::strategy

#endif // RACK_MEM_JSON_DEF_H