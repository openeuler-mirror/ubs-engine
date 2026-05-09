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

#include "test_os_helper.h"
#include "mockcpp/mockcpp.hpp"
#include "ubse_ut_dir.h"

using namespace vm;
namespace ubse::ut::vm {
// 设置测试环境
void TestOsHelper::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestOsHelper::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestOsHelper, GetPidsByContainerIdsSuccess)
{
    std::string sourcePath = OsHelper::procPathPrefix;
    OsHelper::procPathPrefix = std::string(UT_DIRECTORY) + "/exclusive_executable/virt_agent/export/proc";
    std::unordered_map<std::string, std::vector<pid_t>> containerInfos;
    std::unordered_set<std::string> containerIds = {"3d699eb51dadbdd5aa512b3fdc59caf6a122def920a7a332addccdf99dcd265b"};
    auto ret = OsHelper::GetPidsByContainerIds({containerIds}, containerInfos);
    EXPECT_EQ(ret, VM_OK);
    EXPECT_EQ(1, containerInfos.size());
    EXPECT_EQ(1, containerInfos["3d699eb51dadbdd5aa512b3fdc59caf6a122def920a7a332addccdf99dcd265b"].size());
    EXPECT_EQ(333, containerInfos["3d699eb51dadbdd5aa512b3fdc59caf6a122def920a7a332addccdf99dcd265b"][0]); // 333 pid
    OsHelper::procPathPrefix = sourcePath;
}

TEST_F(TestOsHelper, GetPidsByContainerIdsFailed)
{
    std::string sourcePath = OsHelper::procPathPrefix;
    OsHelper::procPathPrefix = std::string(UT_DIRECTORY) + "/exclusive_executable/virt_agent/export/proc1";
    std::unordered_map<std::string, std::vector<pid_t>> containerInfos;
    std::unordered_set<std::string> containerIds = {"3d699eb51dadbdd5aa512b3fdc59caf6a122def920a7a332addccdf99dcd265b"};
    auto ret = OsHelper::GetPidsByContainerIds({containerIds}, containerInfos);
    EXPECT_EQ(ret, VM_ERROR);

    OsHelper::procPathPrefix = std::string(UT_DIRECTORY) + "/exclusive_executable/virt_agent/export/proc_err";
    ret = OsHelper::GetPidsByContainerIds({containerIds}, containerInfos);
    EXPECT_EQ(ret, VM_OK);
    EXPECT_EQ(0, containerInfos.size());
    OsHelper::procPathPrefix = sourcePath;
}
} // namespace ubse::ut::vm