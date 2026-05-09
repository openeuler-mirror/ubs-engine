/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "test_vm_file_util.h"
#include <filesystem>
#include <fstream>

using namespace vm;
namespace ubse::ut::vm {
const std::string TEST_VM_FILE_UTIL_TEST_DIR = "./testVmFileUtil";
const std::string TEST_FILE_DIR = TEST_VM_FILE_UTIL_TEST_DIR + "/sys/devices/system/node/node0";
const std::string TEST_FILE = TEST_FILE_DIR + "/cpulist";

void TestWriteFile(const std::string &filePath, const std::string &message)
{
    std::ofstream testOutputFile;
    testOutputFile.open(filePath);
    testOutputFile << message;
    testOutputFile.close();
}

// 设置测试环境
void TestVmFileUtil::SetUp()
{
    std::filesystem::create_directories(TEST_FILE_DIR);
    TestWriteFile(TEST_FILE, "0-79");
    Test::SetUp();
}

// 拆卸测试环境
void TestVmFileUtil::TearDown()
{
    std::filesystem::remove_all(TEST_VM_FILE_UTIL_TEST_DIR);
    Test::TearDown();
}

/**
 * 读取文件内容成功
 */
TEST_F(TestVmFileUtil, GetFileInfoSuccess)
{
    auto lineInfo = vector<string>();
    auto ret = VmFileUtil::GetFileInfo(TEST_FILE, lineInfo);
    EXPECT_EQ(ret, VM_OK);
}

/**
 * 文件不存在,读取文件内容失败
 */
TEST_F(TestVmFileUtil, GetFileInfoFailed1)
{
    auto lineInfo = vector<string>();
    string filePath = TEST_FILE + "111";
    auto ret = VmFileUtil::GetFileInfo(filePath, lineInfo);
    EXPECT_EQ(ret, VM_WARN);
}

/**
 * 文件路径带非法字符,读取文件内容失败
 */
TEST_F(TestVmFileUtil, GetFileInfoFailed2)
{
    auto lineInfo = vector<string>();
    string filePath = TEST_FILE + "*?111";
    auto ret = VmFileUtil::GetFileInfo(filePath, lineInfo);
    EXPECT_EQ(ret, VM_WARN);
}
} // namespace ubse::ut::vm