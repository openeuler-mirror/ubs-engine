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

#include <dlfcn.h>
#include <unistd.h>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <thread>

#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_audit.h"

namespace ubse {
using namespace ubse::context;
using namespace ubse::log;

UbseContext& g_ubseContext = UbseContext::GetInstance();

void SignalHandler(int signum)
{
    // 信号处理函数内只允许调用异步信号安全(async-signal-safe)函数：
    // std::cout、condition_variable::notify_all均可能持锁，若信号打断持锁线程会死锁，
    // 此处仅使用write输出并设置原子标志，等待线程通过带超时的wait_for轮询退出
    if (signum == SIGPIPE) {
        return;
    }
    // 本handler仅注册于SIGINT/SIGTERM，查表输出信号名即可，避免非信号安全的int转字符串
    const char* msg = (signum == SIGINT) ? "Received SIGINT signal\n" : "Received SIGTERM signal\n";
    auto unused = write(STDOUT_FILENO, msg, strlen(msg));
    (void)unused;
    g_globalStop.store(true); // 设置全局停止标志
}
} // namespace ubse

using namespace ubse;

int main(int argc, char* argv[])
{
    InitializeAuditFunctions();
    // 注册exit信号,实现优雅退出
    if (signal(SIGINT, SignalHandler) == SIG_ERR) {
        std::cerr << "signal=SIGINT error=register_failed" << std::endl;
        return static_cast<int>(UBSE_ERR_OPERATION_FAILED);
    }
    if (signal(SIGTERM, SignalHandler) == SIG_ERR) {
        std::cerr << "signal=SIGTERM error=register_failed" << std::endl;
        return static_cast<int>(UBSE_ERR_OPERATION_FAILED);
    }
    if (signal(SIGPIPE, SignalHandler) == SIG_ERR) {
        std::cerr << "signal=SIGPIPE error=register_failed" << std::endl;
        return static_cast<int>(UBSE_ERR_OPERATION_FAILED);
    }

    // 启动上下文
    UbseResult ret = g_ubseContext.Run(argc, argv);
    if (ret != UBSE_OK) {
        std::cerr << "UbseContext::Run failed" << std::endl;
        ubse::g_ubseContext.Stop();
        AuditDestroy();
        std::cerr << "ubse service failed to start." << std::endl;
        return static_cast<int>(ret);
    }
    std::cout << "ubse service started successfully." << std::endl;

    while (!g_globalStop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 100ms
    }
    g_globalCv.notify_all(); // 唤醒所有等待线程
    // 停止上下文
    g_ubseContext.Stop();
    AuditDestroy();
    return 0;
}