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

#include "test_ubse_logger.h"

namespace ubse::ut::log {
using namespace ubse::ut;
using namespace ubse::log;
TestUbseLogger::TestUbseLogger() {}
void TestUbseLogger::SetUp(void)
{
    Test::SetUp();
}

void TestUbseLogger::TearDown(void)
{
    Test::TearDown();
}
std::string DeleteOther(std::string str)
{
    std::regex pattern("\\[[^\\[\\]]*\\]");
    // 删除匹配的方括号
    int count = 0;
    while (count < 5 && std::regex_search(str, pattern)) { // 删除5对“[]"
        str = std::regex_replace(str, pattern, "", std::regex_constants::format_first_only);
        count++;
    }
    if (!str.empty() && str[0] == ' ') {
        str.erase(0, 1); // 删除第一个字符
    }
    return str;
}
/*
 * 用例描述：
 * 测试LogLevelToString方法
 */
TEST_F(TestUbseLogger, TestLogLevelToString)
{
    // 测试DEBUG级别
    EXPECT_EQ(LogLevelToString(UbseLogLevel::DEBUG), "DEBUG");

    // 测试INFO级别
    EXPECT_EQ(LogLevelToString(UbseLogLevel::INFO), "INFO");

    // 测试WARN级别
    EXPECT_EQ(LogLevelToString(UbseLogLevel::WARN), "WARN");

    // 测试ERROR级别
    EXPECT_EQ(LogLevelToString(UbseLogLevel::ERROR), "ERROR");

    // 测试CRIT级别
    EXPECT_EQ(LogLevelToString(UbseLogLevel::CRIT), "CRIT");

    // 测试未知级别
    EXPECT_EQ(LogLevelToString(static_cast<UbseLogLevel>(999)), "UNKNOWN"); // 设置999来测试UNKNOWN情况
}
/*
 * 用例描述：
 * 测试运算符重载
 */
TEST_F(TestUbseLogger, TestoperatorChar)
{
    UbseLoggerEntry UbseLoggerEntry("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 1);
    char data = 'a'; // 设置要写入的char型数据为a
    UbseLoggerEntry << data;
    std::ostringstream os;
    UbseLoggerEntry.OutPutLog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "a\n");
}
TEST_F(TestUbseLogger, TestoperatorInt)
{
    UbseLoggerEntry UbseLoggerEntry("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 1);
    int32_t data = -12345; // 设置要写入的int32_t型数为-12345
    UbseLoggerEntry << data;
    std::ostringstream os;
    UbseLoggerEntry.OutPutLog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "-12345\n");
}
TEST_F(TestUbseLogger, TestoperatorUint32)
{
    UbseLoggerEntry UbseLoggerEntry("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 1);
    uint32_t data = 12345; // 设置要写入的uint32_t型数为12345
    UbseLoggerEntry << data;
    std::ostringstream os;
    UbseLoggerEntry.OutPutLog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "12345\n");
}
TEST_F(TestUbseLogger, TestoperatorInt64)
{
    UbseLoggerEntry UbseLoggerEntry("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 1);
    int64_t data = -123456789012345; // 设置要写入的in642_t型数为-123456789012345
    UbseLoggerEntry << data;
    std::ostringstream os;
    UbseLoggerEntry.OutPutLog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "-123456789012345\n");
}
TEST_F(TestUbseLogger, TestoperatorUint64)
{
    UbseLoggerEntry UbseLoggerEntry("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 1);
    uint64_t data = 123456789012345; // 设置要写入的uin642_t型数为123456789012345
    UbseLoggerEntry << data;
    std::ostringstream os;
    UbseLoggerEntry.OutPutLog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "123456789012345\n");

    EXPECT_NO_THROW(UbseLoggerEntry << data);
}
TEST_F(TestUbseLogger, TestoperatorDouble)
{
    UbseLoggerEntry UbseLoggerEntry("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 1);
    double data = 3.14; // 设置要写入的duble型数为3.14
    UbseLoggerEntry << data;
    std::ostringstream os;
    UbseLoggerEntry.OutPutLog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "3.14\n");
}
TEST_F(TestUbseLogger, TestoperatorString)
{
    UbseLoggerEntry ubseLoggerEntry("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 1);
    // 长日志输入测试，设置大于512字节的日志消息，测试开发代码因输入长日志导致的偶现问题
    std::string data =
        "This is a long string that exceeds 512 bytes. It is designed to serve as a placeholder for various testing ";
    int count = 10; // 设置循环次数为10，增加日志长度
    while (count--) {
        data += data;
    }
    ubseLoggerEntry << data;
    std::ostringstream os;
    ubseLoggerEntry.OutPutLog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, data + "\n");
}
/*
 * 用例描述：
 * 测试ResizeBuffer方法
 */
TEST_F(TestUbseLogger, TestResizeBuffer)
{
    UbseLoggerEntry ubseLoggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    ubseLoggerEntry.currentSize = 1; // 设置currentSize为1
    ubseLoggerEntry.maxSize = 2;     // 设置maxSize为2
    size_t addSize = 1;              // 设置addSize为1
    EXPECT_NO_THROW(ubseLoggerEntry.ResizeBuffer(addSize));
}
TEST_F(TestUbseLogger, TestResizeBuffer1)
{
    UbseLoggerEntry ubseLoggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    ubseLoggerEntry.heapBuffer = std::make_unique<char[]>(10); // 设置10为数组大小
    ubseLoggerEntry.currentSize = 1;                           // 设置currentSize为1
    ubseLoggerEntry.maxSize = 1;                               // 设置maxSize为1
    size_t addSize = 1;                                        // 设置addSize为1
    ubseLoggerEntry.ResizeBuffer(addSize);
    EXPECT_EQ(ubseLoggerEntry.maxSize, 2); // 设置newSize为2
}
TEST_F(TestUbseLogger, TestResizeBuffer2)
{
    UbseLoggerEntry ubseLoggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    ubseLoggerEntry.heapBuffer = std::make_unique<char[]>(10); // 设置10为数组大小
    ubseLoggerEntry.currentSize = 1;                           // 设置currentSize为1
    ubseLoggerEntry.maxSize = 1;                               // 设置maxSize为1
    size_t addSize = 1;                                        // 设置addSize为1
    ubseLoggerEntry.ResizeBuffer(addSize);
    size_t testSize = ubseLoggerEntry.maxSize;
    EXPECT_EQ(ubseLoggerEntry.maxSize, std::max(testSize, static_cast<size_t>(2))); // 设置newSize为2
}
/*
 * 用例描述：
 * 测试EncodeString方法
 */
TEST_F(TestUbseLogger, TestEncodeString)
{
    UbseLoggerEntry ubseLoggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    char data[512];    // 设置data容量为512
    size_t length = 0; // 设置length为0
    EXPECT_NO_THROW(ubseLoggerEntry.EncodeString(data, length));
}
/*
 * 用例描述：
 * 测试Encodedata方法
 */
TEST_F(TestUbseLogger, testEncodeData)
{
    UbseLoggerEntry ubseLoggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    char *data = "test";
    EXPECT_NO_THROW(ubseLoggerEntry.EncodeData(data));
}
/*
 * 用例描述：
 * 测试DecodeChar方法
 */
TEST_F(TestUbseLogger, TestDecodeChar)
{
    UbseLoggerEntry ubseLoggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    // 准备输入数据
    char value = 'A';                          // 设置要写入的char型为'A'
    char buffer[512];                          // 设置buffer的容量为512
    std::copy_n(&value, sizeof(char), buffer); // 将整数拷贝到 buffer
    std::ostringstream oss;                    // 用于捕获输出流

    // 调用被测试的函数
    const char *nextBuffer = ubseLoggerEntry.DecodeChar(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(char)); // 验证返回的指针
}
/*
 * 用例描述：
 * 测试DecodeUint方法
 */
TEST_F(TestUbseLogger, TestDecodeUint)
{
    UbseLoggerEntry ubseLoggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    uint32_t value = 123456; // 设置要写入的uint32_t型数为123456
    std::ostringstream oss;
    char buffer[512]; // 设置buffer的容量为512
    std::copy_n(&value, sizeof(uint32_t), buffer);
    const char *nextBuffer = ubseLoggerEntry.DecodeUint(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(uint32_t)); // 验证返回的指针

    value = 0; // 设置value为0测试边界值
    std::copy_n(&value, sizeof(uint32_t), buffer);
    nextBuffer = ubseLoggerEntry.DecodeUint(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(uint32_t)); // 验证返回的指针

    value = 4294967295; // 设置value为4294967295测试边界值
    std::copy_n(&value, sizeof(uint32_t), buffer);
    nextBuffer = ubseLoggerEntry.DecodeUint(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(uint32_t)); // 验证返回的指针
}
/*
 * 用例描述：
 * 测试DecodeUlong方法
 */
TEST_F(TestUbseLogger, TestDecodeUlong)
{
    UbseLoggerEntry ubseLoggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    uint64_t value = 123456789012345; // 设置要写入的uint64_t型数为123456789012345
    std::ostringstream oss;
    char buffer[512]; // 设置buffer的容量为512
    std::copy_n(&value, sizeof(uint64_t), buffer);
    const char *nextBuffer = ubseLoggerEntry.DecodeUlong(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(uint64_t)); // 验证返回的指针

    value = 0; // 设置value为0测试边界值
    std::copy_n(&value, sizeof(uint64_t), buffer);
    nextBuffer = ubseLoggerEntry.DecodeUlong(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(uint64_t)); // 验证返回的指针

    value = UINT64_MAX; // 测试边界值
    std::copy_n(&value, sizeof(uint64_t), buffer);
    nextBuffer = ubseLoggerEntry.DecodeUlong(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(uint64_t)); // 验证返回的指针
}
/*
 * 用例描述：
 * 测试DecodeInt方法
 */
TEST_F(TestUbseLogger, TestDecodeInt)
{
    UbseLoggerEntry ubseLoggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    // 准备输入数据
    int32_t value = -123456; // 设置要写入的int32_t型数为-123456
    char buffer[512];        // 设置buffer的容量为512
    std::copy_n(&value, sizeof(int32_t), buffer);
    std::ostringstream oss; // 用于捕获输出流
    // 调用被测试的函数
    const char *nextBuffer = ubseLoggerEntry.DecodeInt(oss, buffer);
    // 验证输出
    EXPECT_EQ(nextBuffer, buffer + sizeof(int32_t)); // 验证返回的指针

    value = INT32_MIN; // 测试INT32_MIN边界值
    std::copy_n(&value, sizeof(int32_t), buffer);
    nextBuffer = ubseLoggerEntry.DecodeInt(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(int32_t)); // 验证返回的指针

    value = INT32_MAX; // 测试INT32_MAX边界值
    std::copy_n(&value, sizeof(int32_t), buffer);
    nextBuffer = ubseLoggerEntry.DecodeInt(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(int32_t)); // 验证返回的指针
}
/*
 * 用例描述：
 * 测试DecodeLong方法
 */
TEST_F(TestUbseLogger, TestDecodeLong)
{
    UbseLoggerEntry ubseLoggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    // 准备输入数据
    int64_t value = -123456789012345; // 假设要写入的in64_t型数为-123456789012345
    char buffer[512];                 // 设置buffer的容量为512
    std::copy_n(&value, sizeof(int64_t), buffer);
    std::ostringstream oss; // 用于捕获输出流
    // 调用DecodeString
    const char *nextBuffer = ubseLoggerEntry.DecodeLong(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(int64_t)); // 验证返回的指针

    value = INT64_MIN; // 测试INT64_MIN边界值
    std::copy_n(&value, sizeof(int64_t), buffer);
    nextBuffer = ubseLoggerEntry.DecodeLong(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(int64_t)); // 验证返回的指针

    value = INT64_MAX; // 测试INT64_MAX边界值
    std::copy_n(&value, sizeof(int64_t), buffer);
    nextBuffer = ubseLoggerEntry.DecodeLong(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(int64_t)); // 验证返回的指针
}
/*
 * 用例描述：
 * 测试DecodeDouble方法
 */
TEST_F(TestUbseLogger, TestDecodeDouble)
{
    UbseLoggerEntry ubseLoggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    // 准备输入数据
    double value = 3.14; // 假设要写入的数为3.14
    char buffer[512];    // 设置buffer的容量为512
    std::copy_n(&value, sizeof(double), buffer);

    std::ostringstream oss; // 用于捕获输出流

    // 调用被测试的函数
    const char *nextBuffer = ubseLoggerEntry.DecodeDouble(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(double)); // 验证返回的指针

    value = DBL_MAX; // 测试DBL_MAX边界值
    std::copy_n(&value, sizeof(double), buffer);
    // 调用被测试的函数
    nextBuffer = ubseLoggerEntry.DecodeDouble(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(double)); // 验证返回的指针

    value = DBL_MIN; // 测试DBL_MIN边界值
    std::copy_n(&value, sizeof(double), buffer);
    // 调用被测试的函数
    nextBuffer = ubseLoggerEntry.DecodeDouble(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(double)); // 验证返回的指针

    value = DBL_TRUE_MIN; // 测试DBL_TRUE_MIN边界值
    std::copy_n(&value, sizeof(double), buffer);
    // 调用被测试的函数
    nextBuffer = ubseLoggerEntry.DecodeDouble(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(double)); // 验证返回的指针

    value = -DBL_MAX; // 测试-DBL_MAX边界值
    std::copy_n(&value, sizeof(double), buffer);
    // 调用被测试的函数
    nextBuffer = ubseLoggerEntry.DecodeDouble(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(double)); // 验证返回的指针

    value = -DBL_MIN; // 测试-DBL_MIN边界值
    std::copy_n(&value, sizeof(double), buffer);
    // 调用被测试的函数
    nextBuffer = ubseLoggerEntry.DecodeDouble(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(double)); // 验证返回的指针

    value = -0.0; // 测试-0.0边界值
    std::copy_n(&value, sizeof(double), buffer);
    // 调用被测试的函数
    nextBuffer = ubseLoggerEntry.DecodeDouble(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(double)); // 验证返回的指针

    value = +0.0; // 测试+0.0边界值
    std::copy_n(&value, sizeof(double), buffer);
    // 调用被测试的函数
    nextBuffer = ubseLoggerEntry.DecodeDouble(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(double)); // 验证返回的指针
}


/*
 * 用例描述：
 * 测试DecodeData方法
 */
TEST_F(TestUbseLogger, TestDecodeData)
{
    UbseLoggerEntry ubseLoggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    std::ostringstream oss;
    char buffer[512]; // 设置buffer的容量为512
    std::vector<char> vec(buffer, buffer + sizeof(buffer));
    uint32_t size = 0; // 初始化size为0
    char c = 'A';
    int32_t i = -123456;              // 设置要写入的int32_t型数为-123456
    int64_t v = -123456789012345;     // 设置要写入的int64_t型数为-123456789012345
    uint32_t s = 123456;              // 设置要写入的uint32_t型数为123456
    uint64_t value = 123456789012345; // 设置要写入的uint64_t型数为123456789012345
    double d = 3.14;                  // 设置要写入的double型数为3.14
    const char *str = "test1";
    const std::string data = "test2";

    *reinterpret_cast<UbseLoggerTypeId *>(&buffer[size]) = UbseLoggerTypeId::CHAR;
    size += sizeof(UbseLoggerTypeId);
    *reinterpret_cast<char *>(&buffer[size]) = c;
    size += sizeof(char);

    *reinterpret_cast<UbseLoggerTypeId *>(&buffer[size]) = UbseLoggerTypeId::INT32;
    size += sizeof(UbseLoggerTypeId);
    *reinterpret_cast<int32_t *>(&buffer[size]) = i;
    size += sizeof(int32_t);

    *reinterpret_cast<UbseLoggerTypeId *>(&buffer[size]) = UbseLoggerTypeId::INT64;
    size += sizeof(UbseLoggerTypeId);
    *reinterpret_cast<int64_t *>(&buffer[size]) = v;
    size += sizeof(int64_t);

    *reinterpret_cast<UbseLoggerTypeId *>(&buffer[size]) = UbseLoggerTypeId::UINT32;
    size += sizeof(UbseLoggerTypeId);
    *reinterpret_cast<uint32_t *>(&buffer[size]) = s;
    size += sizeof(uint32_t);

    *reinterpret_cast<UbseLoggerTypeId *>(&buffer[size]) = UbseLoggerTypeId::UINT64;
    size += sizeof(UbseLoggerTypeId);
    *reinterpret_cast<uint64_t *>(&buffer[size]) = value;
    size += sizeof(uint64_t);

    *reinterpret_cast<UbseLoggerTypeId *>(&buffer[size]) = UbseLoggerTypeId::DOUBLE;
    size += sizeof(UbseLoggerTypeId);
    *reinterpret_cast<double *>(&buffer[size]) = d;
    size += sizeof(double);
    ubseLoggerEntry.DecodeData(oss, &buffer[0], (&buffer[0]) + size);
    EXPECT_EQ(oss.str(), "A-123456-1234567890123451234561234567890123453.14");
    // 和已设置的数"A-123456-1234567890123451234561234567890123453.14"进行比较
}
/*
 * 用例描述：
 * 测试UbseIsLog方法
 */
TEST_F(TestUbseLogger, TestUbseIsLog)
{
    UbseLogLevel level = UbseLogLevel::INFO;
    LoggerOptions options{ UbseLoggerManager::StringToLogLevel("INFO"), 30, 20,
        1024 }; // 设置30为filesize，20为fileNums，1024为maxItem
    UbseLoggerManager::gInstance = new (std::nothrow) UbseLoggerManager();
    UbseLoggerManager::gInstance->SetLogLevel(options.minLogLevel);
    EXPECT_TRUE(UbseIsLog(level));
    UbseLoggerManager::gInstance = nullptr;
    EXPECT_FALSE(UbseIsLog(level));
}
/*
 * 用例描述：
 * 测试UbseLog方法
 */
TEST_F(TestUbseLogger, TestUbseLog)
{
    UbseLoggerEntry ubseLoggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    EXPECT_NO_THROW(UbseLog(ubseLoggerEntry));
}

/*
 * 用例描述：
 * 测试拷贝构造函数
 */
TEST_F(TestUbseLogger, TestCopyConstructor)
{
    UbseLoggerEntry ubseLoggerEntry1(nullptr, UbseLogLevel::INFO, "test.cpp", "testFunc", 111); // 设置行号为111
    UbseLoggerEntry ubseLoggerEntry2(ubseLoggerEntry1);
    ubseLoggerEntry2.GetEntryTimeStamp();
    EXPECT_EQ(ubseLoggerEntry2.GetFile(), "test.cpp");
    EXPECT_EQ(ubseLoggerEntry2.GetLine(), 111); // 查看ubseLoggerEntry2行号是否为111

    UbseLoggerEntry ubseLoggerEntry3(nullptr, UbseLogLevel::INFO, "test.cpp", "testFunc", 111); // 设置行号为111
    std::string str =
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    ubseLoggerEntry3 << str << str << str << str << str << str;
    UbseLoggerEntry ubseLoggerEntry4(ubseLoggerEntry3);
    EXPECT_EQ(ubseLoggerEntry4.GetFile(), "test.cpp");
    EXPECT_EQ(ubseLoggerEntry4.GetLine(), 111); // 查看ubseLoggerEntry4行号是否为111
}

/*
 * 用例描述：
 * 测试UbseLoggerEntry赋值
 */
TEST_F(TestUbseLogger, TestAssignmentOperatorOverload)
{
    UbseLoggerEntry ubseLoggerEntry1(nullptr, UbseLogLevel::INFO, "test.cpp", "testFunc", 111); // 设置行号为111
    UbseLoggerEntry ubseLoggerEntry2 = ubseLoggerEntry1;
    EXPECT_EQ(ubseLoggerEntry2.GetFile(), "test.cpp");
    EXPECT_EQ(ubseLoggerEntry2.GetLine(), 111); // 查看ubseLoggerEntry2行号是否为111

    UbseLoggerEntry ubseLoggerEntry3(nullptr, UbseLogLevel::INFO, "test.cpp", "testFunc", 111); // 设置行号为111
    std::string str =
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    ubseLoggerEntry3 << str << str << str << str << str << str;
    UbseLoggerEntry ubseLoggerEntry4 = ubseLoggerEntry3;
    EXPECT_EQ(ubseLoggerEntry4.GetFile(), "test.cpp");
    EXPECT_EQ(ubseLoggerEntry4.GetLine(), 111); // 查看ubseLoggerEntry4行号是否为111
}

/*
 * 用例描述：
 * 测试格式化返回值
 * 测试步骤：
 * 1. 格式化retCode
 * 预期结果：
 * 1. 返回retCode格式化后的字符串。
 */
TEST_F(TestUbseLogger, TestParseRetCode)
{
    uint32_t retCode = 10001;
    std::string str = FormatRetCode(retCode);
    EXPECT_STREQ(str.c_str(), "ErrorCode=10001");
}
/*
 * 用例描述：
 * 测试FormatSyslog
 * 测试步骤：
 * 1. 通过FormatSyslog获取流
 * 预期结果：
 * 1. 得到的字符串与预期一致
 */
TEST_F(TestUbseLogger, TestFormatSyslog)
{
    UbseLoggerEntry ubseLoggerEntry("test_log", UbseLogLevel::INFO, "Test.log", "TestFunction", 1);
    int64_t data = -123456789012345; // 设置要写入的in642_t型数为-123456789012345
    ubseLoggerEntry << data;
    std::ostringstream os;
    ubseLoggerEntry.FormatSyslog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "-123456789012345\n");
}
}