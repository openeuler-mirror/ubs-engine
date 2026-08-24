/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_ssu_utils.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace ubse::ssu::utils::ut {

using namespace ubse::ssu::utils;

void TestUbseSsuUtils::SetUp()
{
    Test::SetUp();
}

void TestUbseSsuUtils::TearDown()
{
    Test::TearDown();
}

/*
 * 用例描述：GenerateHostNqn 返回格式正确
 * 测试步骤：
 * 1、调用 GenerateHostNqn
 * 预期结果：
 * 1、以 "nqn." 开头
 * 2、包含 ".org.nvmexpress:uuid:"
 * 3、uuid 部分为 8-4-4-4-12 的 uuid 标准格式
 */
TEST_F(TestUbseSsuUtils, GenerateHostNqnFormat)
{
    auto result = GenerateHostNqn();

    EXPECT_EQ(result.substr(0, 4), "nqn.");
    EXPECT_NE(result.find(".org.nvmexpress:uuid:"), std::string::npos);

    auto uuidPos = result.find(".org.nvmexpress:uuid:");
    auto uuid = result.substr(uuidPos + 21); // ".org.nvmexpress:uuid:" 长度为 21
    // uuid 格式：xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    EXPECT_EQ(uuid.size(), 36u);
    EXPECT_EQ(uuid[8], '-');
    EXPECT_EQ(uuid[13], '-');
    EXPECT_EQ(uuid[18], '-');
    EXPECT_EQ(uuid[23], '-');
}

/*
 * 用例描述： GenerateHostNqn 多次调用返回不同结果（uuid 随机）
 * 测试步骤：
 * 1、连续调用 10 次 GenerateHostNqn
 * 预期结果：
 * 1、每次结果唯一
 */
TEST_F(TestUbseSsuUtils, GenerateHostNqnUnique)
{
    std::unordered_set<std::string> results;
    constexpr int COUNT = 10;
    for (int i = 0; i < COUNT; ++i) {
        results.insert(GenerateHostNqn());
    }
    EXPECT_EQ(results.size(), static_cast<size_t>(COUNT));
}

/*
 * 用例描述：GenerateHostNqn 日期部分为当前年月
 * 测试步骤：
 * 1、调用 GenerateHostNqn
 * 预期结果：
 * 1、格式为 nqn.YYYY-MM.org.nvmexpress:uuid:...
 */
TEST_F(TestUbseSsuUtils, GenerateHostNqnDatePart)
{
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream oss;
    oss << "nqn." << std::put_time(&tm, "%Y-%m") << ".org.nvmexpress:uuid:";

    auto result = GenerateHostNqn();
    EXPECT_EQ(result.substr(0, oss.str().size()), oss.str());
}

/*
 * 用例描述： GetSsuExecutor 在未初始化上下文中返回 nullptr
 * 测试步骤：
 * 1、直接调用 GetSsuExecutor（无初始化上下文）
 * 预期结果：
 * 1、返回 nullptr
 */
TEST_F(TestUbseSsuUtils, GetSsuExecutorNull)
{
    auto executor = GetSsuExecutor();
    EXPECT_EQ(executor, nullptr);
}
} // namespace ubse::ssu::utils::ut
