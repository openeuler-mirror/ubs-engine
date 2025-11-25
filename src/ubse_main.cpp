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
#include <csignal>
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

UbseContext &g_ubseContext = UbseContext::GetInstance();

void SignalHandler(int signum)
{
    std::cout << "Received signal " << signum << std::endl;
    if (signum == SIGPIPE) {
        std::cout << "SIGPIPE signal received, ignore it." << std::endl;
        return;
    }

    g_globalStop.store(true); // 设置全局停止标志
    g_globalCv.notify_all(); // 唤醒所有等待线程
}
} // namespace ubse

using namespace ubse;

int main(int argc, char *argv[])
{
    InitializeAuditFunctions();
    // 注册exit信号,实现优雅退出
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGPIPE, SignalHandler);

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
    // 停止上下文
    g_ubseContext.Stop();
    AuditDestroy();
    return 0;
}