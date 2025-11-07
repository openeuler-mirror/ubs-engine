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

#include "test_ubse_file_util.h"

#include <fstream>

#include "ubse_error.h"

namespace ubse::ut::utils {
using namespace ubse::utils;

void TestUbseFileUtil::SetUp()
{
    Test::SetUp();
}

void TestUbseFileUtil::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// 测试文件路径
const std::string testFilePath = "test.txt";

// 测试文件内容
const std::vector<std::string> testFileContent = {
    "Hello, world!",
    "This is a test file.",
    "Goodbye, world!"
};

// 测试用例1：测试正常读取文件
TEST(UbseFileUtilTest, ReadFileNormal) {
    // 创建测试文件
    std::ofstream testFile(testFilePath);
    for (const auto& line : testFileContent) {
        testFile << line << std::endl;
    }
    testFile.close();

    // 调用待测试函数
    std::vector<std::string> info;
    UbseResult result = UbseFileUtil::GetFileInfo(testFilePath, info);

    // 验证结果
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(info, testFileContent);

    // 删除测试文件
    std::remove(testFilePath.c_str());
}

// 测试用例2：测试文件不存在的情况
TEST(UbseFileUtilTest, ReadFileNotExist) {
    // 调用待测试函数
    std::vector<std::string> info;
    UbseResult result = UbseFileUtil::GetFileInfo("nonexistent.txt", info);

    // 验证结果
    EXPECT_EQ(result, UBSE_ERROR);
}

// 测试用例3：测试文件无法打开的情况
TEST(UbseFileUtilTest, ReadFileCannotOpen) {
    // 创建一个无法打开的文件
    std::ofstream testFile(testFilePath);
    testFile << "This file cannot be opened." << std::endl;
    testFile.close();
    std::remove(testFilePath.c_str());

    // 调用待测试函数
    std::vector<std::string> info;
    UbseResult result = UbseFileUtil::GetFileInfo(testFilePath, info);

    // 验证结果
    EXPECT_EQ(result, UBSE_ERROR);
}

/*
 * 用例描述：
 * 测试无效路径的文件读取
 * 测试步骤：
 * 1. 使用一个不存在的文件路径
 * 2. 调用 GetFileInfo 函数读取文件内容
 * 预期结果：
 * 1. 返回值为 UBSE_ERROR
 */
TEST_F(TestUbseFileUtil, InvalidPath)
{
    string path = "invalid_path.txt";
    vector<string> info;

    EXPECT_EQ(UbseFileUtil::GetFileInfo(path, info), UBSE_ERROR);
}

/*
 * 用例描述：
 * 测试有效路径匹配成功
 * 测试步骤：
 * 1. 调用 IsSpecifiedPath 函数，传入有效路径 "/valid/path" 和正则表达式 "^/valid/path$"
 * 2. 检查返回值是否为 UBSE_OK
 * 预期结果：
 * 1. 返回值应为 UBSE_OK
 */
TEST_F(TestUbseFileUtil, MatchValidPath)
{
    EXPECT_EQ(UbseFileUtil::IsSpecifiedPath("/valid/path", R"(^/valid/path$)"), UBSE_OK);
}

/*
 * 用例描述：
 * 测试带有通配符的有效路径匹配成功
 * 测试步骤：
 * 1. 调用 IsSpecifiedPath 函数，传入路径 "/valid/path/extra" 和正则表达式 "^/valid/path/.*$"
 * 2. 检查返回值是否为 UBSE_OK
 * 预期结果：
 * 1. 返回值应为 UBSE_OK
 */
TEST_F(TestUbseFileUtil, MatchValidPathWithWildcard)
{
    EXPECT_EQ(UbseFileUtil::IsSpecifiedPath("/valid/path/extra", R"(^/valid/path/.*$)"), UBSE_OK);
}

/*
 * 用例描述：
 * 测试无效路径匹配失败
 * 测试步骤：
 * 1. 调用 IsSpecifiedPath 函数，传入无效路径 "/invalid/path" 和正则表达式 "^/valid/path$"
 * 2. 检查返回值是否为 UBSE_ERROR
 * 预期结果：
 * 1. 返回值应为 UBSE_ERROR
 */
TEST_F(TestUbseFileUtil, NoMatchInvalidPath)
{
    EXPECT_EQ(UbseFileUtil::IsSpecifiedPath("/invalid/path", R"(^/valid/path$)"), UBSE_ERROR);
}

/*
 * 用例描述：
 * 测试空路径匹配失败
 * 测试步骤：
 * 1. 调用 IsSpecifiedPath 函数，传入空路径 "" 和正则表达式 "^/valid/path$"
 * 2. 检查返回值是否为 UBSE_ERROR
 * 预期结果：
 * 1. 返回值应为 UBSE_ERROR
 */
TEST_F(TestUbseFileUtil, NoMatchEmptyPath)
{
    EXPECT_EQ(UbseFileUtil::IsSpecifiedPath("", R"(^/valid/path$)"), UBSE_ERROR);
}

/*
 * 用例描述：
 * 测试带有空格的路径匹配失败
 * 测试步骤：
 * 1. 调用 IsSpecifiedPath 函数，传入路径 " /valid/path " 和正则表达式 "^/valid/path$"
 * 2. 检查返回值是否为 UBSE_ERROR
 * 预期结果：
 * 1. 返回值应为 UBSE_ERROR
 */
TEST_F(TestUbseFileUtil, NoMatchPathWithSpaces)
{
    EXPECT_EQ(UbseFileUtil::IsSpecifiedPath(" /valid/path ", R"(^/valid/path$)"), UBSE_ERROR);
}

/*
 * 用例描述：
 * 测试根路径匹配成功
 * 测试步骤：
 * 1. 调用 IsSpecifiedPath 函数，传入根路径 "/" 和正则表达式 "^/$"
 * 2. 检查返回值是否为 UBSE_OK
 * 预期结果：
 * 1. 返回值应为 UBSE_OK
 */
TEST_F(TestUbseFileUtil, MatchRootPath)
{
    EXPECT_EQ(UbseFileUtil::IsSpecifiedPath("/", R"(^/$)"), UBSE_OK);
}

/*
 * 用例描述：
 * 测试包含特殊字符的有效路径匹配成功
 * 测试步骤：
 * 1. 调用 IsSpecifiedPath 函数，传入路径 "/valid/path/with_special_chars_!@#$%^&*" 和正则表达式
 * "^/valid/path/with_special_chars_.*$"
 * 2. 检查返回值是否为 UBSE_OK
 * 预期结果：
 * 1. 返回值应为 UBSE_OK
 */
TEST_F(TestUbseFileUtil, MatchPathWithSpecialCharacters)
{
    EXPECT_EQ(UbseFileUtil::IsSpecifiedPath("/valid/path/with_special_chars_!@#$%^&*",
        R"(^/valid/path/with_special_chars_.*$)"),
        UBSE_OK);
}

/*
 * 用例描述：
 * 测试包含特殊字符的无效路径匹配失败
 * 测试步骤：
 * 1. 调用 IsSpecifiedPath 函数，传入路径 "/invalid/path/with_special_chars_!@#$%^&*" 和正则表达式 "^/valid/path/.*$"
 * 2. 检查返回值是否为 UBSE_ERROR
 * 预期结果：
 * 1. 返回值应为 UBSE_ERROR
 */
TEST_F(TestUbseFileUtil, NoMatchPathWithSpecialCharacters)
{
    EXPECT_EQ(UbseFileUtil::IsSpecifiedPath("/invalid/path/with_special_chars_!@#$%^&*", R"(^/valid/path/.*$)"),
        UBSE_ERROR);
}

TEST_F(TestUbseFileUtil, GetLibDir_ExecutablePath)
{
    std::string result = UbseFileUtil::GetLibDir();
    ASSERT_FALSE(result.empty());
}
}
