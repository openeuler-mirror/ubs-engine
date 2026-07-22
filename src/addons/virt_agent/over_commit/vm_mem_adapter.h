/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef VM_MEM_ADAPTER_H
#define VM_MEM_ADAPTER_H

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "vm_error.h"

namespace vm::overcommit {
class VmMemAdapter {
public:
    static VmResult GetMemoryRemoteNumaIds(const std::unordered_set<std::string>& borrowIds,
                                           std::unordered_map<std::string, uint16_t>& borrowIdMaps);
};
} // namespace vm::overcommit
#endif // VM_MEM_ADAPTER_H
