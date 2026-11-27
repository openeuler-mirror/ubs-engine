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

#ifndef HUGEPAGE_HANDLER_H
#define HUGEPAGE_HANDLER_H

#include <string>
#include <unordered_map>
#include <vector>

#include "vm_error.h"
#include "vm_struct.h"

namespace vm {
class HugePageHandler {
public:
    // Reconcile the hugepages of the given NUMA nodes according to the borrow ledger and the persisted records.
    static VmResult ReconcileHugePages(const std::vector<uint16_t>& numaIds);
    // Recover all hugepages at startup: drop the records of the returned borrowIds, then reconcile all NUMA nodes.
    static VmResult RecoverHugePages();
    // Best effort: allocate hugepages for the given borrowIds from the borrow ledger with the given page size.
    // Used when the borrow record cannot be persisted.
    static VmResult AllocateHugePagesForBorrows(const std::vector<std::string>& borrowIds,
                                                const std::vector<uint16_t>& numaIds, uint64_t pageSize);

private:
    static inline const std::string HUGEPAGES_PATH_HEAD = "/sys/devices/system/node/node";
    static constexpr size_t MAX_WRITE_HUGE_PAGE_RETRY_TIME = 2;   // retry only once
    static constexpr uint16_t WRITE_HUGE_PAGE_RETRY_INTERVAL = 1; //  wait 1s

    static VmResult ReconcileHugePagesInternal(const std::vector<uint16_t>& numaIds, bool removeStaleRecords);
    static VmResult ReconcileWithRecords(const std::unordered_map<std::string, uint64_t>& borrowPageSizes,
                                         const std::vector<uint16_t>& numaIds, bool removeStaleRecords);
    static VmResult GetCurrentHugePages(const std::string& realPath, uint64_t& currentHugePages);
    static VmResult AllocateHugePages(const HugePageInfo& pageInfo);
    static VmResult GetHugePageDirName(const uint64_t& pageSize, std::string& dirName);
    static VmResult GetHugePageCanonicalPath(const std::string& numaId, const uint64_t& pageSize,
                                             std::string& filePath);
    static VmResult RewriteHugePages(const std::string& realPath, const uint64_t& targetHugePages);
};
} // namespace vm

#endif // HUGEPAGE_HANDLER_H
