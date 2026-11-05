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

#include "test_virtagent_exit_guard.h"

#include <dirent.h>
#include <unistd.h>
#include <cstdio>
#include <mutex>

#include <gtest/gtest.h>

namespace {
// 退出期等待后台线程自然结束的最长时间（秒）
constexpr int MAX_EXIT_WAIT_SECONDS = 30;

// 统计当前进程的线程数（读 /proc/self/task）；读取失败时按 1 个线程处理，即不做等待
int CountProcessThreads()
{
    DIR* dir = opendir("/proc/self/task");
    if (dir == nullptr) {
        return 1;
    }
    int count = 0;
    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] != '.') {
            ++count;
        }
    }
    closedir(dir);
    return count;
}

// 说明：virtagent 用例会启动若干异步任务线程（如 mem_fragmentation 的异步借/还内存线程，均以 detach 方式创建），
// 用例跑完后这些线程可能仍在执行被测模块代码。若进程在退出期与它们并发开始静态析构，
// 会出现 "pure virtual method called" / "terminate called without an active exception"，
// 进程最终以 SIGABRT(134) 退出：用例已全部通过、xml 报告也已落盘，但 DT 门禁仍被判失败，
// 且覆盖率数据来不及写盘。这里在退出期（静态析构之前）等待这些后台线程自然结束，
// 线程全部退出后恢复正常退出流程，避免上述误判，同时保证覆盖率数据正常落盘。
void VirtAgentExitGuard()
{
    auto* unitTest = testing::UnitTest::GetInstance();
    // 有用例失败时保持原有退出流程，让失败继续体现在退出码上，避免掩盖真实失败
    if (unitTest == nullptr || unitTest->test_to_run_count() <= 0 || unitTest->failed_test_count() != 0) {
        return;
    }
    for (int seconds = 0; seconds < MAX_EXIT_WAIT_SECONDS; ++seconds) {
        int threadCount = CountProcessThreads();
        if (threadCount <= 1) {
            return;
        }
        if (seconds == 0) {
            fprintf(stderr, "[UT-EXIT] %d background threads still running after all tests, waiting for their exit\n",
                    threadCount - 1);
            fflush(stderr);
        }
        sleep(1);
    }
}
} // namespace

namespace ubse::vm::ut {
void InstallVirtAgentExitGuard()
{
    static std::once_flag installed;
    std::call_once(installed, [] { std::atexit(VirtAgentExitGuard); });
}
} // namespace ubse::vm::ut
