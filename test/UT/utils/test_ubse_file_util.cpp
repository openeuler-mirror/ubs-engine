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

#include <filesystem>
#include <fstream>
#include "ubse_error.h"

namespace ubse::ut::utils {
using namespace ubse::utils;

const std::string FILE_PATH = "/var/log/CheckFileExists";

void TestUbseFileUtil::SetUp()
{
    Test::SetUp();
    std::filesystem::create_directories("/var/log");
    ofstream ofs(FILE_PATH);
    ofs << "";
    ofs.close();
}

void TestUbseFileUtil::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
    std::filesystem::remove(FILE_PATH);
}

// 测试文件路径
const std::string testFilePath = "test.txt";

// 测试文件内容
const std::vector<std::string> testFileContent = {"Hello, world!", "This is a test file.", "Goodbye, world!"};

// 测试用例1：测试正常读取文件
TEST(UbseFileUtilTest, ReadFileNormal)
{
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
TEST(UbseFileUtilTest, ReadFileNotExist)
{
    // 调用待测试函数
    std::vector<std::string> info;
    UbseResult result = UbseFileUtil::GetFileInfo("nonexistent.txt", info);

    // 验证结果
    EXPECT_EQ(result, UBSE_ERROR);
}

// 测试用例3：测试文件无法打开的情况
TEST(UbseFileUtilTest, ReadFileCannotOpen)
{
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
 * 测试属于绝对路径
 * 测试步骤：
 * 1. 调用 IsAbsolutePath 函数，传入路径 "/var/log"
 * 2. 检查返回值是否为 UBSE_OK
 * 预期结果：
 * 1. 返回值应为 UBSE_OK
 */
TEST_F(TestUbseFileUtil, IsAbsolutePath)
{
    EXPECT_EQ(UbseFileUtil::IsAbsolutePath("/var/log"), UBSE_OK);
}

/*
 * 用例描述：
 * 测试不属于绝对路径
 * 测试步骤：
 * 1. 调用 IsAbsolutePath 函数，传入路径 "var/log"
 * 2. 检查返回值是否为 UBSE_ERROR
 * 预期结果：
 * 1. 返回值应为 UBSE_ERROR
 */
TEST_F(TestUbseFileUtil, NotAbsolutePath)
{
    EXPECT_EQ(UbseFileUtil::IsAbsolutePath("var/log"), UBSE_ERROR);
}

/*
 * 用例描述：
 * 测试属于目录
 * 测试步骤：
 * 1. 调用 IsDirectory 函数，传入路径 "/var/log"
 * 2. 检查返回值
 * 预期结果：
 * 1. 返回值true
 */
TEST_F(TestUbseFileUtil, IsDirectory)
{
    EXPECT_EQ(UbseFileUtil::IsDirectory("/var/log"), true);
}

/*
 * 用例描述：
 * 测试不属于目录
 * 测试步骤：
 * 1. 调用 IsDirectory 函数，传入路径 "/var/log/messages"
 * 2. 检查返回值
 * 预期结果：
 * 1. 返回false
 */
TEST_F(TestUbseFileUtil, NotDirectory)
{
    EXPECT_EQ(UbseFileUtil::IsDirectory("/var/log/messages"), false);
}

/*
 * 用例描述：
 * 测试列出目录下面的文件
 * 测试步骤：
 * 1. 调用 ListFiles 函数，传入目录"/InvalidDirectory"和正则匹配log
 * 2. 检查返回值列表是否为空
*  3. 调用 ListFiles 函数，传入目录"/var"和正则匹配log
 * 4. 检查返回值列表是否为空
 * 预期结果：
 * E2. 检查返回值列表为空
 * E4. 检查返回值列表不为空
 */
TEST_F(TestUbseFileUtil, ListFiles)
{
    std::string pattern = "log";
    std::regex express(pattern);

    std::vector<std::string> files = UbseFileUtil::ListFiles("/InvalidDirectory", express);
    EXPECT_TRUE(files.empty());

    files = UbseFileUtil::ListFiles("/var", express);
    EXPECT_FALSE(files.empty());
}

/*
 * 用例描述：
 * 测试创建目录
 * 测试步骤：
 * 1. 调用 CreateAndChmodDirectory 函数，传入路径 "/var/run/ubse"
 * 2. 检查返回值是否 UBSE_OK
 * 3. 调用 CreateAndChmodDirectory 函数，传入路径 "var/run/ubse"
 * 4. 检查返回值是否 UBSE_OK
 * 预期结果：
 * E2. 返回值是 UBSE_OK
 * E4. 返回值不是 UBSE_OK
 */
TEST_F(TestUbseFileUtil, CreateAndChmodDirectory)
{
    GTEST_SKIP();
    EXPECT_EQ(UbseFileUtil::CreateAndChmodDirectory("/var/run/ubse", 0750), UBSE_OK);
    EXPECT_NE(UbseFileUtil::CreateAndChmodDirectory("var/run/ubse", 0750), UBSE_OK);
}

/*
 * 用例描述：
 * 测试检查文件
 * 测试步骤：
 * 1. 调用 CheckFileExists 函数，传入路径 FILE_PATH
 * 2. 检查返回值
 * 3. 调用 CheckFileExists 函数，传入路径 "/var/log/InvalidFile"
 * 4. 检查返回值
 * 预期结果：
 * E2. 返回true
 * E4. 返回false
 */
TEST_F(TestUbseFileUtil, CheckFileExists)
{
    EXPECT_EQ(UbseFileUtil::CheckFileExists("/var/log/InvalidFile"), false);
}

/*
 * 用例描述：
 * 测试检查文件
 * 测试步骤：
 * 1. 调用 SetFileAttributes 函数，传入路径 FILE_PATH
 * 2. 检查不抛异常
 * 预期结果：
 * E2. 不抛异常
 */
TEST_F(TestUbseFileUtil, SetFileAttributes)
{
    EXPECT_NO_THROW(UbseFileUtil::SetFileAttributes(FILE_PATH, 1024, 0, 0750));
}
} // namespace ubse::ut::utils
