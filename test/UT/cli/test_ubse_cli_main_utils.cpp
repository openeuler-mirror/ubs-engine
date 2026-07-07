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

#include "test_ubse_cli_main_utils.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <string>
#include <vector>

#include "ubse_cli_main_utils.h"
#include "ubse_error.h"

namespace ubse::ut::cli {
using namespace ubse::cli::framework;

void TestUbseCliMainUtils::SetUp() {}

void TestUbseCliMainUtils::TearDown()
{
    // 启动过ITIMER_REAL的用例不能把进程级定时器泄露给后续用例。
    (void)UbseCliStopTimeoutTimer();
}

namespace {
// 读取pipe捕获到的子进程标准输出，直到EOF。
std::string ReadAllFromFd(int fd)
{
    std::string output;
    std::array<char, 256> buffer{};
    ssize_t readSize = 0;
    while ((readSize = read(fd, buffer.data(), buffer.size())) > 0) {
        output.append(buffer.data(), static_cast<size_t>(readSize));
    }
    return output;
}
} // namespace

TEST_F(TestUbseCliMainUtils, ValidateStartupConditionsRejectsNullArgv)
{
    // argv在argc范围校验之后检查，以匹配CLI启动行为。
    testing::internal::CaptureStdout();
    EXPECT_EQ(UbseCliValidateStartupConditions(1, nullptr), UBSE_ERROR);
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("ERROR: System input data is empty."), std::string::npos);
}

TEST_F(TestUbseCliMainUtils, ValidateStartupConditionsRejectsTooManyArgs)
{
    // 44超过公共CLI参数上限43。
    std::vector<char *> argv(44, const_cast<char *>("arg"));

    testing::internal::CaptureStdout();
    EXPECT_EQ(UbseCliValidateStartupConditions(static_cast<int>(argv.size()), argv.data()), UBSE_ERROR);
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("ERROR: Unsupported number of parameters"), std::string::npos);
}

TEST_F(TestUbseCliMainUtils, ValidateStartupConditionsAcceptsNormalInput)
{
    // 仅包含程序名是合法输入，命令校验在后续流程处理。
    char program[] = "ubsectl";
    char *argv[] = {program};

    EXPECT_EQ(UbseCliValidateStartupConditions(1, argv), UBSE_OK);
}

TEST_F(TestUbseCliMainUtils, BuildArgsSkipsProgramNameAndReservesExtraCapacity)
{
    // 专用CLI需要预留额外容量，用于插入隐式type。
    char program[] = "ubsectl";
    char command[] = "display";
    char type[] = "node";
    char *argv[] = {program, command, type};

    auto args = UbseCliBuildArgs(3, argv, 5);

    ASSERT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "display");
    EXPECT_EQ(args[1], "node");
    EXPECT_GE(args.capacity(), 8);
}

TEST_F(TestUbseCliMainUtils, ValidateArgsByWhitelistAcceptsValidArguments)
{
    // command、type和option name位置均合法。
    EXPECT_EQ(UbseCliValidateArgsByWhitelist({"display", "node", "-id", "node@1"}), UBSE_OK);
}

TEST_F(TestUbseCliMainUtils, ValidateArgsByWhitelistRejectsInvalidFirstArgument)
{
    // 第一个解析器参数是command，必须通过白名单。
    testing::internal::CaptureStdout();
    EXPECT_EQ(UbseCliValidateArgsByWhitelist({"display!", "node"}), UBSE_ERROR);
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("ERROR: Invalid characters in the whitelist."), std::string::npos);
}

TEST_F(TestUbseCliMainUtils, ValidateArgsByWhitelistRejectsInvalidSecondArgument)
{
    // 第二个解析器参数是type，必须通过白名单。
    EXPECT_EQ(UbseCliValidateArgsByWhitelist({"display", "node!"}), UBSE_ERROR);
}

TEST_F(TestUbseCliMainUtils, ValidateArgsByWhitelistRejectsInvalidOddPositionArgument)
{
    // command/type之后，奇数解析位置是option name，需要校验。
    EXPECT_EQ(UbseCliValidateArgsByWhitelist({"display", "node", "-id!"}), UBSE_ERROR);
}

TEST_F(TestUbseCliMainUtils, ValidateArgsByWhitelistAllowsInvalidEvenValueArgument)
{
    // option value保留历史宽松校验规则，由命令自身校验。
    EXPECT_EQ(UbseCliValidateArgsByWhitelist({"display", "node", "-id", "node!"}), UBSE_OK);
}

TEST_F(TestUbseCliMainUtils, ValidateCommandArgsCountPrintsProgramNameWhenTooFewArgs)
{
    // 错误提示必须使用各CLI可执行文件传入的程序名。
    testing::internal::CaptureStdout();
    EXPECT_EQ(UbseCliValidateCommandArgsCount({"display"}, "ubsectl-test"), UBSE_ERROR);
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("ubsectl-test --help"), std::string::npos);
}

TEST_F(TestUbseCliMainUtils, ValidateCommandArgsCountAcceptsCommandAndType)
{
    // 注册表解析要求的最小参数形态是command + type。
    EXPECT_EQ(UbseCliValidateCommandArgsCount({"display", "node"}, "ubsectl"), UBSE_OK);
}

TEST_F(TestUbseCliMainUtils, StartAndStopTimeoutTimerSucceed)
{
    // 成功路径同时验证SIGALRM注册和定时器清理。
    EXPECT_EQ(UbseCliStartTimeoutTimer(), UBSE_OK);
    EXPECT_EQ(UbseCliStopTimeoutTimer(), UBSE_OK);
}

TEST_F(TestUbseCliMainUtils, TimeoutSignalHandlerPrintsTimeoutAndExitsWithSignalNumber)
{
    // handler按设计会结束进程，因此放到子进程中执行。
    int pipeFd[2];
    ASSERT_EQ(pipe(pipeFd), 0) << strerror(errno);

    pid_t pid = fork();
    ASSERT_GE(pid, 0) << strerror(errno);

    if (pid == 0) {
        // 重定向子进程标准输出，便于父进程校验超时信息。
        close(pipeFd[0]);
        dup2(pipeFd[1], STDOUT_FILENO);
        close(pipeFd[1]);
        UbseCliTimeoutSignalHandler(SIGALRM);
    }

    close(pipeFd[1]);
    std::string output = ReadAllFromFd(pipeFd[0]);
    close(pipeFd[0]);

    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), SIGALRM);
    EXPECT_EQ(output, "ERROR: Timeout 30s.\n");
}
} // namespace ubse::ut::cli
