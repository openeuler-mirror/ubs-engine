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

#include "test_pt_utils.h"
#include <chrono>
#include <iostream>

namespace ubse::pt::utils {
char** g_argv;

void SetArgv(char** argv)
{
    g_argv = argv;
}

char** GetArgv()
{
    return g_argv;
}

inline uint64_t GetCurrentTimeInMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string GetIp()
{
    const char* shellstr = R"(ip -4 addr show | grep -v '127.0.0.1' | grep -oP '(?<=inet\s)\d+(\.\d+){3}')";
    FILE* fp = popen(shellstr, "r");

    if (fp == nullptr) {
        return "";
    }

    char line[20];
    std::string result = "";
    if (fgets(line, sizeof(line), fp) != nullptr) {
        result = std::string(line);
    }
    pclose(fp);
    for (int i = 0; i < result.length(); i++) {
        if (result[i] == '\n') {
            result.erase(i);
        }
    }
    return result;
}
} // namespace ubse::pt::utils