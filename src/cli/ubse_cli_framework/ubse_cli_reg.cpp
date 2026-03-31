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

#include "ubse_cli_reg.h"
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <iomanip>
#include "ubse_cli_whitelist.h"

namespace ubse::cli::reg {
using namespace ubse::cli::framework;

bool UbseCliParse::UbseCliArgsParse(const std::vector<std::string> &args)
{
    if (args.size() <
        2) { // The number of parameters must be greater than or equal to 2 in order to proceed with the parsing.
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation(
            "ERROR: Unrecognized command.Please try 'ubsectl --help' for more info.\n");
        return false;
    }
    std::string command_key = std::string(args[0] + "_" + args[1]);
    if (!UbseCliModuleRegistry::GetInstance().UbseCliCommandExist(command_key)) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation(
            "ERROR: Unrecognized command.Please try 'ubsectl --help' for more info.\n");
        return false;
    }
    if (args.size() < 3) { // A parameter count of less than 3 indicates that there are no parameter options available.
        return true;
    }
    auto match_command = UbseCliModuleRegistry::GetInstance().UbseCliGetMatchCommand();
    if (match_command.options.empty()) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("ERROR: The command '" + args[0] + " " + args[1] +
            "' does not support any long or short options.\n");
    }
    return UbseCliArgsMapParse(args);
}

bool UbseCliParse::UbseCliArgsMapParse(const std::vector<std::string> &args)
{
    size_t arg_index = 2; // The index for parameter options starts from 2.
    while (arg_index < args.size()) {
        const std::string &arg = args[arg_index];

        if (arg.empty()) {
            UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("ERROR: Unexpected argument '" + arg + "'.\n");
            return false;
        }

        bool is_long_option;
        if (!UbseIsLongOption(arg, is_long_option)) {
            return false;
        }

        std::string option_name = UbseCliGetInputOptionName(arg, is_long_option);
        if (!UbseCliValidateOption(option_name, is_long_option)) {
            return false;
        }

        if (!UbseCliProcessOption(arg_index, args, option_name, is_long_option)) {
            return false;
        }
    }
    return true;
}

bool UbseCliParse::UbseIsLongOption(const std::string &arg, bool &is_long_option)
{
    size_t long_dash_num = 2;  // The beginning of long options is marked by two(2) dashes (--).
    size_t short_dash_num = 1; // The beginning of short options is marked by one(1) dash (-).

    if (arg.length() > long_dash_num && arg.substr(0, long_dash_num) == "--") {
        is_long_option = true;
        return true;
    } else if (arg.length() >= short_dash_num && arg.substr(0, short_dash_num) == "-" &&
        arg.substr(1, short_dash_num) != "-") {
        is_long_option = false;
        return true;
    } else {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("ERROR: The format of '" + arg +
            "' is invalid.\n");
        return false;
    }
}

std::string UbseCliParse::UbseCliGetInputOptionName(const std::string &arg, bool is_long_option)
{
    size_t dash_count = is_long_option ? 2 : 1; // The beginning of opitons dash count is 2 or 1.
    return arg.substr(dash_count);
}

bool UbseCliParse::UbseCliValidateOption(const std::string &option_name, bool is_long_option)
{
    auto match_command = UbseCliModuleRegistry::GetInstance().UbseCliGetMatchCommand();
    for (const auto &option : match_command.options) {
        if ((is_long_option && option.longOpt == option_name) || (!is_long_option && option.shortOpt == option_name)) {
            return true;
        }
    }

    UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("ERROR: Unexpected option '" +
        std::string(is_long_option ? "--" : "-") + option_name + "'.\n");
    return false;
}

bool UbseCliParse::UbseCliProcessOption(size_t &arg_index, const std::vector<std::string> &args,
    const std::string &option_name, bool is_long_option)
{
    auto match_command = UbseCliModuleRegistry::GetInstance().UbseCliGetMatchCommand();
    for (const auto &option : match_command.options) {
        if ((is_long_option && option.longOpt == option_name) || (!is_long_option && option.shortOpt == option_name)) {
            std::string key = option.longOpt.empty() ? option.shortOpt : option.longOpt;
            if (inputOptionMap_.find(key) != inputOptionMap_.end()) {
                UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("ERROR: Duplicate option '" +
                    std::string(is_long_option ? "--" : "-") + option_name + "'.\n");
                return false;
            }
            if (arg_index + 1 >= args.size()) {
                UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("ERROR: Option '" +
                    std::string(is_long_option ? "--" : "-") + option_name + "' requires a value.\n");
                return false;
            }
            const std::string &value = args[arg_index + 1];
            if (value.size() > UBSE_MAX_VALUE_LENGTH) {
                UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation(
                    "ERROR: The length of the option value has been exceeded " + std::to_string(UBSE_MAX_VALUE_LENGTH) +
                    ".\n");
                return false;
            }
            inputOptionMap_[key] = value;
            arg_index += 2; // It has been ensured that +2 will not go out of bounds.
            return true;
        }
    }
    return false;
}

void UbseCliModuleRegistry::UbseCliLoadedModule(const std::string &module_name, UbseCliModuleCreator module_creator)
{
    if (module_creator == nullptr) {
        return;
    }
    // Each module name can have only one creator.
    if (this->creators_.find(module_name) == this->creators_.end()) {
        this->creators_[module_name] = std::move(module_creator);
    }
}

void UbseCliModuleRegistry::UbseCliCallAllModuleSignUp()
{
    for (const auto &module_creator : this->creators_) {
        if (module_creator.second == nullptr) {
            continue;
        }
        auto module = module_creator.second();
        if (module == nullptr) {
            continue;
        }
        module->UbseCliSignUp();
        module->UbseCliRegisterCmd();
        delete module;
    }
    this->creators_.clear();
}

void UbseCliModuleRegistry::UbseCliRegister(std::vector<UbseCliCommandInfo> &commands_info)
{
    if (commands_info.empty()) {
        return;
    }
    if (commands_info.size() > UBSE_MAX_CMD_NUM) {
        return;
    }
    UbseCliWhitelist whitelist;
    for (const auto &command_info : commands_info) {
        if (command_info.command.empty() || command_info.type.empty()) {
            continue;
        }
        if (command_info.command.size() > UBSE_MAX_CMD_OR_TYPE_LENGTH ||
            command_info.type.size() > UBSE_MAX_CMD_OR_TYPE_LENGTH) {
            continue;
        }
        if (command_info.commandFunc == nullptr) {
            continue;
        }
        if (!whitelist.UbseCliIsAllowed(command_info.command) || !whitelist.UbseCliIsAllowed(command_info.type)) {
            continue;
        }
        std::string command_key = std::string(command_info.command + "_" + command_info.type);
        if (this->fullCommandInfo_.find(command_key) != this->fullCommandInfo_.end()) {
            continue;
        }
        if (!command_info.options.empty()) {
            UbseCliRegisterOptions(command_key, command_info);
        } else {
            this->fullCommandInfo_.insert(std::make_pair(command_key,
                UbseCliCommandInfo{ command_info.command, command_info.type, {}, command_info.commandFunc }));
        }
    }
}

void UbseCliModuleRegistry::UbseCliRegisterOptions(const std::string &command_key,
    const UbseCliCommandInfo &command_info)
{
    std::unordered_set<std::string> options_set{};
    std::vector<UbseCliOptionsInfo> filtered_options{};
    for (const auto &command_option : command_info.options) {
        if (command_option.shortOpt.empty() || command_option.longOpt.empty()) {
            continue;
        }
        if (command_option.shortOpt.size() > UBSE_MAX_OPTIONS_LENGTH ||
            command_option.longOpt.size() > UBSE_MAX_OPTIONS_LENGTH) {
            continue;
        }
        if (command_option.desc.empty()) {
            continue;
        }
        if (options_set.find(command_option.shortOpt) != options_set.end() ||
            options_set.find(command_option.longOpt) != options_set.end()) {
            continue;
        }
        options_set.insert(command_option.shortOpt);
        options_set.insert(command_option.longOpt);
        filtered_options.emplace_back(
            UbseCliOptionsInfo{ command_option.shortOpt, command_option.longOpt, command_option.desc });
    }
    if (filtered_options.size() > UBSE_MAX_OPTIONS_NUM || filtered_options.size() == 0) {
        return;
    }
    this->fullCommandInfo_.insert(std::make_pair(command_key,
        UbseCliCommandInfo{ command_info.command, command_info.type, filtered_options, command_info.commandFunc }));
}

bool UbseCliModuleRegistry::UbseCliCommandExist(const std::string &command_key)
{
    if (auto iter = this->fullCommandInfo_.find(command_key); iter != this->fullCommandInfo_.end()) {
        this->matchCommand_ = iter->second;
        return true;
    }
    return false;
}

bool UbseCliModuleRegistry::UbseCliHelpInfoParse(const std::vector<std::string> &args)
{
    const size_t size = args.size();
    if (size == 0) {
        UbseCliModuleRegistry::GetInstance().UbseCliDisplayHelpInfo();
        return true;
    }
    const std::string &last_arg = args.back();
    if (last_arg != "-h" && last_arg != "--help") {
        return false;
    }

    switch (size) {
        case UBSE_ALL_HELP_ARGS:
            UbseCliModuleRegistry::GetInstance().UbseCliDisplayHelpInfo();
            return true;
        case UBSE_CMD_HELP_ARGS:
            if (args[0].length() > UBSE_MAX_CMD_OR_TYPE_LENGTH || args[1].length() > UBSE_MAX_CMD_OR_TYPE_LENGTH) {
                return false;
            }
            UbseCliModuleRegistry::GetInstance().UbseCliDisplayCommandOptionsHelpInfo(args[0], args[1]);
            return true;
        default:
            UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation(
                "ERROR: Please enter ubsectl -h or --help for more info.");
            return true;
    }
}

void UbseCliModuleRegistry::UbseCliDisplayHelpInfo()
{
    struct winsize win {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) < 0) {
        win.ws_col = UBSE_DEFAULT_SCREEN_WIDTH;
    }
    size_t line_width_limit = win.ws_col;
    if (this->fullCommandInfo_.empty()) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("INFO: No commands have been registered yet.\n");
        return;
    }
    for (const auto &key_command_info : this->fullCommandInfo_) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("  Usage: ubsectl " +
            key_command_info.second.command + " " + key_command_info.second.type + "[OPTIONS]\nOPTIONS:\n");
        UbseCliDisplayParamsHelpInfo(key_command_info.second.options, line_width_limit);
    }
}

void UbseCliModuleRegistry::UbseCliDisplayCommandOptionsHelpInfo(const std::string &command, const std::string &type)
{
    struct winsize win {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) < 0) {
        win.ws_col = UBSE_DEFAULT_SCREEN_WIDTH;
    }
    size_t line_width_limit = win.ws_col;
    std::string key = command + "_" + type;
    if (this->fullCommandInfo_.find(key) != this->fullCommandInfo_.end()) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation("OPTIONS:\n");
        UbseCliDisplayParamsHelpInfo(this->fullCommandInfo_[key].options, line_width_limit);
    } else {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation(
            "INFO: The command does not exist or does not support parameters.Please try 'ubsectl "
            "--help' for more info.");
    }
}

void UbseCliModuleRegistry::UbseCliDisplayParamsHelpInfo(const std::vector<UbseCliOptionsInfo> &options,
    size_t &line_width_limit)
{
    if (options.empty()) {
        UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation(
            "\tThe command does not support any option arguments.");
    }
    for (const auto &option : options) {
        std::stringstream stream;
        int opt_gap = 3;              // The distance between long and short options.
        int min_remaining_width = 32; // To prevent excessive line breaks in help information caused
                                      // by word wrapping, reserve NO_32 positions.
        if (line_width_limit <= UBSE_INDENT_SIZE + static_cast<unsigned long>(min_remaining_width)) {
            stream << "\t    -" << std::left << std::setw(opt_gap) << option.shortOpt << ",--" << std::left
                   << std::setw(min_remaining_width) << option.longOpt << "\n\n"
                   << std::left << "\t" << option.desc << std::endl;
            std::string one_option_help_info = stream.str();
            UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation(one_option_help_info);
            std::cout << std::endl;
        } else {
            stream << "    -" << std::left << std::setw(opt_gap) << option.shortOpt << ",--" << std::left <<
                std::setw(min_remaining_width) << option.longOpt << "  " << std::left << option.desc << std::endl;

            std::string one_option_help_info = stream.str();
            UbseCliDisplayOptionInfoWithWidthLimit(one_option_help_info, line_width_limit, "\n");
        }
    }
    std::cout << std::endl;
}

void UbseCliModuleRegistry::UbseCliDisplayOptionInfoWithWidthLimit(const std::string &one_option_help_info,
    size_t line_width_limit, const std::string &delimiter) const
{
    size_t option_info_str_length = one_option_help_info.length();
    size_t left_index = 0;      // traverse the indices of a string.
    bool is_first_line = true;  // If the index located in the first.
    size_t each_line_end_index; // the index at the end of each line.
    size_t line_width_without_indent =
        line_width_limit - UBSE_INDENT_SIZE;   // the remaining width after removing formatting.
    std::string indent(UBSE_INDENT_SIZE, ' '); // prefix empty lines.
    std::string prefix = delimiter + indent;   // line breaks and format prefixes.

    while (left_index < option_info_str_length) {
        each_line_end_index = std::min(left_index + (is_first_line ? line_width_limit : line_width_without_indent),
            option_info_str_length);
        is_first_line = false;

        size_t line_end_index = each_line_end_index;
        if (line_end_index < option_info_str_length && !std::isspace(one_option_help_info[each_line_end_index])) {
            while (line_end_index > left_index && !std::isspace(one_option_help_info[line_end_index - 1])) {
                line_end_index--;
            }
            if (line_end_index == left_index) {
                line_end_index = each_line_end_index;
            }
        }

        std::cout << one_option_help_info.substr(left_index, line_end_index - left_index);
        // determine if it is at the end.
        if (line_end_index == option_info_str_length - 1) {
            std::cout << delimiter;
        } else if (line_end_index < option_info_str_length - 1) {
            std::cout << prefix;
        }
        left_index = line_end_index;
        // skip blank strings.
        while (left_index < option_info_str_length && std::isspace(one_option_help_info[left_index])) {
            left_index++;
        }
    }
}

void UbseCliWaitIndicator::DisplayProgressSpinner()
{
    const char *spinner = "-\\|/"; // 纯 ASCII
    const int spinnerLen = 4;
    auto start = std::chrono::steady_clock::now();
    size_t frame = 0;
    while (!stop_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start);
        int total_sec = static_cast<int>(elapsed.count());
        int hours = total_sec / 3600;
        int minutes = (total_sec % 3600) / 60;
        int seconds = total_sec % 60;
        std::ostringstream oss;
        oss << spinner[frame % spinnerLen] << " " << message_ << " [elapsed: ";
        if (hours > 0) {
            oss << hours << "h " << minutes << "m " << seconds << "s";
        } else if (minutes > 0) {
            oss << minutes << "m " << seconds << "s";
        } else {
            oss << seconds << "s";
        }
        oss << "]";
        std::lock_guard<std::mutex> lock(io_mutex_);
        // 先清空当前终端行、立即显示当前的最新文本、确保用户看到实时的等待信息
        std::cout << "\r\033[K" << oss.str() << std::flush;
        frame++;
        std::this_thread::sleep_for(std::chrono::milliseconds(UBSE_SPINNER_TIME));
    }
}

void UbseCliWaitIndicator::Stop()
{
    if (stop_.exchange(true)) {
        return;
    }
    if (printer_thread_.joinable()) {
        printer_thread_.join();
    }
    std::lock_guard<std::mutex> lock(io_mutex_);
    std::cout << "\r\033[K" << std::flush; // 回到行首 + 清空整行
}
} // namespace ubse::cli::reg