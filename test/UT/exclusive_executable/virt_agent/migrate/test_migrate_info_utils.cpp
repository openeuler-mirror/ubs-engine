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

#include "test_migrate_info_utils.h"
#include <mockcpp/mockcpp.hpp>
#include <ubse_com.h>

#include "migrate_info_utils.h"
#include "vm_error.h"
#include "response_info_message.h"

using namespace vm;
using namespace ubse::com;
namespace ubse::vm::ut {
void TestMigrateInfoUtils::SetUp()
{
    Test::SetUp();
}

void TestMigrateInfoUtils::TearDown()
{
    Test::TearDown();
}

TEST_F(TestMigrateInfoUtils, GetPidByVmUUID_ShouldReturnError_WhenUuidNotFound)
{
    pid_t pid = MigrateInfoUtil::GetPidByVmUUID("nonexistent-uuid");
    EXPECT_EQ(pid, -1);
}

TEST_F(TestMigrateInfoUtils, GetNumaIdAndPageSizeByPid_ShouldReturnError_WhenNumaInfoNotFound)
{
    MigrateInfoBase numaInfo{};
    VmResult result = MigrateInfoUtil::GetNumaIdAndPageSizeByPid(1, numaInfo);
    EXPECT_EQ(result, VM_ERROR);
}

constexpr uint32_t invalidNumaId = 1234;

TEST_F(TestMigrateInfoUtils, GetSocketIdByNumaId_ShouldReturnERROR_WhenSocketIdNotFound)
{
    uint32_t socketId = 0;
    VmResult result = MigrateInfoUtil::GetSocketIdByNumaId(invalidNumaId, &socketId);
    EXPECT_EQ(result, VM_WARN);
}

TEST_F(TestMigrateInfoUtils, GetSocketIdByNumaId_ShouldReturnOK_WhenSocketIdFound)
{
    uint32_t socketId = 0;
    VmResult result = MigrateInfoUtil::GetSocketIdByNumaId(0, &socketId);
    EXPECT_EQ(result, VM_OK);
}

}  // namespace ubse::vm::ut