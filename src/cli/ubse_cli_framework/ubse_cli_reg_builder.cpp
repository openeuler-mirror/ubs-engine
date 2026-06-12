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
#include "ubse_cli_reg_builder.h"

namespace ubse::cli::framework {
UbseCliRegBuilder::UbseCliRegBuilder()
{
    this->commandInfo_.command = "";
    this->commandInfo_.type = "";
    this->commandInfo_.options = {};
    this->commandInfo_.commandFunc = nullptr;
}

UbseCliRegBuilder& UbseCliRegBuilder::UbseCliSetCommand(const std::string& command)
{
    this->commandInfo_.command = command;
    return *this;
}

UbseCliRegBuilder& UbseCliRegBuilder::UbseCliSetType(const std::string& type)
{
    this->commandInfo_.type = type;
    return *this;
}

UbseCliRegBuilder& UbseCliRegBuilder::UbseCliAddOption(const std::string& shortOpt, const std::string& longOpt,
                                                       const std::string& desc)
{
    UbseCliOptionsInfo optionInfo;
    optionInfo.shortOpt = shortOpt;
    optionInfo.longOpt = longOpt;
    optionInfo.desc = desc;
    this->commandInfo_.options.push_back(optionInfo);
    return *this;
}

UbseCliRegBuilder& UbseCliRegBuilder::UbseCliAddFlagOption(const std::string& shortOpt, const std::string& longOpt,
                                                           const std::string& desc)
{
    UbseCliOptionsInfo optionInfo;
    optionInfo.shortOpt = shortOpt;
    optionInfo.longOpt = longOpt;
    optionInfo.desc = desc;
    optionInfo.isFlag = true;
    this->commandInfo_.options.push_back(optionInfo);
    return *this;
}

UbseCliRegBuilder& UbseCliRegBuilder::UbseCliSetFunc(UbseCliCommandFunc func)
{
    this->commandInfo_.commandFunc = func;
    return *this;
}

UbseCliCommandInfo UbseCliRegBuilder::UbseCliBuild()
{
    return this->commandInfo_;
}
} // namespace ubse::cli::framework