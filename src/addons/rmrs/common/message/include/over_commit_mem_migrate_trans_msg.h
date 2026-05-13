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

#ifndef OVER_COMMIT_MEM_MIGRATE_H
#define OVER_COMMIT_MEM_MIGRATE_H

#include <utility>
#include <vector>

#include "common_delete_func.h"
#include "mempooling_interface.h"
#include "over_commit_def.h"
#include "rmrs_serialize.h"
namespace mempooling {
using namespace mempooling::over_commit;
using rmrs::serialize::RmrsInStream;
using rmrs::serialize::RmrsOutStream;

struct OverCommitMemMigrateTrans {
    outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam;
    std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs;
    std::vector<MemMigrateResult> memMigrateResults;
};

class OverCommitMemMigrateTransMsg {
public:
    OverCommitMemMigrateTransMsg() = default;

    explicit OverCommitMemMigrateTransMsg(OverCommitMemMigrateTrans overCommitMemMigrateTrans)
        : _overCommitMemMigrateTrans(std::move(overCommitMemMigrateTrans))
    {
    }

    inline OverCommitMemMigrateTrans GetOverCommitMemMigrateTrans() const
    {
        return _overCommitMemMigrateTrans;
    }
    OverCommitMemMigrateTrans _overCommitMemMigrateTrans{};
};
} // namespace mempooling
#endif // OVER_COMMIT_MEM_MIGRATE_H
