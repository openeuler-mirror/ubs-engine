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
#include <gtest/gtest.h>
#include <securec.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <ucontext.h>
#include <unistd.h>
#include <climits>
#include <cstdio>

constexpr uint32_t TRACE_BUFFER_SIZE = (64 * 1024); // 64KB 缓冲区
constexpr int MAX_STACK_DEPTH = 50;

static char traceBuffer[TRACE_BUFFER_SIZE];
static size_t traceBufferOffset = 0;

void InitTraceBuffer()
{
    traceBufferOffset = 0;
    traceBuffer[0] = '\0';
}

template <typename... Args>
void AppendToTraceBuffer(const char *format, Args... args)
{
    if (traceBufferOffset >= TRACE_BUFFER_SIZE - 1) {
        return;
    }

    // 如果是简单字符串，直接复制（编译时优化）
    if constexpr (sizeof...(Args) == 0) {
        size_t len = strlen(format);
        if (traceBufferOffset + len >= TRACE_BUFFER_SIZE) {
            len = TRACE_BUFFER_SIZE - traceBufferOffset - 1;
        }
        memcpy_s(traceBuffer + traceBufferOffset, TRACE_BUFFER_SIZE - traceBufferOffset, format, len);
        traceBufferOffset += len;
    } else {
        // 需要格式化的情况
        int written = snprintf_s(traceBuffer + traceBufferOffset, TRACE_BUFFER_SIZE - traceBufferOffset,
                                 TRACE_BUFFER_SIZE - traceBufferOffset, format, args...);
        if (written > 0) {
            traceBufferOffset += static_cast<size_t>(written);
        }
    }
}

// 输出整个缓冲区内容
void FlushTraceBuffer()
{
    if (traceBufferOffset > 0) {
        write(STDERR_FILENO, traceBuffer, traceBufferOffset);
        InitTraceBuffer(); // 重置缓冲区
    }
}

// 使用 addr2line 解析地址的函数
void ResolveAndPrintAddress(unsigned long long int address, int index, char *path)
{
    char cmd[PATH_MAX];
    char result[PATH_MAX];
    FILE *fp;
    Dl_info dl_info;

    memset_s(&dl_info, sizeof(Dl_info), 0, sizeof(Dl_info));
    AppendToTraceBuffer("%2d# [%p] ", index, reinterpret_cast<void *>(address));

    // 使用 addr2line 获取详细的行号信息
    snprintf_s(cmd, sizeof(cmd), sizeof(cmd), "addr2line -e %s -f -C -p %p 2>/dev/null", path,
               reinterpret_cast<void *>(address));

    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(result, sizeof(result), fp)) {
            result[strcspn(result, "\n")] = 0;
            AppendToTraceBuffer("%s", result);
        }
        pclose(fp);
    }
    AppendToTraceBuffer("\n");
}

#if defined(__aarch64__)
// ARM架构的栈回溯实现（完全保留原始代码）
static void PrintBackTraceARMImpl(const ucontext_t *uc, char *exePath)
{
    // AArch64 使用 X29 作为帧指针，X30 作为链接寄存器
    auto *fp = reinterpret_cast<unsigned long long int *>(uc->uc_mcontext.regs[29]); // X29/FP
    unsigned long long int lr = uc->uc_mcontext.regs[30]; // X30/LR (作为起始返回地址)
    unsigned long long int pc = uc->uc_mcontext.pc;       // 当前程序计数器

    int i = 0;

    // 首先打印触发错误的PC和LR
    ResolveAndPrintAddress(pc, i++, exePath);
    ResolveAndPrintAddress(lr, i++, exePath);

    // 然后遍历调用栈
    while (fp && lr) {
        // 获取上一级的帧指针和返回地址
        auto *nextFp = reinterpret_cast<unsigned long long int *>(*fp);
        if (nextFp == fp || nextFp == nullptr) {
            break; // 避免循环或空指针
        }

        // 获取返回地址
        lr = fp[1];

        // 解析并打印地址
        ResolveAndPrintAddress(lr, i++, exePath);

        // 检查是否到达 main 函数
        Dl_info dl_info;
        memset_s(&dl_info, sizeof(Dl_info), 0, sizeof(Dl_info));
        if (dladdr(reinterpret_cast<void *>(lr), &dl_info)) {
            if (dl_info.dli_sname && !strcmp(dl_info.dli_sname, "main")) {
                break;
            }
        }

        // 检查调用栈是否过深
        if (i > MAX_STACK_DEPTH) {
            break;
        }

        // 移动到上一级栈帧
        fp = nextFp;
    }
}
#endif

#if defined(__x86_64__)
// x86_64架构的栈回溯实现（参考原始ARM逻辑实现）
static void PrintBackTraceX86_64Impl(const ucontext_t *uc, char *exePath)
{
    // x86_64 使用 RBP 作为帧指针，RIP 作为程序计数器
    auto *fp = reinterpret_cast<unsigned long long int *>(uc->uc_mcontext.gregs[REG_RBP]); // RBP
    unsigned long long int pc = uc->uc_mcontext.gregs[REG_RIP];                             // RIP
    unsigned long long int lr = 0;                                                          // 返回地址（从栈中获取）

    int i = 0;

    // 首先打印触发错误的PC（x86_64没有单独的LR寄存器，返回地址在栈中）
    ResolveAndPrintAddress(pc, i++, exePath);
    
    // 如果有帧指针，获取第一个返回地址
    if (fp) {
        lr = fp[1];
        if (lr != 0) {
            ResolveAndPrintAddress(lr, i++, exePath);
        }
    }

    // 然后遍历调用栈
    while (fp && lr) {
        // 获取上一级的帧指针和返回地址
        auto *nextFp = reinterpret_cast<unsigned long long int *>(*fp);
        if (nextFp == fp || nextFp == nullptr) {
            break; // 避免循环或空指针
        }

        // 获取返回地址
        lr = fp[1];

        // 解析并打印地址
        ResolveAndPrintAddress(lr, i++, exePath);

        // 检查是否到达 main 函数
        Dl_info dl_info;
        memset_s(&dl_info, sizeof(Dl_info), 0, sizeof(Dl_info));
        if (dladdr(reinterpret_cast<void *>(lr), &dl_info)) {
            if (dl_info.dli_sname && !strcmp(dl_info.dli_sname, "main")) {
                break;
            }
        }

        // 检查调用栈是否过深
        if (i > MAX_STACK_DEPTH) {
            break;
        }

        // 移动到上一级栈帧
        fp = nextFp;
    }
}
#endif

void PrintBackTrace(const ucontext_t *uc)
{
    InitTraceBuffer();
    AppendToTraceBuffer("========== Stack trace start ==========\n");
    
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len >= 0) {
        exePath[len] = '\0';
    }
    
    #if defined(__aarch64__)
        PrintBackTraceARMImpl(uc, exePath);
    #elif defined(__x86_64__)
        PrintBackTraceX86_64Impl(uc, exePath);
    #else
        AppendToTraceBuffer("Unsupported architecture\n");
    #endif
    
    AppendToTraceBuffer("========== Stack trace end ==========\n");
    FlushTraceBuffer();
}

void SignalHandlerWithContext(int sig, siginfo_t *info, void *context)
{
    if (context) {
        const auto *uc = reinterpret_cast<ucontext_t *>(context);
        PrintBackTrace(uc);
    }

    _exit(128 + sig); // 退出信号设置为128加上原有信号
}

void SetupSignalHandlers()
{
    struct sigaction sa {};
    sa.sa_sigaction = SignalHandlerWithContext;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
}

int main(int argc, char **argv)
{
    SetupSignalHandlers();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}