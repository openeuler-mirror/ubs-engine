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

#include "test_ubse_logger_filesink.h"
#include <filesystem>
#include <thread>
#include "ubse_context.h"

namespace fs = std::filesystem;

namespace ubse::ut::log {
using namespace ubse::context;
std::string FindFirstFileMatchingPattern(const std::string& pattern, const std::string prefix)
{
    std::regex regexPattern(pattern);
    for (const auto& entry : fs::directory_iterator(prefix)) {
        if (entry.is_regular_file() && std::regex_match(entry.path().filename().string(), regexPattern)) {
            return entry.path().string();
        }
    }
    return "";
}

void TestUbseLoggerFileSink::SetUp()
{
    Test::SetUp();
    currentPath = std::filesystem::current_path().string();
    auto result = mkdir((currentPath + FILE_PATH).c_str(), 0750); // 权限0750

    auto canonicalPath = realpath("/var/log/ubse", nullptr);
    if (canonicalPath == nullptr) {
        auto result = mkdir("/var/log/ubse", 0777); // 权限0777
        if (result == -1) {
            perror("mkdir"); // Print error message
            std::cerr << "Failed to create directory: " << std::endl;
        } else {
            std::cout << "Directory created successfully: " << std::endl;
        }
    }
}
/*
 * 测试结束后清理测试生成的日志文件
 * 检查当前目录下是否存在名为"test_log.log"的文件，如果存在则删除；
 * 遍历当前目录下的所有文件，如果文件是普通文件（非目录），并且文件名以"test_log_"开头，则删除该文件；
 * 检查当前目录下是否存在名为"log"的子目录，如果存在则删除该子目录及其所有内容。
 */
void TestUbseLoggerFileSink::TearDown()
{
    Test::TearDown();
    if (fs::exists(currentPath + FILE_PATH + FILE_NAME + ".log")) {
        fs::remove(currentPath + FILE_PATH + FILE_NAME + ".log");
    }
    if (!fs::exists(currentPath + FILE_PATH)) {
        return;
    }
    for (const auto& entry : fs::directory_iterator(currentPath + FILE_PATH)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string filename = entry.path().filename().string();
        if (filename.find("test_log_") == 0) {
            fs::remove(entry.path());
        }
    }
}

/*
 * 用例描述：
 * 成功写入日志消息
 * 测试步骤：
 * 1. 创建FileSink实例，设置日志文件大小限制为1024字节。
 * 2. 调用Write方法写入一条日志消息。
 * 预期结果：
 * 1. 日志文件成功创建并打开。
 * 2. 日志文件中包含写入的消息。
 */
TEST_F(TestUbseLoggerFileSink, TestWrite)
{
    UbseLoggerFilesink sink(currentPath + FILE_PATH, 1024, 16); // 1024设置为文件最大大小，16设置为文件最大数量
    UbseLoggerEntry entry("test_log", UbseLogLevel::INFO, "test.log", "TestFunction", 1);
    ASSERT_TRUE(sink.Initialize());
    ASSERT_TRUE(sink.Write(entry));

    std::ifstream logFile(currentPath + FILE_PATH + "test_log.log");
    ASSERT_TRUE(logFile.is_open());
}
/*
 * 用例描述：
 * 文件滚动并压缩
 * 测试步骤：
 * 1. 创建FileSink实例，设置日志文件大小限制为32字节。
 * 2. 连续写入多条日志消息以触发文件滚动。
 * 预期结果：
 * 1. 原始日志文件存在。
 * 2. 生成一个压缩的日志文件。
 */
TEST_F(TestUbseLoggerFileSink, RollFile)
{
    // 设置一个较小的maxFileSize以便触发滚动，20设置为文件最大大小，16设置为文件最大数量
    UbseLoggerFilesink sink(currentPath + FILE_PATH, 20, 16);
    ASSERT_TRUE(sink.Initialize());
    // 设置ubseLoggerEntry1的行数为1
    UbseLoggerEntry ubseLoggerEntry1("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 1);
    // 设置ubseLoggerEntry2的行数为2
    UbseLoggerEntry ubseLoggerEntry2("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 2);
    // 设置ubseLoggerEntry3的行数为3
    UbseLoggerEntry ubseLoggerEntry3("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 3);
    ASSERT_TRUE(sink.Write(ubseLoggerEntry1));
    ASSERT_TRUE(sink.Write(ubseLoggerEntry2));
    ASSERT_TRUE(sink.Write(ubseLoggerEntry3)); // 触发文件滚动

    ASSERT_TRUE(!fs::exists(currentPath + FILE_PATH + "test_log.log")); // 原始日志文件不存在
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_001\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 压缩文件应存在
}

/*
 * 用例描述：
 * 文件滚动并压缩，达到最大数量时删除最旧文件并重新命名
 * 测试步骤：
 * 1. 创建FileSink实例，设置日志文件大小限制为32字节，最大文件数量为3。
 * 2. 连续写入多条日志消息以触发文件滚动并生成多个压缩文件。
 * 预期结果：
 * 1. 原始日志文件存在。
 * 2. 生成最多3个压缩的日志文件。
 * 3. 最旧的压缩文件被删除，剩余文件按序重新命名。
 */
TEST_F(TestUbseLoggerFileSink, RollFileWithMaxCount)
{
    // 设置一个较小的maxFileSize以便触发滚动，最大文件数量为3
    UbseLoggerFilesink sink(currentPath + FILE_PATH, 20, 3); // 20设置为文件最大大小，3设置为文件最大数量
    ASSERT_TRUE(sink.Initialize());
    // 写入足够多的日志消息以触发滚动并生成多个压缩文件
    // 设置ubseLoggerEntry1的行数为1
    UbseLoggerEntry ubseLoggerEntry1("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 1);
    // 设置ubseLogggerEntry2的行数为2
    UbseLoggerEntry ubseLoggerEntry2("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 2);
    // 设置ubseLoggerEntry3的行数为3
    UbseLoggerEntry ubseLoggerEntry3("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 3);
    // 设置ubseLoggerEntry4的行数为4
    UbseLoggerEntry ubseLoggerEntry4("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 4);
    // 设置ubseLogggerEntry5的行数为5
    UbseLoggerEntry ubseLoggerEntry5("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 5);
    // 设置ubseLoggerEntry6的行数为6
    UbseLoggerEntry ubseLoggerEntry6("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 6);
    // 设置ubseLoggerEntry7的行数为7
    UbseLoggerEntry ubseLoggerEntry7("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 7);
    // 设置ubseLoggerEntry8的行数为8
    UbseLoggerEntry ubseLoggerEntry8("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 8);
    ASSERT_TRUE(sink.Write(ubseLoggerEntry1));
    ASSERT_TRUE(sink.Write(ubseLoggerEntry2)); // 触发第一次滚动
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_001\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 1号压缩文件应存在
    ASSERT_TRUE(sink.Write(ubseLoggerEntry3));
    ASSERT_TRUE(sink.Write(ubseLoggerEntry4)); // 触发第二次滚动
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_002\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 2号压缩文件应存在
    ASSERT_TRUE(sink.Write(ubseLoggerEntry5));
    ASSERT_TRUE(sink.Write(ubseLoggerEntry6)); // 触发第三次滚动
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_003\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 3号压缩文件应存在
    ASSERT_TRUE(sink.Write(ubseLoggerEntry7));
    ASSERT_TRUE(sink.Write(ubseLoggerEntry8)); // 触发第四次滚动
    ASSERT_TRUE(FindFirstFileMatchingPattern(R"(test_log_.*_004\.tar\.gz)", currentPath + FILE_PATH)
                    .empty()); // 4号压缩文件不存在
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_001\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 1号压缩文件应存在
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_002\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 2号压缩文件应存在
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_003\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 3号压缩文件应存在
    // 检查原始日志文件是否存在
    ASSERT_TRUE(!fs::exists(currentPath + FILE_PATH + "test_log.log"));
}

/*
 * 用例描述：
 * 设置非同级相对路径，文件滚动并压缩，达到最大数量时删除最旧文件并重新命名
 * 测试步骤：
 * 1. 创建FileSink实例，设置日志文件大小限制为32字节，最大文件数量为3。
 * 2. 连续写入多条日志消息以触发文件滚动并生成多个压缩文件。
 * 预期结果：
 * 1. 原始日志文件存在。
 * 2. 生成最多3个压缩的日志文件。
 * 3. 最旧的压缩文件被删除，剩余文件按序重新命名。
 */
TEST_F(TestUbseLoggerFileSink, NonPeerDirRollFileWithMaxCount)
{
    // 设置一个较小的值20为maxFileSize以便触发滚动，最大文件数量为3
    UbseLoggerFilesink sink(currentPath + FILE_PATH, 20, 3);
    ASSERT_TRUE(sink.Initialize());
    // 设置ubseLoggerEntry1的行数为1
    UbseLoggerEntry ubseLoggerEntry1("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 1);
    // 设置ubseLogggerEntry2的行数为2
    UbseLoggerEntry ubseLoggerEntry2("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 2);
    // 设置ubseLoggerEntry3的行数为3
    UbseLoggerEntry ubseLoggerEntry3("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 3);
    // 设置ubseLoggerEntry4的行数为4
    UbseLoggerEntry ubseLoggerEntry4("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 4);
    // 设置ubseLogggerEntry5的行数为5
    UbseLoggerEntry ubseLoggerEntry5("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 5);
    // 设置ubseLoggerEntry6的行数为6
    UbseLoggerEntry ubseLoggerEntry6("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 6);
    // 设置ubseLoggerEntry7的行数为7
    UbseLoggerEntry ubseLoggerEntry7("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 7);
    // 设置ubseLoggerEntry8的行数为8
    UbseLoggerEntry ubseLoggerEntry8("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 8);
    // 写入足够多的日志消息以触发滚动并生成多个压缩文件
    ASSERT_TRUE(sink.Write(ubseLoggerEntry1));
    ASSERT_TRUE(sink.Write(ubseLoggerEntry2)); // 触发第一次滚动
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_001\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 1号压缩文件应存在
    ASSERT_TRUE(sink.Write(ubseLoggerEntry3));
    ASSERT_TRUE(sink.Write(ubseLoggerEntry4)); // 触发第二次滚动
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_002\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 2号压缩文件应存在
    ASSERT_TRUE(sink.Write(ubseLoggerEntry5));
    ASSERT_TRUE(sink.Write(ubseLoggerEntry6)); // 触发第三次滚动
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_003\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 3号压缩文件应存在
    ASSERT_TRUE(sink.Write(ubseLoggerEntry7));
    ASSERT_TRUE(sink.Write(ubseLoggerEntry8)); // 触发第四次滚动
    ASSERT_TRUE(FindFirstFileMatchingPattern(R"(test_log_.*_004\.tar\.gz)", currentPath + FILE_PATH)
                    .empty()); // 4号压缩文件不存在
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_001\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 1号压缩文件应存在
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_002\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 2号压缩文件应存在
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_003\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 3号压缩文件应存在
    // 检查原始日志文件是否存在
    ASSERT_TRUE(!fs::exists(currentPath + FILE_PATH + "test_log.log"));
    for (const auto& entry : std::filesystem::directory_iterator(currentPath + FILE_PATH)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string filename = entry.path().filename().string();
        if (filename.find("test_log_") == 0) {
            std::filesystem::remove(entry.path());
        }
    }
}

/*
 * 用例描述：
 * 文件滚动后文件索引增加
 * 测试步骤：
 * 1. 创建FileSink实例，设置日志文件大小限制为4字节。
 * 2. 连续写入多条日志消息以触发多次文件滚动。
 * 预期结果：
 * 1. 文件索引依次递增。
 * 2. 生成多个压缩文件。
 */
TEST_F(TestUbseLoggerFileSink, FileIndexIncrement)
{
    // 设置一个较小的值4作为maxFileSize以便触发滚动，maxFileCount为16
    UbseLoggerFilesink sink(currentPath + FILE_PATH, 4, 16);
    // 设置ubseLoggerEntry1的行数为1
    UbseLoggerEntry ubseLoggerEntry1("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 1);
    // 设置ubseLogggerEntry2的行数为2
    UbseLoggerEntry ubseLoggerEntry2("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 2);
    // 设置ubseLoggerEntry3的行数为3
    UbseLoggerEntry ubseLoggerEntry3("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 3);
    // 设置ubseLoggerEntry4的行数为4
    UbseLoggerEntry ubseLoggerEntry4("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 4);
    ASSERT_TRUE(sink.Initialize());
    UbseLoggerEntry* entrys[] = {&ubseLoggerEntry1, &ubseLoggerEntry2, &ubseLoggerEntry3, &ubseLoggerEntry4};
    // 将4条日志消息写入
    for (const auto& entry : entrys) {
        ASSERT_TRUE(sink.Write(*entry));
    }
    // 循环4次生成4个两位数字符串
    for (int i = 1; i <= 4; ++i) {
        std::stringstream ss;
        ss << std::setw(3) << std::setfill('0') << i; // 设置输出字符串的宽度为3
        std::string compressedFilename = "test_log_.*_" + ss.str() + ".tar.gz";
        EXPECT_TRUE(!FindFirstFileMatchingPattern(compressedFilename, currentPath + FILE_PATH).empty());
    }
}

/*
 * 用例描述：
 * 最大文件边界值处理
 * 测试步骤：
 * 1. 创建FileSink实例，设置文件大小为0。
 * 2. 尝试写入日志消息。
 * 预期结果：
 * 1. FileSink初始化配置函数应处理文件大小为0并返回false。
 * 2. Write方法返回false。
 */
TEST_F(TestUbseLoggerFileSink, ZeroMaxFileSize)
{
    UbseLoggerFilesink sink(currentPath + FILE_PATH, 0, 16); // 0设置为文件最大大小，16设置为文件最大数量
    ASSERT_FALSE(sink.Initialize());
}

/*
 * 用例描述：
 * 最大压缩包数边界值处理
 * 测试步骤：
 * 1. 创建FileSink实例，设置压缩包最大数量为0。
 * 2. 尝试写入日志消息。
 * 预期结果：
 * 1. FileSink初始化配置函数应处理压缩包最大数量为0并返回false。
 * 2. Write方法返回false。
 */
TEST_F(TestUbseLoggerFileSink, ZeroMaxFileCount)
{
    UbseLoggerFilesink sink(currentPath + FILE_PATH, 1024, 0); // 1024设置为文件最大大小，0设置为文件最大数量
    ASSERT_FALSE(sink.Initialize());
}

/*
 * 用例描述：
 * 日志文件路径不存在
 * 测试步骤：
 * 1. 创建FileSink实例，设置一个不存在的日志文件路径。
 * 2. 调用Write方法写入日志消息。
 * 预期结果：
 * 1. FileSink构造函数使用默认路径./log/run。
 * 2. Write方法返回true。
 */
TEST_F(TestUbseLoggerFileSink, NonexistentFilePath)
{
    UbseLoggerFilesink sink(currentPath + "/noexist", 1024, 16); // 1024设置为文件最大大小，16设置为文件最大数量
    // 设置ubseLoggerEntry的行数为1
    UbseLoggerEntry ubseLoggerEntry("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 1);
    ASSERT_TRUE(sink.Write(ubseLoggerEntry));
    std::string fileName = currentPath + "/noexist/" + FILE_NAME + ".log";
    std::ifstream logFile(fileName);
    ASSERT_TRUE(logFile.is_open());
}

/*
 * 用例描述：
 * 测试判断文件inode是否已经失效
 * 测试步骤：
 * 1. 创建FileSink实例，设置一个日志文件路径。
 * 2. 调用Write方法写入日志消息。
 * 3.判断文件是否被外部修改
 * 预期结果：
 * 1. 文件未被外部修改，inode依然有效。
 */
TEST_F(TestUbseLoggerFileSink, TestIsFileStatusChanged)
{
    UbseLoggerFilesink sink(currentPath + FILE_PATH, 1024, 16); // 1024设置为文件最大大小，16设置为文件最大数量
    UbseLoggerEntry ubseLoggerEntry("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 1);
    sink.Write(ubseLoggerEntry);
    std::string fileName = "test_log";
    ASSERT_FALSE(sink.IsFileStatusChanged(fileName));
}
} // namespace ubse::ut::log