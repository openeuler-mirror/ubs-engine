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

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "dbg_deadloop_it.h"

namespace ubse::it::deadloop {
using namespace ubse::deadloop;
using namespace ubse::context;
using namespace ubse::log;

int32_t ITestCmdDeadloopEnable(ProcessMmap* pMmap)
{
    std::cout << "======================ITestCmdDeadloopEnable====================" << std::endl;
    // 获取当前时间戳，用于筛选日志
    auto currentTime = std::chrono::system_clock::now();
    UbseContext& ubseContext = UbseContext::GetInstance();
    // 判断依赖模块是否存在
    auto ubseLoggerModule = ubseContext.GetModule<UbseLoggerModule>();
    if (ubseLoggerModule == nullptr) {
        return UBSE_ERROR;
    }
    auto ubseDeadloopModule = ubseContext.GetModule<UbseDeadloopModule>();
    if (ubseDeadloopModule == nullptr) {
        return UBSE_ERROR;
    }

    UbseResult ret;
    // 设置开关
    ret = ubseDeadloopModule->SetDeadLoopSwitch(true);
    if (ret != UBSE_OK) {
        std::cout << "SetDeadLoopSwitch ERROR" << std::endl;
        return ret;
    }
    // 设置阈值
    ret = ubseDeadloopModule->SetOverRunThreshold(5); // 设置死循环超时阈值为5s
    if (ret != UBSE_OK) {
        std::cout << "SetOverRunThreshold ERROR" << std::endl;
        return ret;
    }
    // 设置钩子函数
    UBSEDEADLOOPHOOK pfnHandler = [](pthread_t taskId, uint32_t elapsedSeconds) {
        std::cout << "===========DeadLoopHook EXCUTE============" << std::endl;
        return UBSE_OK;
    };
    ret = ubseDeadloopModule->RegDeadLoopHandler(pfnHandler);
    if (ret != UBSE_OK) {
        std::cout << "RegDeadLoopHandler ERROR" << std::endl;
        return ret;
    }
    // 开始检测
    ubseDeadloopModule->DeadLoopTaskBegin();
    std::this_thread::sleep_for(seconds(10)); // 线程睡眠10秒
    ubseDeadloopModule->DeadLoopTaskEnd();

    std::vector<std::string> matchedLogs{};
    std::string logfilename = GetLogPath();
    findLogsWithGrep(logfilename, currentTime, "SetDeadLoopSwitch", matchedLogs);
    findLogsWithGrep(logfilename, currentTime, "SetOverRunThreshold", matchedLogs);
    findLogsWithGrep(logfilename, currentTime, "DeadLoopTaskBegin", matchedLogs);
    findLogsWithGrep(logfilename, currentTime, "DeadLoopTaskCheck", matchedLogs);
    findLogsWithGrep(logfilename, currentTime, "DeadLoopTaskEnd", matchedLogs);
    std::cout << "==================log===============" << std::endl;
    for (auto log : matchedLogs) {
        std::cout << log << std::endl;
    }
    std::cout << "===================== ITestCmdDeadloopEnable===================" << std::endl;
    return UBSE_OK;
}

int32_t ITestCmdDeadloopDisable(ProcessMmap* pMmap)
{
    std::cout << "======================ITestCmdDeadloopDisable===================" << std::endl;
    auto currentTime = std::chrono::system_clock::now();
    UbseContext& ubseContext = UbseContext::GetInstance();
    // 判断依赖模块是否存在
    auto ubseLoggerModule = ubseContext.GetModule<UbseLoggerModule>();
    if (ubseLoggerModule == nullptr) {
        return UBSE_ERROR;
    }
    auto ubseDeadloopModule = ubseContext.GetModule<UbseDeadloopModule>();
    if (ubseDeadloopModule == nullptr) {
        return UBSE_ERROR;
    }

    UbseResult ret;
    // 设置开关
    ret = ubseDeadloopModule->SetDeadLoopSwitch(false);
    if (ret != UBSE_OK) {
        std::cout << "SetDeadLoopSwitch ERROR" << std::endl;
        return ret;
    }
    // 设置阈值
    ret = ubseDeadloopModule->SetOverRunThreshold(5); // 设置死循环超时阈值为5s
    if (ret != UBSE_OK) {
        std::cout << "SetOverRunThreshold ERROR" << std::endl;
        return ret;
    }
    // 设置钩子函数
    UBSEDEADLOOPHOOK pfnHandler = [](pthread_t taskId, uint32_t elapsedSeconds) {
        std::cout << "===========DeadLoopHook EXCUTE============" << std::endl;
        return UBSE_OK;
    };
    ret = ubseDeadloopModule->RegDeadLoopHandler(pfnHandler);
    if (ret != UBSE_OK) {
        std::cout << "RegDeadLoopHandler ERROR" << std::endl;
        return ret;
    }
    // 开始检测
    ubseDeadloopModule->DeadLoopTaskBegin();
    std::this_thread::sleep_for(seconds(7)); // 线程睡眠7秒
    ubseDeadloopModule->DeadLoopTaskEnd();

    std::vector<std::string> matchedLogs{};
    std::string logfilename = GetLogPath();
    findLogsWithGrep(logfilename, currentTime, "SetDeadLoopSwitch", matchedLogs);
    findLogsWithGrep(logfilename, currentTime, "SetOverRunThreshold", matchedLogs);
    findLogsWithGrep(logfilename, currentTime, "DeadLoopTaskBegin", matchedLogs);
    findLogsWithGrep(logfilename, currentTime, "DeadLoopTaskCheck", matchedLogs);
    findLogsWithGrep(logfilename, currentTime, "DeadLoopTaskEnd", matchedLogs);
    std::cout << "==================log===============" << std::endl;
    for (auto log : matchedLogs) {
        std::cout << log << std::endl;
    }
    std::cout << "===================== ITestCmdDeadloopDisable===================" << std::endl;
    return UBSE_OK;
}

// 过滤日志，确认日志中输出的信息无误
void findLogsWithGrep(const std::string& filename, std::chrono::system_clock::time_point& currentTime,
                      const std::string& keyword, std::vector<std::string>& matchedLogs)
{
    // 构造 grep 命令，查找包含关键字的行
    std::string command = "grep -a '" + keyword + "' " + filename;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);

    if (!pipe) {
        std::cerr << "Failed to run grep command." << std::endl;
        return;
    }

    char buffer[512];
    std::regex timestampRegex(R"(\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\.\d+)");

    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        std::string line = buffer;

        // 使用正则表达式解析时间戳
        std::smatch match;
        if (std::regex_search(line, match, timestampRegex)) {
            std::string timestampStr = match[1];
            auto logTime = ParseTimestamp(timestampStr);
            // 检查时间戳是否在前1秒钟，后10秒钟内
            if (logTime >= currentTime - std::chrono::seconds(1) && logTime <= currentTime + std::chrono::seconds(10)) {
                matchedLogs.push_back(line); // 匹配的行存入结果
            }
        }
    }
}

// 将时间戳解析为时间对象
std::chrono::system_clock::time_point ParseTimestamp(const std::string& timestamp)
{
    std::tm tm = {};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        throw std::runtime_error("Failed to parse timestamp");
    }
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::string GetLogPath()
{
    std::filesystem::path currentPath = std::filesystem::current_path();
    // 相对路径
    std::string relativePath = "test/IT/manager_log/ubse.log";
    std::filesystem::path basePath = currentPath;
    basePath = basePath.parent_path();
    basePath = basePath.parent_path();
    basePath = basePath.parent_path();
    // 拼接当前工作目录与相对路径
    std::filesystem::path absolutePath = basePath / relativePath;
    return absolutePath.string();
}
} // namespace ubse::it::deadloop