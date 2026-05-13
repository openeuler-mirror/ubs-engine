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

#ifndef MEMPOOLING_COLLECT_UTIL_H
#define MEMPOOLING_COLLECT_UTIL_H

#include <unistd.h>
#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "mempooling_interface.h"
#include "mp_error.h"
#include "numa_info.h"

namespace mempooling::over_commit {
class CollectUtil {
public:
    static MpResult GetRemoteVmPids(const std::string& nodeID, const std::vector<uint32_t>& remoteNumaIds,
                                    std::unordered_map<std::uint16_t, std::vector<pid_t>>& res);

    static MpResult GetRemoteVmPidsByLocal(const std::vector<uint32_t>& remoteNumaIds,
                                           std::unordered_map<std::uint16_t, std::vector<pid_t>>& res,
                                           bool isReturn = false);

    static MpResult GetNumaMemInfos(const std::string& nodeId, const std::set<int16_t>& numaIds,
                                    std::map<int, mempooling::NumaMetaData>& numaMemInfos);
};
} // namespace mempooling::over_commit
#endif // MEMPOOLING_COLLECT_UTIL_H
