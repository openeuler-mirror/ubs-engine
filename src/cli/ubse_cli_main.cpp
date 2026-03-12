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
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>

#include "ubse_cli_reg.h"
#include "ubse_cli_whitelist.h"
#include "ubse_error.h"

using namespace ubse::cli::reg;
using namespace ubse::cli::framework;

constexpr size_t TIMEOUT_SECONDS = 30;
constexpr unsigned int ROOT_USER_ID = 0;
constexpr size_t MIN_NUM_PARAMS = 1;
constexpr size_t MAX_NUM_PARAMS = 43;
constexpr size_t CHECK_PARAM_NUMS = 2;

namespace {
void SignalHandler(int signum)
{
    if (signum == SIGALRM) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation(
            std::string("ERROR: Timeout " + std::to_string(TIMEOUT_SECONDS) + "s.\n"));
    }
    exit(signum);
}
} // namespace

int ValidateStartupConditions(int argc, char *argv[])
{
    if (geteuid() == ROOT_USER_ID) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation(
            "ERROR: The current program does not support the root user. Switch to a user in the ubse group.");
        return UBSE_ERROR;
    }
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

int main(int argc, char *argv[])
{
    auto validRet = ValidateStartupConditions(argc, argv);
    if (validRet != UBSE_OK) {
        return validRet;
    }

    UbseCliModuleRegistry::GetInstance().UbseCliCallAllModuleSignUp();
    UbseCliWhitelist whitelist;
    std::vector<std::string> args;
    args.reserve(static_cast<unsigned long>(argc));
    for (size_t i = 1; i < static_cast<size_t>(argc); ++i) {
        if ((i <= CHECK_PARAM_NUMS && !whitelist.UbseCliIsAllowed(std::string(argv[i]))) ||
            (i % 2 == 1 && !whitelist.UbseCliIsAllowed(std::string(argv[i])))) {
            UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("ERROR: Invalid characters in the whitelist.");
            return UBSE_ERROR;
        }
        args.emplace_back(argv[i]);
    }
    if (UbseCliModuleRegistry::GetInstance().UbseCliHelpInfoParse(args)) {
        return UBSE_OK;
    }
    if (args.size() < CHECK_PARAM_NUMS) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation(
            "ERROR: Unrecognized command.Please try 'ubsectl --help' for more info.\n");
        return UBSE_ERROR;
    }
    if (!UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse(args)) {
        return UBSE_ERROR;
    }
    signal(SIGALRM, SignalHandler);
    struct itimerval timer {};
    timer.it_value.tv_sec = TIMEOUT_SECONDS;
    if (setitimer(ITIMER_REAL, &timer, nullptr) != 0) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("ERROR: Set timer failed. " +
                                                                     std::string(strerror(errno)) + "\n");
        return UBSE_ERROR;
    }
    UbseCliModuleRegistry::GetInstance()
        .UbseCliGetMatchCommand()
        .commandFunc(UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliGetInputOptionMap())
        ->UbseCliDisplayResult();
    if (setitimer(ITIMER_REAL, nullptr, nullptr) != 0) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("ERROR: Set timer failed. " +
                                                                     std::string(strerror(errno)) + "\n");
        return UBSE_ERROR;
    }
    return UBSE_OK;
}