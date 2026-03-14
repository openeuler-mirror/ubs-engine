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

#include "test_ubse_logger_audit.h"
#include <dlfcn.h>
#include "ubse_logger_audit.cpp"

#define UBSE_AUDIT_LOGGING_OFF
namespace ubse::ut::log {
using namespace ubse::log;

TestUbseLoggerAudit::TestUbseLoggerAudit() {}
constexpr int USER_CMD = 1123;
constexpr int DEV_ALLOC = 2307;
constexpr int DEV_DEALLOC = 2308;
constexpr int CRYPTO_PARAM_CHANGE_USER = 2401;
void TestUbseLoggerAudit::SetUp(void)
{
    Test::SetUp();
}

void TestUbseLoggerAudit::TearDown(void)
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 测试AuditLoggerEntry类的RecordToAudit方法
 * 测试步骤：如下
 * 1.设置RecordToAudit传入的type值为AUDIT_OPERATE
 * 预期结果：如下
 * 1.返回值为AUDIT_USER_CMD
 */
TEST_F(TestUbseLoggerAudit, RecordToAudit_ShouldReturnAUDIT_USER_CMD_WhenRecordTypeIsAUDIT_OPERATE)
{
    OperateLoggerEntry operateLoggerEntry("test", RecordType::AUDIT_OPERATE);
    RecordType type = RecordType::AUDIT_OPERATE;
    int result = operateLoggerEntry.RecordToAudit(type);
    EXPECT_EQ(result, USER_CMD);
}

/*
 * 用例描述：
 * 测试AuditLoggerEntry类的RecordToAudit方法
 * 测试步骤：如下
 * 1.设置RecordToAudit传入的type值为AUDIT_RUNTIME_ALLOC
 * 预期结果：如下
 * 1.返回值为AUDIT_DEV_ALLOC
 */
TEST_F(TestUbseLoggerAudit, RecordToAudit_ShouldReturnAUDIT_DEV_ALLOC_WhenRecordTypeIsAUDIT_RUNTIME_ALLOC)
{
    OperateLoggerEntry operateLoggerEntry("test", RecordType::AUDIT_OPERATE);
    RecordType type = RecordType::AUDIT_RUNTIME_ALLOC;
    int result = operateLoggerEntry.RecordToAudit(type);
    EXPECT_EQ(result, DEV_ALLOC);
}

/*
 * 用例描述：
 * 测试AuditLoggerEntry类的RecordToAudit方法
 * 测试步骤：如下
 * 1.设置RecordToAudit传入的type值为AUDIT_RUNTIME_DEALLOC
 * 预期结果：如下
 * 1.返回值为AUDIT_DEV_ALLOC
 */
TEST_F(TestUbseLoggerAudit, RecordToAudit_ShouldReturnAUDIT_DEV_DEALLOC_WhenRecordTypeIsAUDIT_RUNTIME_DEALLOC)
{
    OperateLoggerEntry operateLoggerEntry("test", RecordType::AUDIT_OPERATE);
    RecordType type = RecordType::AUDIT_RUNTIME_DEALLOC;
    int result = operateLoggerEntry.RecordToAudit(type);
    EXPECT_EQ(result, DEV_DEALLOC);
}

/*
 * 用例描述：
 * 测试AuditLoggerEntry类的RecordToAudit方法
 * 测试步骤：如下
 * 1.设置RecordToAudit传入的type值为AUDIT_SECURITY
 * 预期结果：如下
 * 1.返回值为AUDIT_CRYPTO_PARAM_CHANGE_USER
 */
TEST_F(TestUbseLoggerAudit, RecordToAudit_WhenRecordTypeIsAUDIT_SECURITY)
{
    OperateLoggerEntry operateLoggerEntry("test", RecordType::AUDIT_OPERATE);
    RecordType type = RecordType::AUDIT_SECURITY;
    int result = operateLoggerEntry.RecordToAudit(type);
    EXPECT_EQ(result, CRYPTO_PARAM_CHANGE_USER);
}

/*
 * 用例描述：
 * 测试AuditLoggerEntry类的RecordToString方法
 * 测试步骤：如下
 * 1.设置RecordToString传入的type值为AUDIT_OPERATE
 * 预期结果：如下
 * 1.返回值为"OperateLog"
 */
TEST_F(TestUbseLoggerAudit, RecordToString_ShouldReturnOperateLog_WhenRecordTypeIsAuditOperate)
{
    OperateLoggerEntry operateLoggerEntry("test", RecordType::AUDIT_OPERATE);
    RecordType type = RecordType::AUDIT_OPERATE;
    std::string result = operateLoggerEntry.RecordToString(type);
    EXPECT_EQ("OperateLog", result);
}

/*
 * 用例描述：
 * 测试AuditLoggerEntry类的RecordToString方法
 * 测试步骤：如下
 * 1.设置RecordToString传入的type值为AUDIT_RUNTIME_ALLOC
 * 预期结果：如下
 * 1.返回值为"RuntimeLog"
 */
TEST_F(TestUbseLoggerAudit, RecordToString_ShouldReturnRuntimeLog_WhenRecordTypeIsAuditRuntimeAlloc)
{
    OperateLoggerEntry operateLoggerEntry("test", RecordType::AUDIT_OPERATE);
    RecordType type = RecordType::AUDIT_RUNTIME_ALLOC;
    std::string result = operateLoggerEntry.RecordToString(type);
    EXPECT_EQ("RuntimeLog", result);
}

/*
 * 用例描述：
 * 测试AuditLoggerEntry类的RecordToString方法
 * 测试步骤：如下
 * 1.设置RecordToString传入的type值为AUDIT_RUNTIME_DEALLOC
 * 预期结果：如下
 * 1.返回值为"RuntimeLog"
 */
TEST_F(TestUbseLoggerAudit, RecordToString_ShouldReturnRuntimeLog_WhenRecordTypeIsAuditRuntimeDealloc)
{
    OperateLoggerEntry operateLoggerEntry("test", RecordType::AUDIT_OPERATE);
    RecordType type = RecordType::AUDIT_RUNTIME_DEALLOC;
    std::string result = operateLoggerEntry.RecordToString(type);
    EXPECT_EQ("RuntimeLog", result);
}

/*
 * 用例描述：
 * 测试AuditLoggerEntry类的RecordToString方法
 * 测试步骤：如下
 * 1.设置RecordToString传入的type值为AUDIT_SECURITY
 * 预期结果：如下
 * 1.返回值为"SecurityLog"
 */
TEST_F(TestUbseLoggerAudit, RecordToString_ShouldReturnSecurityLog_WhenRecordTypeIsAuditSecurity)
{
    RuntimeLoggerEntry runtimeLoggerEntry(RecordType::AUDIT_RUNTIME_ALLOC);
    RecordType type = RecordType::AUDIT_SECURITY;
    std::string result = runtimeLoggerEntry.RecordToString(type);
    EXPECT_EQ("SecurityLog", result);
}

/*
 * 用例描述：
 * 测试AuditLogger的写操作日志方法
 * 测试步骤：如下
 * 1.设置合理的值,调用sendAuditMessage
 * 预期结果：如下
 * 1.无异常抛出
 */
TEST_F(TestUbseLoggerAudit, sendAuditMessage1)
{
    OperateLoggerEntry operateLoggerEntry("test", RecordType::AUDIT_OPERATE);
    RecordType type = RecordType::AUDIT_OPERATE;
    std::string logMessage = "Test log message";
    int result = 0;
    EXPECT_NO_THROW(operateLoggerEntry.SendAuditMessage(type, logMessage, result));
}

/*
 * 用例描述：
 * 测试AuditLogger的写操作日志方法
 * 测试步骤：
 * 1.设置合理的值
 * 预期结果：如下
 * 1.无异常抛出
 */

TEST_F(TestUbseLoggerAudit, sendAuditMessage2)
{
    std::string interface = "test_interface";
    EXPECT_NO_THROW(UBSE_AUDIT_OPERATE(interface) << "1111111111111111111111 test op");
}
TEST_F(TestUbseLoggerAudit, sendAuditMessage3)
{
    std::string interface = "test_interface";
    EXPECT_NO_THROW(UBSE_AUDIT_RUNTIME_ALLOC << "1111111111111111111111 test");
}
TEST_F(TestUbseLoggerAudit, sendAuditMessage4)
{
    std::string interface = "test_interface";
    EXPECT_NO_THROW(UBSE_AUDIT_RUNTIME_DEALLOC << "1111111111111111111111 test");
}
TEST_F(TestUbseLoggerAudit, sendAuditMessage5)
{
    std::string interface = "test_interface";
    EXPECT_NO_THROW(UBSE_AUDIT_SECURITY(interface) << "1111111111111111111111 test");
}

struct TestLibrary {};
inline void MockFunc() {}
bool CheckName1(const char *name)
{
    if (strcmp(name, "audit_open") == 0) {
        return true;
    } else {
        return false;
    }
}

bool CheckName2(const char *name)
{
    if (strcmp(name, "audit_close") == 0) {
        return true;
    } else {
        return false;
    }
}

/*
 * 用例描述：
 * 测试初始化dlopen库
 * 测试步骤：
 * 1.dlopen失败
 * 预期结果：
 * 1.打印错误日志
 */
TEST_F(TestUbseLoggerAudit, InitializeAuditFunctionsDlopen)
{
    GTEST_SKIP();
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(nullptr)));
    std::streambuf *oldCerrBuffer = std::cerr.rdbuf();
    std::stringstream capturedOutput;
    std::cerr.rdbuf(capturedOutput.rdbuf());
    InitializeAuditFunctions();
    std::cerr.rdbuf(oldCerrBuffer);
    EXPECT_EQ(capturedOutput.str(), "dlopen unable to load libaudit: ");
}

/*
 * 用例描述：
 * 测试初始化dlsym函数
 * 测试步骤：
 * 1.dlsym audit_open失败
 * 预期结果：
 * 1.打印"Unable to find symbol 'audit_open'"
 */
TEST_F(TestUbseLoggerAudit, InitializeAuditFunctionsDlsymAuditOpen)
{
    GTEST_SKIP();
    auto testLibrary = TestLibrary();
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(&testLibrary)));
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    MOCKER(dlclose).stubs().will(returnValue(0));
    std::streambuf *oldCerrBuffer = std::cerr.rdbuf();
    std::stringstream capturedOutput;
    std::cerr.rdbuf(capturedOutput.rdbuf());
    InitializeAuditFunctions();
    std::cerr.rdbuf(oldCerrBuffer);
    EXPECT_EQ(capturedOutput.str(), "Unable to find symbol 'audit_open': ");
    g_loaded = false;
    g_isOpenAudit = false;
}

/*
 * 用例描述：
 * 测试初始化dlsym函数
 * 测试步骤：
 * 1.dlsym audit_open成功
 * 2.dlsym audit_close失败
 * 预期结果：
 * 1.打印"Unable to find symbol 'audit_close'"
 */
TEST_F(TestUbseLoggerAudit, InitializeAuditFunctionsDlsymAuditClose)
{
    auto testLibrary = TestLibrary();
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(&testLibrary)));
    MOCKER(dlsym)
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(&MockFunc)))
        .then(returnValue(static_cast<void *>(nullptr)));
    MOCKER(dlclose).stubs().will(returnValue(0));
    std::streambuf *oldCerrBuffer = std::cerr.rdbuf();
    std::stringstream capturedOutput;
    std::cerr.rdbuf(capturedOutput.rdbuf());
    InitializeAuditFunctions();
    std::cerr.rdbuf(oldCerrBuffer);
    EXPECT_EQ(capturedOutput.str(), "Unable to find symbol 'audit_close': ");
    g_loaded = false;
    g_isOpenAudit = false;
}

/*
 * 用例描述：
 * 测试初始化dlsym函数
 * 测试步骤：
 * 1.dlsym audit_open成功
 * 2.dlsym audit_close成功
 * 3.dlsym audit_log_user_message失败
 * 预期结果：
 * 1.打印"Unable to find symbol 'audit_log_user_message'"
 */
TEST_F(TestUbseLoggerAudit, InitializeAuditFunctionsDlsymAuditLog)
{
    auto testLibrary = TestLibrary();
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(&testLibrary)));
    MOCKER(dlsym)
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(&MockFunc)))
        .then(returnValue(reinterpret_cast<void *>(&MockFunc)))
        .then(returnValue(static_cast<void *>(nullptr)));
    MOCKER(dlclose).stubs().will(returnValue(0));
    std::streambuf *oldCerrBuffer = std::cerr.rdbuf();
    std::stringstream capturedOutput;
    std::cerr.rdbuf(capturedOutput.rdbuf());
    InitializeAuditFunctions();
    std::cerr.rdbuf(oldCerrBuffer);
    EXPECT_EQ(capturedOutput.str(), "Unable to find symbol 'audit_log_user_message': ");
    g_loaded = false;
    g_isOpenAudit = false;
}

/*
 * 用例描述：
 * 测试初始化dlsym函数
 * 测试步骤：
 * 1.dlsym audit_open成功
 * 2.dlsym audit_close成功
 * 3.dlsym audit_log_user_message成功
 * 预期结果：
 * 1.未打印错误日志
 */
TEST_F(TestUbseLoggerAudit, InitializeAuditFunctionsSuccess)
{
    auto testLibrary = TestLibrary();
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(&testLibrary)));
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockFunc)));
    MOCKER(dlclose).stubs().will(returnValue(0));
    std::streambuf *oldCerrBuffer = std::cerr.rdbuf();
    std::stringstream capturedOutput;
    std::cerr.rdbuf(capturedOutput.rdbuf());
    InitializeAuditFunctions();
    std::cerr.rdbuf(oldCerrBuffer);
    EXPECT_EQ(capturedOutput.str(), "");
    g_loaded = false;
    g_isOpenAudit = false;
}
} // namespace ubse::ut::log