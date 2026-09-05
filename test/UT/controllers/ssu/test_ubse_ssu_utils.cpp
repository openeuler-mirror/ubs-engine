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

// =============== IsValidHostNqn 测试用例 ===============

/*
 * 用例描述：IsValidHostNqn 接受标准 UUID 型 NQN
 * 测试步骤：
 * 1、调用 GenerateHostNqn 获得合法 NQN
 * 预期结果：
 * 1、IsValidHostNqn 返回 true
 */
TEST_F(TestUbseSsuUtils, IsValidHostNqn_ValidUuidNqn)
{
    auto nqn = GenerateHostNqn();
    EXPECT_TRUE(IsValidHostNqn(nqn));
}

/*
 * 用例描述：IsValidHostNqn 接受非 UUID 通用 NQN（NVMe 规范允许的域名风格）
 * 测试步骤：
 * 1、传入 com.vendor 域名风格 NQN
 * 预期结果：
 * 1、返回 true
 */
TEST_F(TestUbseSsuUtils, IsValidHostNqn_ValidGenericNqn)
{
    EXPECT_TRUE(IsValidHostNqn("nqn.2014-08.com.vendor"));
    EXPECT_TRUE(IsValidHostNqn("nqn.2014-08.com.vendor:node1"));
    EXPECT_TRUE(IsValidHostNqn("nqn.2014-08.org.nvmexpress"));
    EXPECT_TRUE(IsValidHostNqn("nqn.2014-08.com.example:storage:array1"));
}

/*
 * 用例描述：IsValidHostNqn 接受 UUID 大写 hex
 * 测试步骤：
 * 1、传入 UUID 为大写的 NQN
 * 预期结果：
 * 1、返回 true（hex 校验不区分大小写）
 */
TEST_F(TestUbseSsuUtils, IsValidHostNqn_UuidUppercase)
{
    EXPECT_TRUE(IsValidHostNqn(
        "nqn.2024-01.org.nvmexpress:uuid:123E4567-E89B-12D3-A456-426614174000"));
}

/*
 * 用例描述：IsValidHostNqn 拒绝空字符串
 * 测试步骤：
 * 1、传入空串
 * 预期结果：
 * 1、返回 false
 */
TEST_F(TestUbseSsuUtils, IsValidHostNqn_Empty)
{
    EXPECT_FALSE(IsValidHostNqn(""));
}

/*
 * 用例描述：IsValidHostNqn 拒绝缺少 nqn. 前缀的字符串
 * 测试步骤：
 * 1、传入不以 nqn. 开头的字符串
 * 预期结果：
 * 1、返回 false
 */
TEST_F(TestUbseSsuUtils, IsValidHostNqn_NoPrefix)
{
    EXPECT_FALSE(IsValidHostNqn("2024-01.org.nvmexpress:uuid:12345678-1234-1234-1234-1234567890ab"));
    EXPECT_FALSE(IsValidHostNqn("nqn2024-01.org.nvmexpress:uuid:12345678-1234-1234-1234-1234567890ab"));
}

/*
 * 用例描述：IsValidHostNqn 拒绝年份段非数字
 * 测试步骤：
 * 1、传入年份段含字母的 NQN
 * 预期结果：
 * 1、返回 false
 */
TEST_F(TestUbseSsuUtils, IsValidHostNqn_YearNonDigit)
{
    EXPECT_FALSE(IsValidHostNqn("nqn.20ab-01.org.nvmexpress:uuid:12345678-1234-1234-1234-1234567890ab"));
    EXPECT_FALSE(IsValidHostNqn("nqn.20-01.org.nvmexpress:uuid:12345678-1234-1234-1234-1234567890ab"));
}

/*
 * 用例描述：IsValidHostNqn 拒绝月份越界
 * 测试步骤：
 * 1、传入月份 00 的 NQN
 * 2、传入月份 13 的 NQN
 * 预期结果：
 * 1、返回 false
 * 2、返回 false
 */
TEST_F(TestUbseSsuUtils, IsValidHostNqn_MonthOutOfRange)
{
    EXPECT_FALSE(IsValidHostNqn("nqn.2024-00.org.nvmexpress:uuid:12345678-1234-1234-1234-1234567890ab"));
    EXPECT_FALSE(IsValidHostNqn("nqn.2024-13.org.nvmexpress:uuid:12345678-1234-1234-1234-1234567890ab"));
}

/*
 * 用例描述：IsValidHostNqn 拒绝年份早于 2010
 * 测试步骤：
 * 1、传入年份 2009（早于 NVMe 规范发布年份）
 * 预期结果：
 * 1、返回 false
 */
TEST_F(TestUbseSsuUtils, IsValidHostNqn_YearBelowMin)
{
    EXPECT_FALSE(IsValidHostNqn("nqn.2009-01.org.nvmexpress:uuid:12345678-1234-1234-1234-1234567890ab"));
}

/*
 * 用例描述：IsValidHostNqn 拒绝缺少日期分隔符
 * 测试步骤：
 * 1、传入 YYYYMM 不带连字符的 NQN
 * 预期结果：
 * 1、返回 false
 */
TEST_F(TestUbseSsuUtils, IsValidHostNqn_MissingDateHyphen)
{
    EXPECT_FALSE(IsValidHostNqn("nqn.202401.org.nvmexpress:uuid:12345678-1234-1234-1234-1234567890ab"));
}

/*
 * 用例描述：IsValidHostNqn 拒绝 UUID 包含非 hex 字符
 * 测试步骤：
 * 1、传入 UUID 含 'g' 的 NQN
 * 预期结果：
 * 1、返回 false
 */
TEST_F(TestUbseSsuUtils, IsValidHostNqn_UuidNonHex)
{
    EXPECT_FALSE(IsValidHostNqn(
        "nqn.2024-01.org.nvmexpress:uuid:12345678-1234-1234-1234-1234567890ag"));
}

/*
 * 用例描述：IsValidHostNqn 拒绝 UUID 长度偏差
 * 测试步骤：
 * 1、传入 UUID 少一字符的 NQN
 * 2、传入 UUID 多一字符的 NQN
 * 预期结果：
 * 1、返回 false
 * 2、返回 false
 */
TEST_F(TestUbseSsuUtils, IsValidHostNqn_UuidLengthOffByOne)
{
    // UUID 少末尾一字符
    EXPECT_FALSE(IsValidHostNqn(
        "nqn.2024-01.org.nvmexpress:uuid:12345678-1234-1234-1234-1234567890a"));
    // UUID 多末尾一字符
    EXPECT_FALSE(IsValidHostNqn(
        "nqn.2024-01.org.nvmexpress:uuid:12345678-1234-1234-1234-1234567890abc"));
}

/*
 * 用例描述：IsValidHostNqn 拒绝 UUID 缺少连字符
 * 测试步骤：
 * 1、传入 UUID 无连字符的 NQN
 * 预期结果：
 * 1、返回 false
 */
TEST_F(TestUbseSsuUtils, IsValidHostNqn_UuidMissingHyphen)
{
    EXPECT_FALSE(IsValidHostNqn(
        "nqn.2024-01.org.nvmexpress:uuid:123456781234123412341234567890ab"));
}

/*
 * 用例描述：IsValidHostNqn 拒绝反向域名含非法字符
 * 测试步骤：
 * 1、传入域名含下划线的 NQN
 * 2、传入域名含空格的 NQN
 * 预期结果：
 * 1、返回 false
 * 2、返回 false
 */
TEST_F(TestUbseSsuUtils, IsValidHostNqn_InvalidDomainChars)
{
    EXPECT_FALSE(IsValidHostNqn("nqn.2024-01.com.vendor_node"));
    EXPECT_FALSE(IsValidHostNqn("nqn.2024-01.com.vendor node"));
}

/*
 * 用例描述：IsValidHostNqn 拒绝 UUID 分组位置错误
 * 测试步骤：
 * 1、传入 UUID 连字符位置偏移的 NQN
 * 预期结果：
 * 1、返回 false
 */
TEST_F(TestUbseSsuUtils, IsValidHostNqn_UuidWrongHyphenPos)
{
    EXPECT_FALSE(IsValidHostNqn(
        "nqn.2024-01.org.nvmexpress:uuid:1234567-81234-1234-1234-1234567890ab"));
}

/*
 * 用例描述：IsValidHostNqn 拒绝不同反向域名下的非法 UUID
 * 测试步骤：
 * 1、传入 com.example:uuid: 下含非法 UUID 的 NQN
 * 预期结果：
 * 1、返回 false（:uuid: 子域在任何反向域名下均触发 UUID 校验）
 */
TEST_F(TestUbseSsuUtils, IsValidHostNqn_NonOrgUuidInvalid)
{
    EXPECT_FALSE(IsValidHostNqn(
        "nqn.2024-01.com.example:uuid:invalid-uuid-1234-5678"));
}

/*
 * 用例描述：IsValidHostNqn 拒绝超长 NQN（超过序列化层上限 UBSE_SSU_MAX_NQN_LENGTH=69 含 \0）
 * 测试步骤：
 * 1、传入合法但总长 68 字符的 NQN（边界值，应在序列化层上限内）
 * 2、传入合法但总长 69 字符的 NQN（超出序列化层上限）
 * 预期结果：
 * 1、返回 true（68 < 69，在有效范围内）
 * 2、返回 false（69 >= 69，超出序列化层上限）
 */
TEST_F(TestUbseSsuUtils, IsValidHostNqn_ExceedsMaxLength)
{
    // 68 字符：nqn.2014-08. + 56 个 'a' = 68 总长，在 UBSE_SSU_MAX_NQN_LENGTH - 1 范围内
    // 对应有效字符上限 68 的边界值
    std::string validLongNqn = "nqn.2014-08." + std::string(56, 'a');
    EXPECT_EQ(validLongNqn.size(), 68);
    EXPECT_TRUE(IsValidHostNqn(validLongNqn));

    // 69 字符：nqn.2014-08. + 57 个 'a' = 69 总长，超出序列化层上限（含 \0）
    std::string invalidLongNqn = "nqn.2014-08." + std::string(57, 'a');
    EXPECT_EQ(invalidLongNqn.size(), 69);
    EXPECT_FALSE(IsValidHostNqn(invalidLongNqn));
}
} // namespace ubse::ssu::utils::ut
