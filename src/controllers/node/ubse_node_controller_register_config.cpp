/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#include <string>
#include <regex>
#include <cstdio>
#include <memory>
#include <fstream>
#include <sstream>
#include "ubse_conf_module.h"
#include "ubse_mem_constants.h"
#include "ubse_logger_module.h"

namespace ubse::nodeController {
constexpr size_t BUFFER_SIZE = 1024;
using namespace ubse::config;
using namespace ubse::log;

UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::mem::strategy;

std::string GetCmdLineResult(std::string_view cmd)
{
    std::string cmdline_output;

    // 方案1：先检查popen结果，再创建unique_ptr
    FILE* raw_pipe = popen(cmd.data(), "r");
    if (!raw_pipe) {
        return cmdline_output;
    }

    std::unique_ptr<FILE, decltype(&pclose)> pipe(raw_pipe, pclose);

    char buffer[BUFFER_SIZE];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        cmdline_output += buffer;
    }

    return cmdline_output;
}


static bool in_strings(std::string_view s, std::initializer_list<std::string_view> list)
{
    return std::find(list.begin(), list.end(), s) != list.end();
}

static std::string trim(const std::string& str)
{
    if (str.empty()) {
        return str;
    }

    auto start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) {
        return "";
    }

    auto end = str.find_last_not_of(" \t\n\r\f\v");

    return str.substr(start, end - start + 1);
}

std::optional<ConfigItem> RegisterPmdMappingConfig()
{
    std::optional<ConfigItem> result;

    // 使用 C++ 标准库读取 /proc/cmdline 文件
    std::ifstream cmdline_file("/proc/cmdline");
    if (!cmdline_file.is_open()) {
        return result;
    }
    std::string cmdline_output;
    std::getline(cmdline_file, cmdline_output);

    if (cmdline_output.empty()) {
        return result;
    }
    // 使用 [[:space:]] 替代 \s 提高兼容性
    std::regex pattern(R"((?:^|[[:space:]])(pmd_mapping=([0-9]+)%)(?=[[:space:]]|$))");
    std::smatch match;

    if (!std::regex_search(cmdline_output, match, pattern)) {
        return result;
    }
    std::string percent_str = match[2].str();  // 跟正则表达式匹配只能是2
    int percentage = -1;
    try {
        percentage = std::stoi(percent_str);
    } catch (const std::exception& e) {
        return result;
    }

    if (percentage <= 0 || percentage > 100) {
        return result;
    }
    result = {"os", "pmd_mapping", std::move(percent_str)};
    return result;
}

std::optional<ConfigItem> RegisterAllocatorConfig()
{
    std::optional<ConfigItem> result;

    std::string cmdline_output = GetCmdLineResult("cat /sys/module/obmm/parameters/mempool_allocator");
    if (cmdline_output.empty()) {
        return result;
    }

    cmdline_output = trim(cmdline_output);
    if (!in_strings(cmdline_output, {"hugetlb_pmd", "hugetlb_pud", "buddy_highmem"})) {
        return result;
    }

    result = {"obmm", "mempool_allocator", std::move(cmdline_output)};
    return result;
}

std::optional<ConfigItem> RegisterOsPageSize()
{
    std::optional<ConfigItem> result;

    std::string cmdline_output = GetCmdLineResult("getconf PAGE_SIZE");
    if (cmdline_output.empty()) {
        return result;
    }

    cmdline_output = trim(cmdline_output);
    if (!in_strings(cmdline_output, {PAGE_SIZE_4K, PAGE_SIZE_64K})) {
        return result;
    }

    result = {"os", "page_size", std::move(cmdline_output)};
    return result;
}
RegisterConfigHelper pmdMappingRegister(RegisterPmdMappingConfig);
RegisterConfigHelper allocatorRegister(RegisterAllocatorConfig);
RegisterConfigHelper osPageSizeRegister(RegisterOsPageSize);
}