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
#include "test_mem_instance_inner.h"
#include "mem_instance_inner.h"
#define private public
#include "ubse_obmm_executor.h"
#undef private

namespace ubse::ut::mmi {
using namespace ubse::mmi;

TEST_F(TestMemInstanceInner, ExportRollback_Success)
{
    RmObmmExecutor::GetInstance().obmmUnexportFunc = [](mem_id id, unsigned long flags) {
        return 0;
    };
    EXPECT_NO_THROW(MemInstanceInner::GetInstance().RollbackExport({1, 2, 3, 4, 5}));
}

TEST_F(TestMemInstanceInner, ImportRollback_Success)
{
    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](mem_id id, unsigned long flags) {
        return 0;
    };
    EXPECT_NO_THROW(MemInstanceInner::GetInstance().RollbackImport({1, 2, 3, 4, 5}));
}
}