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
#include <string>

#include "ubse_cli_main_utils.h"
#include "ubse_cli_reg.h"
#include "ubse_error.h"

using namespace ubse::cli::reg;
using namespace ubse::cli::framework;

const std::string SSU_TYPE = "ssu";

int main(int argc, char *argv[])
{
    auto validRet = UbseCliValidateStartupConditions(argc, argv);
    if (validRet != UBSE_OK) {
        return validRet;
    }

    auto &registry = UbseCliModuleRegistry::GetInstance();
    registry.UbseCliCallAllModuleSignUp();
    registry.UbseCliSetProgramContext("ubsectl-ssu", false);

    auto args = UbseCliBuildArgs(argc, argv, 1);
    if (args.size() == 1 && registry.UbseCliHelpInfoParse(args)) {
        return UBSE_OK;
    }
    if (!args.empty()) {
        args.insert(args.begin() + 1, SSU_TYPE);
    }

    validRet = UbseCliValidateArgsByWhitelist(args);
    if (validRet != UBSE_OK) {
        return validRet;
    }
    if (registry.UbseCliHelpInfoParse(args)) {
        return UBSE_OK;
    }
    validRet = UbseCliValidateCommandArgsCount(args, registry.UbseCliGetProgramName());
    if (validRet != UBSE_OK) {
        return validRet;
    }
    if (!registry.UbseCliGetParseTool().UbseCliArgsParse(args)) {
        return UBSE_ERROR;
    }
    validRet = UbseCliStartTimeoutTimer();
    if (validRet != UBSE_OK) {
        return validRet;
    }
    UbseCliDisableIpcLog();
    UbseCliExecuteMatchedCommand(registry);
    validRet = UbseCliStopTimeoutTimer();
    if (validRet != UBSE_OK) {
        return validRet;
    }
    return UBSE_OK;
}
