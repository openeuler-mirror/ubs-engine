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

#ifndef MP_VM_QUOTA_UTIL_H
#define MP_VM_QUOTA_UTIL_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "../../interface/mempooling_interface.h"

namespace mempooling {

class MpVmQuotaUtil {
public:
    static std::string GetVmNameFromPid(pid_t pid);

    static std::map<int, uint64_t> ParseNumatuneXml(const std::string& xmlStr);

    static uint32_t ValidateNumaQuota(pid_t pid,
                                      const std::vector<mempooling::outinterface::PageSwapPair>& pageSwapPairs);

private:
    static uint32_t GetVmNumaLimits(pid_t pid, std::map<int, uint64_t>& numaLimits);

    static uint32_t CheckQuotaExceedsLimit(const std::vector<mempooling::outinterface::PageSwapPair>& pageSwapPairs,
                                           const std::map<int, uint64_t>& numaLimits);
};

} // namespace mempooling

#endif // MP_VM_QUOTA_UTIL_H