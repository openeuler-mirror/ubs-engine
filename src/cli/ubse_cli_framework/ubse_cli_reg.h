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

#ifndef UBSE_CLI_REG_H
#define UBSE_CLI_REG_H

#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include "ubse_cli_reg_builder.h"

namespace ubse::cli::reg {
using ubse::cli::framework::UbseCliCommandInfo;
using ubse::cli::framework::UbseCliOptionsInfo;
using ubse::cli::framework::UbseCliResultEcho;
using ubse::cli::framework::UbseCliStringEcho;
using ubse::cli::framework::UbseCliVariableCellEcho;
using ubse::cli::framework::UbseCliVariableCellInfo;

class UbseCliRegModule;

constexpr size_t UBSE_ALL_HELP_ARGS = 1;
constexpr size_t UBSE_CMD_HELP_ARGS = 3;
constexpr size_t UBSE_MAX_VALUE_LENGTH = 1024;
constexpr int UBSE_SPINNER_TIME = 250;

class UbseCliParse {
public:
    bool UbseCliArgsParse(const std::vector<std::string>& args);

    std::map<std::string, std::string>& UbseCliGetInputOptionMap()
    {
        return this->inputOptionMap_;
    }

    void UbseCliReset()
    {
        this->inputOptionMap_.clear();
    }

private:
    bool UbseCliArgsMapParse(const std::vector<std::string>& args);

    static bool UbseIsLongOption(const std::string& arg, bool& is_long_option);
    static std::string UbseCliGetInputOptionName(const std::string& arg, bool is_long_option);
    static bool UbseCliValidateOption(const std::string& option_name, bool is_long_option);
    bool UbseCliProcessOption(size_t& arg_index, const std::vector<std::string>& args, const std::string& option_name,
                              bool is_long_option);

private:
    std::map<std::string, std::string> inputOptionMap_{};
};

constexpr size_t UBSE_MAX_CMD_NUM = 20;
constexpr size_t UBSE_MAX_CMD_OR_TYPE_LENGTH = 64;
constexpr size_t UBSE_MAX_OPTIONS_LENGTH = 64;
constexpr size_t UBSE_MAX_OPTIONS_NUM = 10;
constexpr size_t UBSE_INDENT_SIZE = 45;

class UbseCliModuleRegistry {
public:
    using UbseCliModuleCreator = std::function<UbseCliRegModule*()>;

    static UbseCliModuleRegistry& GetInstance()
    {
        static UbseCliModuleRegistry instance;
        return instance;
    }

    UbseCliModuleRegistry(const UbseCliModuleRegistry&) = delete;

    UbseCliModuleRegistry(UbseCliModuleRegistry&&) = delete;

    UbseCliModuleRegistry& operator=(const UbseCliModuleRegistry&) = delete;

    UbseCliModuleRegistry& operator=(UbseCliModuleRegistry&&) = delete;

    void UbseCliLoadedModule(const std::string& module_name, UbseCliModuleCreator module_creator);

    void UbseCliCallAllModuleSignUp();

    void UbseCliRegister(std::vector<UbseCliCommandInfo>& commands_info);

    bool UbseCliCommandExist(const std::string& command_key);

    bool UbseCliHelpInfoParse(const std::vector<std::string>& args);

    void UbseCliReset()
    {
        this->creators_.clear();
        this->fullCommandInfo_.clear();
    }

    UbseCliCommandInfo& UbseCliGetMatchCommand()
    {
        return this->matchCommand_;
    }

    UbseCliParse& UbseCliGetParseTool()
    {
        return this->parseTool_;
    }

private:
    void UbseCliRegisterOptions(const std::string& command_key, const UbseCliCommandInfo& command_info);

    void UbseCliDisplayHelpInfo();

    void UbseCliDisplayCommandOptionsHelpInfo(const std::string& command, const std::string& type);

    void UbseCliDisplayParamsHelpInfo(const std::vector<UbseCliOptionsInfo>& options, size_t& line_width_limit);

    void UbseCliDisplayOptionInfoWithWidthLimit(const std::string& one_option_help_info, size_t line_width_limit,
                                                const std::string& delimiter) const;

private:
    UbseCliModuleRegistry() = default;
    UbseCliParse parseTool_{};
    std::unordered_map<std::string, UbseCliModuleCreator> creators_{};
    std::unordered_map<std::string, UbseCliCommandInfo> fullCommandInfo_;
    UbseCliCommandInfo matchCommand_{};
};

// Register module base class, all registered modules need to inherit this class.
class UbseCliRegModule {
public:
    virtual void UbseCliSignUp() = 0;

    virtual ~UbseCliRegModule() = default;

    static inline std::shared_ptr<UbseCliResultEcho> UbseCliStringPromptReply(const std::string& str)
    {
        return std::make_shared<UbseCliStringEcho>(str);
    }
    static inline std::shared_ptr<UbseCliResultEcho> UbseCliVariableCelReply(UbseCliVariableCellInfo variable_cell)
    {
        return std::make_shared<UbseCliVariableCellEcho>(variable_cell);
    }
    static inline bool DisableTimeoutTimer()
    {
        struct itimerval timer {};
        return setitimer(ITIMER_REAL, &timer, nullptr) == 0;
    }
    void UbseCliRegisterCmd()
    {
        UbseCliModuleRegistry::GetInstance().UbseCliRegister(this->cmd_);
    }
    std::vector<UbseCliCommandInfo> cmd_{};

#define UBSE_CLI_REGISTER_MODULE(name, type)                                                             \
    static struct Register##type##Helper_##__LINE__ {                                                    \
        Register##type##Helper_##__LINE__() noexcept                                                     \
        {                                                                                                \
            UbseCliModuleRegistry::GetInstance().UbseCliLoadedModule(name, []() { return new type(); }); \
        }                                                                                                \
    } g_register##type##Helper_##__LINE__
};

class UbseCliWaitIndicator {
public:
    explicit UbseCliWaitIndicator(std::string initial_message = "Processing")
        : message_(std::move(initial_message)),
          stop_(false)
    {
        printer_thread_ = std::thread(&UbseCliWaitIndicator::DisplayProgressSpinner, this);
    }
    ~UbseCliWaitIndicator()
    {
        Stop();
    }
    void Stop();

private:
    void DisplayProgressSpinner();
    std::string message_;
    std::atomic<bool> stop_{false};
    std::thread printer_thread_;
    mutable std::mutex io_mutex_;
};
} // namespace ubse::cli::reg
#endif
