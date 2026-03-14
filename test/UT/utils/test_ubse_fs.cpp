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

#include "test_ubse_fs.h"

#include <filesystem>
#include "ubse_error.h"
#include "ubse_fs.h"
#include "ubse_ut_dir.h"

namespace ubse::ut::utils {
using namespace ubse::misc::fs;

const std::string UBSE_TEST_DATA_PATH = std::string(UT_DIRECTORY) + "tmp";
constexpr size_t UBSE_DATA_MAX_SIZE = 500 * 1024;

void TestUbseFs::SetUp()
{
    std::filesystem::create_directories(UBSE_TEST_DATA_PATH);
    Test::SetUp();
}

void TestUbseFs::TearDown()
{
    std::filesystem::remove_all(UBSE_TEST_DATA_PATH);
    Test::TearDown();
    GlobalMockObject::verify();
}

/**
 * 用例描述：
 * 测试文件目录不存在时，写文件失败
 * 测试步骤：
 * 1. 未创建对应目录时，调用 WriteFile 函数写文件
 * 2. 检查返回值
 * 预期结果：
 * E2. 返回值是 UBSE_ERROR
 */
TEST_F(TestUbseFs, WirteFileFail)
{
    std::filesystem::remove_all(UBSE_TEST_DATA_PATH);
    std::string test = "test";
    auto ret = UbseFs(UBSE_TEST_DATA_PATH).WriteFile("test", reinterpret_cast<uint8_t *>(test.data()), test.size());
    EXPECT_EQ(ret, UBSE_ERROR);
}

/**
 * 用例描述：
 * 测试写文件正常
 * 测试步骤：
 * 1. 创建对应目录，调用 WriteFile 函数写文件
 * 2. 检查返回值
 * 预期结果：
 * E2. 返回值是 UBSE_OK
 */
TEST_F(TestUbseFs, WirteFileNormal)
{
    std::string test = "test";
    auto ret = UbseFs(UBSE_TEST_DATA_PATH).WriteFile("test", reinterpret_cast<uint8_t *>(test.data()), test.size());
    EXPECT_EQ(ret, UBSE_OK);
}

/**
 * 用例描述：
 * 测试写文件超过大小限制异常
 * 测试步骤：
 * 1. 创建对应目录，调用 WriteFile 函数写文件，写大小超出限制
 * 2. 检查返回值
 * 预期结果：
 * E2. 返回值是 UBSE_ERROR
 */
TEST_F(TestUbseFs, WirteFileFailWhenOverLimit)
{
    int size = UBSE_DATA_MAX_SIZE + 1;
    std::string test = "test";
    uint8_t data[size];
    auto ret = UbseFs(UBSE_TEST_DATA_PATH).WriteFile("test", data, size);
    EXPECT_EQ(ret, UBSE_ERROR);
}

/**
 * 用例描述：
 * 测试读不存在的文件返回值正常，数据为空
 * 测试步骤：
 * 1. 调用 ReadFile 读不存在的文件
 * 2. 检查返回值，数据长度
 * 预期结果：
 * E2. 返回值是 UBSE_OK，数据长度是0
 */
TEST_F(TestUbseFs, ReadNotExistFile)
{
    uint8_t *data = nullptr;
    uint32_t len = 0;
    auto ret = UbseFs(UBSE_TEST_DATA_PATH).ReadFile("test1", data, len);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(len, 0);
}

/**
 * 用例描述：
 * 测试读文件返回值正常，数据正常
 * 测试步骤：
 * 1. 调用 ReadFile 读之前写过的文件
 * 2. 检查返回值，数据内容
 * 预期结果：
 * E2. 返回值是 UBSE_OK，数据内容和之前一致
 */
TEST_F(TestUbseFs, ReadFileNormal)
{
    std::string test = "test";
    UbseFs(UBSE_TEST_DATA_PATH).WriteFile("test", reinterpret_cast<uint8_t *>(test.data()), test.size());
    uint8_t *data = nullptr;
    uint32_t len = 0;
    auto ret = UbseFs(UBSE_TEST_DATA_PATH).ReadFile("test", data, len);
    EXPECT_EQ(ret, UBSE_OK);
    std::string str(data, data + len);
    EXPECT_EQ("test", str);
    delete[] data;
}

/**
 * 用例描述：
 * 测试删除文件正常
 * 测试步骤：
 * 1. 调用 DeleteFile
 * 2. 检查返回值
 * 预期结果：
 * E2. 返回值是 UBSE_OK
 */
TEST_F(TestUbseFs, DeleteFileNormal)
{
    auto ret = UbseFs(UBSE_TEST_DATA_PATH).DeleteFile("test");
    EXPECT_EQ(ret, UBSE_OK);
}

/**
 * 用例描述：
 * 测试删除不存在的文件正常
 * 测试步骤：
 * 1. 调用 DeleteFile
 * 2. 检查返回值
 * 预期结果：
 * E2. 返回值是 UBSE_OK
 */
TEST_F(TestUbseFs, DeleteNotExistFileNormal)
{
    auto ret = UbseFs(UBSE_TEST_DATA_PATH).DeleteFile("test1");
    EXPECT_EQ(ret, UBSE_OK);
}
} // namespace ubse::ut::utils
