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

#include "test_ubs_engine_log.h"

#include <algorithm>
#include <fstream>

#include "ubs_engine_log.h"
#include "ubse_ipc_log.h"
#include "ubse_ut_dir.h"

namespace ubse::sdk::ut {
const std::string LOG_PATH = std::string(UT_DIRECTORY) + "sdk.log";
void TestUbsEngineLog::SetUp()
{
    Test::SetUp();
}

void TestUbsEngineLog::TearDown()
{
    std::remove(LOG_PATH.c_str());
    Test::TearDown();
}

TEST_F(TestUbsEngineLog, LogCallbackRegisterUsesStdout)
{
    // 1. 注册 nullptr → 使用 stdout 输出
    ubs_engine_log_callback_register(nullptr);
    StdoutCapture cap;

    // 测试打日志
    IPC_LOG_INFO << "Info log";
    IPC_LOG_DEBUG << "Debug log";
    IPC_LOG_ERROR << "ERROR log";
    IPC_LOG_WARN << "Warn log";

    std::string out = cap.output();
    EXPECT_NE(out.find("Info log"), std::string::npos);
    EXPECT_NE(out.find("Debug log"), std::string::npos);
    EXPECT_NE(out.find("ERROR log"), std::string::npos);
    EXPECT_NE(out.find("Warn log"), std::string::npos);
}

void FileLogCallback(uint32_t level, const char *msg)
{
    std::ofstream logFile(LOG_PATH, std::ios::app);
    if (logFile.is_open()) {
        logFile << "[" << level << "] " << (msg ? msg : "(null)") << "\n";
        logFile.close();
    } else {
        std::cerr << "Failed to open log file." << std::endl;
    }
}

std::string ReadFile()
{
    std::ifstream ifs(LOG_PATH);
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

TEST_F(TestUbsEngineLog, LogCallbackRegisterUsesLogFile)
{
    // 1. 注册 nullptr → 使用 文件 输出
    ubs_engine_log_callback_register(FileLogCallback);

    // 测试打日志
    IPC_LOG_INFO << "Info log";
    IPC_LOG_DEBUG << "Debug log";
    IPC_LOG_ERROR << "ERROR log";
    IPC_LOG_WARN << "Warn log";

    std::string out = ReadFile();
    EXPECT_NE(out.find("Info log"), std::string::npos);
    EXPECT_NE(out.find("Debug log"), std::string::npos);
    EXPECT_NE(out.find("ERROR log"), std::string::npos);
    EXPECT_NE(out.find("Warn log"), std::string::npos);
}
} // namespace ubse::sdk::ut