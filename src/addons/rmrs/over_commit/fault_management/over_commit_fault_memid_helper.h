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

#ifndef MEMPOOLING_OVER_COMMIT_FAULT_HELPER_H
#define MEMPOOLING_OVER_COMMIT_FAULT_HELPER_H

#include <iostream>
#include <string>
#include "ubse_common_def.h"
#include "mp_error.h"

namespace mempooling {
using ubse::common::def::UbseResult;

class OverCommitFaultMemIdHelper {
public:
    static OverCommitFaultMemIdHelper& Instance()
    {
        static OverCommitFaultMemIdHelper instance;
        return instance;
    }

    MpResult FaultMemIdManageHelper(std::string importNodeId, uint64_t importMemId);

private:
    OverCommitFaultMemIdHelper() = default;
    ~OverCommitFaultMemIdHelper() = default;
    OverCommitFaultMemIdHelper(const OverCommitFaultMemIdHelper&) = delete;
    OverCommitFaultMemIdHelper& operator=(const OverCommitFaultMemIdHelper&) = delete;
};

} // namespace mempooling
#endif // FAULT_MEMID_HELPER_H