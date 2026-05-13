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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "mp_file_util.h"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace std;

namespace mempooling {
// Mocking the logging utility
class MockLogger {
public:
    MOCK_METHOD(void, LogError,
                (const std::string& moduleName, const std::string& moduleCode, const std::string& message), ());
};

// Mocking the realPath function
extern "C" {
char* RealPath(const char* path, char* resolvedPath);
}

// Test suite for MpFileUtil
class MpFileUtilTest : public ::testing::Test {
protected:
    // You can initialize any common objects or state for all test cases here
    MockLogger logger;

    MpFileUtilTest()
    {
        // Initialize or configure any mocks
    }

    // Clean up after each test
    ~MpFileUtilTest() override
    {
        // Any necessary cleanup
    }
};

// 正确打开文件
TEST_F(MpFileUtilTest, GetFileInfo_ValidFile)
{
    // 准备
    std::string path = "/tmp/valid_path_to_file.txt"; // 使用临时文件路径
    std::vector<std::string> info;
    std::string expectedContent = "Line1\nLine2\nLine3";

    // 确保文件夹存在
    if (access("/tmp", F_OK) == -1) {
        mkdir("/tmp", 0777); // 如果没有 "/tmp" 目录，则创建它
    }

    // 模拟 realPath 返回有效路径
    EXPECT_CALL(logger, LogError(testing::_, testing::_, testing::_)).Times(0); // 确保没有错误日志

    // 使用 std::ofstream 创建文件并写入内容
    std::ofstream file(path);
    ASSERT_TRUE(file.is_open()); // 确保文件打开成功
    file << expectedContent;
    file.close();

    // Act
    MEM_POOLING_RES result = MpFileUtil::GetFileInfo(path, info);

    // Assert
    EXPECT_EQ(result, MEM_POOLING_OK);
    EXPECT_EQ(info.size(), 3);
    EXPECT_EQ(info[0], "Line1");
    EXPECT_EQ(info[1], "Line2");
    EXPECT_EQ(info[2], "Line3");

    // 清理测试中创建的文件
    std::remove(path.c_str());
}

} // namespace mempooling