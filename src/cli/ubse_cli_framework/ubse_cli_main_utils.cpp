/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_cli_main_utils.h"

#include <sys/time.h>
#include <unistd.h>
#include <cerrno>
#include <csignal>
#include <cstring>

#include "ubse_cli_whitelist.h"
#include "ubse_error.h"
#include "ubse_ipc_log.h"

namespace ubse::cli::framework {
namespace {
// 修改超时时间时，需要同步修改下方TIMEOUT_ERROR_MESSAGE。
constexpr size_t TIMEOUT_SECONDS = 30;
constexpr char TIMEOUT_ERROR_MESSAGE[] = "ERROR: Timeout 30s.\n";
constexpr size_t MIN_NUM_PARAMS = 1;
constexpr size_t MAX_NUM_PARAMS = 43;
constexpr size_t CHECK_PARAM_NUMS = 2;
constexpr size_t ODD_CHECK_INTERVAL = 2;
} // namespace

int UbseCliValidateStartupConditions(int argc, char *argv[])
{
    if (static_cast<size_t>(argc) < MIN_NUM_PARAMS || static_cast<size_t>(argc) > MAX_NUM_PARAMS) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("ERROR: Unsupported number of parameters");
        return UBSE_ERROR;
    }
    if (argv == nullptr) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("ERROR: System input data is empty.");
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

std::vector<std::string> UbseCliBuildArgs(int argc, char *argv[], size_t extraReserve)
{
    std::vector<std::string> args;
    if (argc <= 0 || argv == nullptr) {
        return args;
    }

    args.reserve(static_cast<size_t>(argc) + extraReserve);
    for (size_t i = 1; i < static_cast<size_t>(argc); ++i) {
        args.emplace_back(argv[i]);
    }
    return args;
}

int UbseCliValidateArgsByWhitelist(const std::vector<std::string> &args)
{
    UbseCliWhitelist whitelist;
    for (size_t i = 0; i < args.size(); ++i) {
        const size_t cliArgIndex = i + 1;
        // 保持历史校验规则：command/type和option name需校验，option value由各命令自行校验。
        if ((cliArgIndex <= CHECK_PARAM_NUMS && !whitelist.UbseCliIsAllowed(args[i])) ||
            (cliArgIndex % ODD_CHECK_INTERVAL == 1 && !whitelist.UbseCliIsAllowed(args[i]))) {
            UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("ERROR: Invalid characters in the whitelist.");
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

int UbseCliValidateCommandArgsCount(const std::vector<std::string> &args, const std::string &programName)
{
    if (args.size() < CHECK_PARAM_NUMS) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("ERROR: Unrecognized command.Please try '" +
                                                                     programName + " --help' for more info.\n");
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

int UbseCliStartTimeoutTimer()
{
    if (signal(SIGALRM, UbseCliTimeoutSignalHandler) == SIG_ERR) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("ERROR: Register signal handler failed.\n");
        return UBSE_ERROR;
    }

    struct itimerval timer {};
    timer.it_value.tv_sec = TIMEOUT_SECONDS;
    if (setitimer(ITIMER_REAL, &timer, nullptr) != 0) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("ERROR: Set timer failed. " +
                                                                     std::string(strerror(errno)) + "\n");
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

int UbseCliStopTimeoutTimer()
{
    // 全零itimerval用于关闭ITIMER_REAL。
    struct itimerval timer {};
    if (setitimer(ITIMER_REAL, &timer, nullptr) != 0) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("ERROR: Set timer failed. " +
                                                                     std::string(strerror(errno)) + "\n");
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseCliDisableIpcLog()
{
    ubse::ipc::UbseIpcLog::SetLogFunc([]([[maybe_unused]] uint32_t level, [[maybe_unused]] const char *message) {});
}

void UbseCliExecuteMatchedCommand(ubse::cli::reg::UbseCliModuleRegistry &registry)
{
    registry.UbseCliGetMatchCommand()
        .commandFunc(registry.UbseCliGetParseTool().UbseCliGetInputOptionMap())
        ->UbseCliDisplayResult();
}

void UbseCliTimeoutSignalHandler(int signum)
{
    if (signum == SIGALRM) {
        // 由于信号处理函数中不能使用printf等非异步安全函数，直接使用write打印超时信息。
        (void)write(STDOUT_FILENO, TIMEOUT_ERROR_MESSAGE, sizeof(TIMEOUT_ERROR_MESSAGE) - 1);
    }

    _exit(signum);
}
} // namespace ubse::cli::framework
