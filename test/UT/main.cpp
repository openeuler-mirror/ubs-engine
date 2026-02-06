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
#include <ucontext.h>
#include <unistd.h>
#include <climits>
#include <cstdio>

constexpr uint32_t TRACE_BUFFER_SIZE = (64 * 1024);
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

    if constexpr (sizeof...(Args) == 0) {
        size_t len = strlen(format);
        if (traceBufferOffset + len >= TRACE_BUFFER_SIZE) {
            len = TRACE_BUFFER_SIZE - traceBufferOffset - 1;
        }
        memcpy_s(traceBuffer + traceBufferOffset, TRACE_BUFFER_SIZE - traceBufferOffset, format, len);
        traceBufferOffset += len;
    } else {
        int written = snprintf_s(traceBuffer + traceBufferOffset, TRACE_BUFFER_SIZE - traceBufferOffset,
                                 TRACE_BUFFER_SIZE - traceBufferOffset, format, args...);
        if (written > 0) {
            traceBufferOffset += static_cast<size_t>(written);
        }
    }
}

void FlushTraceBuffer()
{
    if (traceBufferOffset > 0) {
        write(STDERR_FILENO, traceBuffer, traceBufferOffset);
        InitTraceBuffer();
    }
}

void ResolveAndPrintAddress(unsigned long long int address, int index, char *path)
{
    char cmd[PATH_MAX];
    char result[PATH_MAX];
    FILE *fp;
    Dl_info dl_info;

    memset_s(&dl_info, sizeof(Dl_info), 0, sizeof(Dl_info));
    AppendToTraceBuffer("%2d# [%p] ", index, reinterpret_cast<void *>(address));

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

void PrintBacktrace(const ucontext_t *uc)
{
    InitTraceBuffer();

    auto *fp = reinterpret_cast<unsigned long long int *>(uc->uc_mcontext.regs[29]);
    unsigned long long int lr = uc->uc_mcontext.regs[30];
    unsigned long long int pc = uc->uc_mcontext.pc;

    AppendToTraceBuffer("========== Stack trace start ==========\n");
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len >= 0) {
        exePath[len] = '\0';
    }

    int i = 0;
    ResolveAndPrintAddress(pc, i++, exePath);
    ResolveAndPrintAddress(lr, i++, exePath);

    while (fp && lr) {
        auto *nextFp = reinterpret_cast<unsigned long long int *>(*fp);
        if (nextFp == fp || nextFp == nullptr) {
            break;
        }

        lr = fp[1];
        ResolveAndPrintAddress(lr, i++, exePath);

        Dl_info dl_info;
        memset_s(&dl_info, sizeof(Dl_info), 0, sizeof(Dl_info));
        if (dladdr(reinterpret_cast<void *>(lr), &dl_info)) {
            if (dl_info.dli_sname && !strcmp(dl_info.dli_sname, "main")) {
                break;
            }
        }

        if (i > MAX_STACK_DEPTH) {
            break;
        }

        fp = nextFp;
    }

    AppendToTraceBuffer("========== Stack trace end ==========\n");
    FlushTraceBuffer();
}

void SignalHandlerWithContext(int sig, siginfo_t *info, void *context)
{
    if (context) {
        const auto *uc = reinterpret_cast<ucontext_t *>(context);
        PrintBacktrace(uc);
    }

    _exit(128 + sig);
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