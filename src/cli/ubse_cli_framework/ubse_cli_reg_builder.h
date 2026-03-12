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

#ifndef UBSE_CLI_REG_BUILDER_H
#define UBSE_CLI_REG_BUILDER_H

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "ubse_cli_res_builder.h"

namespace ubse::cli::framework {
using UbseCliCommandFunc = std::shared_ptr<UbseCliResultEcho> (*)([
    [maybe_unused]] const std::map<std::string, std::string> &);

struct UbseCliOptionsInfo {
    std::string shortOpt;
    std::string longOpt;
    std::string desc;

    bool operator == (const UbseCliOptionsInfo &other) const
    {
        return shortOpt == other.shortOpt && longOpt == other.longOpt && desc == other.desc;
    }
};

struct UbseCliCommandInfo {
    std::string command;
    std::string type;
    std::vector<UbseCliOptionsInfo> options;
    UbseCliCommandFunc commandFunc;

    bool operator == (const UbseCliCommandInfo &other) const
    {
        return command == other.command && type == other.type && options == other.options &&
            commandFunc == other.commandFunc;
    }
};

class UbseCliRegBuilder {
public:
    UbseCliRegBuilder();

    UbseCliRegBuilder &UbseCliSetCommand(const std::string &command);

    UbseCliRegBuilder &UbseCliSetType(const std::string &type);

    UbseCliRegBuilder &UbseCliAddOption(const std::string &short_opt, const std::string &long_opt,
        const std::string &desc);

    UbseCliRegBuilder &UbseCliSetFunc(UbseCliCommandFunc func);

    UbseCliCommandInfo UbseCliBuild();

private:
    UbseCliCommandInfo commandInfo_;
};
} // namespace ubse::cli::framework
#endif