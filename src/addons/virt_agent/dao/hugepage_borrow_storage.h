/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

#ifndef HUGE_PAGE_BORROW_STORAGE_H
#define HUGE_PAGE_BORROW_STORAGE_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <ubse_storage.h>

#include "vm_error.h"
#include "vm_lock.h"
#include "vm_struct.h"

namespace vm {
using namespace ubse::storage;

// Persist the hugepage size that each borrowId is split into.
class HugePageBorrowStorage {
public:
    static VmResult SaveBorrowPageSizes(const std::vector<std::string>& borrowIds, const uint64_t& pageSize);
    static VmResult DeleteBorrowPageSizes(const std::vector<std::string>& borrowIds);
    static VmResult GetAllBorrowPageSizes(std::unordered_map<std::string, uint64_t>& borrowPageSizes);
    static void QueryHandler(const std::string& keyPrefix, const std::string& key, const UbseByteBuffer& buff,
                             void* ctx);

private:
    static VmResult LoadRecords(std::vector<HugePageBorrowRecord>& records);
    static VmResult SaveRecords(const std::vector<HugePageBorrowRecord>& records);
    static ReadWriteLock hugePageBorrowLock;
};
} // namespace vm

#endif // HUGE_PAGE_BORROW_STORAGE_H
