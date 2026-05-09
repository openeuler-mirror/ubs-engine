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

#include "vm_error.h"
#include "vm_struct.h"

namespace vm {
class HugePageHandler {
public:
    static VmResult SetHugePages(const uint64_t &numaId, const uint64_t &borrowedSize);

private:
    static inline const std::string HUGEPAGES_PATH_HEAD = "/sys/devices/system/node/node";
    static inline const std::string HUGEPAGES_PATH_TAIL = "/hugepages/hugepages-2048kB/nr_hugepages";
    static constexpr size_t MAX_WRITE_HUGE_PAGE_RETRY_TIME = 2;   // retry only once
    static constexpr uint16_t WRITE_HUGE_PAGE_RETRY_INTERVAL = 1; //  wait 1s

    static VmResult GetCurrentHugePages(const std::string &realPath, uint64_t &currentHugePages);
    static VmResult AllocateHugePages(const HugePageInfo &pageInfo);
    static VmResult GetHugePageCanonicalPath(const std::string &numaId, std::string &filePath);
    static VmResult RewriteHugePages(const std::string &realPath, const uint64_t &targetHugePages);
};
} // namespace vm

#endif // HUGEPAGE_HANDLER_H
