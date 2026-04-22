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
    std::string cmdlineOutput;
    FILE* rawPipe = popen(cmd.data(), "r");
    if (!rawPipe) {
        return cmdlineOutput;
    }
    std::unique_ptr<FILE, decltype(&pclose)> pipe(rawPipe, pclose);

    char buffer[BUFFER_SIZE];
    size_t bytesRead = 0;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), pipe.get())) > 0) {
        for (size_t i = 0; i < bytesRead; ++i) {
            cmdlineOutput += (buffer[i] == '\0') ? ' ' : buffer[i];
        }
    }
    return cmdlineOutput;
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
    std::ifstream cmdlineFile("/proc/cmdline", std::ios::binary);
    if (!cmdlineFile.is_open()) {
        return result;
    }
    std::string cmdlineOutput((std::istreambuf_iterator<char>(cmdlineFile)),
                               std::istreambuf_iterator<char>());
    cmdlineFile.close();

    for (char& c : cmdlineOutput) {
        if (c == '\0') {
            c = ' ';
        }
    }
    if (cmdlineOutput.empty()) {
        return result;
    }
    std::regex pattern(R"((?:^|[[:space:]])(pmd_mapping=([0-9]+)%)(?=[[:space:]]|$))");
    std::smatch match;
    if (!std::regex_search(cmdlineOutput, match, pattern)) {
        return result;
    }
    std::string percentStr = match[2].str();
    int percentage = -1;
    try {
        percentage = std::stoi(percentStr);
    } catch (const std::exception& e) {
        return result;
    }
    if (percentage <= 0 || percentage > 100) {
        return result;
    }
    result = {"os", "pmd_mapping", std::move(percentStr)};
    return result;
}

std::optional<ConfigItem> RegisterAllocatorConfig()
{
    std::optional<ConfigItem> result;

    std::string cmdlineOutput = GetCmdLineResult("cat /sys/module/obmm/parameters/mempool_allocator");
    if (cmdlineOutput.empty()) {
        return result;
    }

    cmdlineOutput = trim(cmdlineOutput);
    if (!in_strings(cmdlineOutput, {"hugetlb_pmd", "hugetlb_pud", "buddy_highmem"})) {
        return result;
    }

    result = {"obmm", "mempool_allocator", std::move(cmdlineOutput)};
    return result;
}

std::optional<ConfigItem> RegisterOsPageSize()
{
    std::optional<ConfigItem> result;

    std::string cmdlineOutput = GetCmdLineResult("getconf PAGE_SIZE");
    if (cmdlineOutput.empty()) {
        return result;
    }

    cmdlineOutput = trim(cmdlineOutput);
    if (!in_strings(cmdlineOutput, {PAGE_SIZE_4K, PAGE_SIZE_64K})) {
        return result;
    }

    result = {"os", "page_size", std::move(cmdlineOutput)};
    return result;
}
RegisterConfigHelper pmdMappingRegister(RegisterPmdMappingConfig);
RegisterConfigHelper allocatorRegister(RegisterAllocatorConfig);
RegisterConfigHelper osPageSizeRegister(RegisterOsPageSize);
}