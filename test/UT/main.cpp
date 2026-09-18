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
#include <pthread.h>
#include <securec.h>
#include <setjmp.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

constexpr uint32_t TRACE_BUFFER_SIZE = (64 * 1024); // 64KB 缓冲区
constexpr int MAX_STACK_DEPTH = 50;
// 帧指针距栈指针的最大合法距离（线程栈通常 8MB），用于过滤被优化掉帧指针的场景（如打断在 glibc 内部）
constexpr uintptr_t STACK_WALK_MAX_RANGE = (8 * 1024 * 1024);

static char traceBuffer[TRACE_BUFFER_SIZE];
static size_t traceBufferOffset = 0;

void InitTraceBuffer()
{
    traceBufferOffset = 0;
    traceBuffer[0] = '\0';
}

template <typename... Args>
void AppendToTraceBuffer(const char* format, Args... args)
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
void ResolveAndPrintAddress(unsigned long long int address, int index, char* path)
{
    char cmd[PATH_MAX];
    char result[PATH_MAX];
    FILE* fp;
    Dl_info dl_info;

    memset_s(&dl_info, sizeof(Dl_info), 0, sizeof(Dl_info));
    AppendToTraceBuffer("%2d# [%p] ", index, reinterpret_cast<void*>(address));
    // 先落盘原始地址：若后续 addr2line（popen/fork/malloc）在信号处理器内阻塞，帧信息也不丢失
    FlushTraceBuffer();

    // 使用 addr2line 获取详细的行号信息
    snprintf_s(cmd, sizeof(cmd), sizeof(cmd), "addr2line -e %s -f -C -p %p 2>/dev/null", path,
               reinterpret_cast<void*>(address));

    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(result, sizeof(result), fp)) {
            result[strcspn(result, "\n")] = 0;
            AppendToTraceBuffer("%s", result);
        }
        pclose(fp);
    }
    AppendToTraceBuffer("\n");
    FlushTraceBuffer();
}

// 帧指针合法性检查：必须位于栈指针之上且不超过线程栈范围，防止解引用非法帧指针导致信号处理器内崩溃
static inline bool IsValidFrame(const void* fp, uintptr_t sp)
{
    auto fpVal = reinterpret_cast<uintptr_t>(fp);
    return fp != nullptr && fpVal >= sp && fpVal - sp < STACK_WALK_MAX_RANGE;
}

#if defined(__aarch64__)
// ARM架构的栈回溯实现（完全保留原始代码）
static void PrintBackTraceARMImpl(const ucontext_t* uc, char* exePath)
{
    // AArch64 使用 X29 作为帧指针，X30 作为链接寄存器
    auto* fp = reinterpret_cast<unsigned long long int*>(uc->uc_mcontext.regs[29]); // X29/FP
    unsigned long long int lr = uc->uc_mcontext.regs[30];                           // X30/LR (作为起始返回地址)
    unsigned long long int pc = uc->uc_mcontext.pc;                                 // 当前程序计数器
    uintptr_t sp = static_cast<uintptr_t>(uc->uc_mcontext.sp);

    int i = 0;

    // 首先打印触发错误的PC和LR
    ResolveAndPrintAddress(pc, i++, exePath);
    ResolveAndPrintAddress(lr, i++, exePath);

    // 然后遍历调用栈
    while (fp && lr) {
        if (!IsValidFrame(fp, sp)) {
            break; // 帧指针非法（可能被编译器优化掉）
        }

        // 获取上一级的帧指针和返回地址
        auto* nextFp = reinterpret_cast<unsigned long long int*>(*fp);
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
        if (dladdr(reinterpret_cast<void*>(lr), &dl_info)) {
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
static void PrintBackTraceX86_64Impl(const ucontext_t* uc, char* exePath)
{
    // x86_64 使用 RBP 作为帧指针，RIP 作为程序计数器
    auto* fp = reinterpret_cast<unsigned long long int*>(uc->uc_mcontext.gregs[REG_RBP]); // RBP
    unsigned long long int pc = uc->uc_mcontext.gregs[REG_RIP];                           // RIP
    unsigned long long int lr = 0; // 返回地址（从栈中获取）
    uintptr_t sp = static_cast<uintptr_t>(uc->uc_mcontext.gregs[REG_RSP]);

    int i = 0;

    // 首先打印触发错误的PC（x86_64没有单独的LR寄存器，返回地址在栈中）
    ResolveAndPrintAddress(pc, i++, exePath);

    // 如果有帧指针，获取第一个返回地址
    if (IsValidFrame(fp, sp)) {
        lr = fp[1];
        if (lr != 0) {
            ResolveAndPrintAddress(lr, i++, exePath);
        }
    }

    // 然后遍历调用栈
    while (fp && lr) {
        if (!IsValidFrame(fp, sp)) {
            break; // 帧指针非法（可能被编译器优化掉）
        }

        // 获取上一级的帧指针和返回地址
        auto* nextFp = reinterpret_cast<unsigned long long int*>(*fp);
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
        if (dladdr(reinterpret_cast<void*>(lr), &dl_info)) {
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

void PrintBackTrace(const ucontext_t* uc)
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

// 栈回溯保护的跳转点：若回溯期间再次触发致命信号（如探到非法帧指针），跳回此处优雅降级
static sigjmp_buf g_backtraceJmpBuf;
static volatile sig_atomic_t g_inBacktrace = 0;

void SignalHandlerWithContext(int sig, siginfo_t* info, void* context)
{
    // 栈回溯过程中发生嵌套致命信号：跳回上一层信号处理器的恢复点，保证按原信号退出
    if (g_inBacktrace != 0) {
        g_inBacktrace = 0;
        siglongjmp(g_backtraceJmpBuf, 1);
    }

    if (context) {
        if (sigsetjmp(g_backtraceJmpBuf, 1) == 0) {
            g_inBacktrace = 1;
            const auto* uc = reinterpret_cast<ucontext_t*>(context);
            PrintBackTrace(uc);
            g_inBacktrace = 0;
        } else {
            AppendToTraceBuffer("[backtrace aborted: invalid frame pointer]\n");
        }
        FlushTraceBuffer();
    }

    _exit(128 + sig); // 退出信号设置为128加上原有信号
}

void SetupSignalHandlers()
{
    struct sigaction sa {
    };
    sa.sa_sigaction = SignalHandlerWithContext;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
}

namespace {
// 单用例最长运行时间（秒） 0 = 关闭拦截
std::atomic<int> g_testTimeoutSec{30};

// 单用例超时看门狗：超时后向测试线程定向投递 SIGABRT，
// 复用上面的 SignalHandlerWithContext 在挂死线程上打印栈回溯后退出
class TimeoutWatchdog {
public:
    static TimeoutWatchdog& Instance()
    {
        // 单例故意只 new 不 delete（进程退出时由操作系统回收）。
        // 原因：gtest 的 ASSERT_EXIT/ASSERT_DEATH 用例会 fork 子进程并在子进程里调用 exit()；
        // 如果单例是静态对象，exit() 会执行它的析构函数，析构里 Stop() 会 join 看门狗线程，
        // 但 fork 出的子进程只有调用线程，看门狗线程并不存在，join 将永久阻塞，子进程僵死。
        // 不参与静态析构（没有析构时机）就不会触发 join，子进程可以直接退出。
        static TimeoutWatchdog* w = new TimeoutWatchdog();
        return *w;
    }

    void Start()
    {
        thread_ = std::thread([this] { WatchLoop(); });
    }

    void Stop()
    {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            stopping_ = true;
            running_ = false;
        }
        cv_.notify_all();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    // gtest 监听器回调与被测用例运行在同一线程，此时 pthread_self 即测试线程
    void OnTestStart(std::string name)
    {
        std::lock_guard<std::mutex> lk(mtx_);
        currentTest_ = std::move(name);
        testThread_ = pthread_self();
        testTid_ = static_cast<pid_t>(syscall(SYS_gettid));
        start_ = std::chrono::steady_clock::now();
        running_ = true;
        cv_.notify_all();
    }

    void OnTestEnd()
    {
        std::lock_guard<std::mutex> lk(mtx_);
        running_ = false;
        cv_.notify_all();
    }

private:
    void WatchLoop()
    {
        std::unique_lock<std::mutex> lk(mtx_);
        for (;;) {
            cv_.wait(lk, [this] { return running_ || stopping_; });
            if (stopping_) {
                return;
            }
            auto deadline = start_ + std::chrono::seconds(g_testTimeoutSec.load());
            while (running_ && std::chrono::steady_clock::now() < deadline) {
                cv_.wait_until(lk, deadline); // OnTestEnd 会提前唤醒
            }
            if (!running_) {
                continue; // 用例正常结束
            }
            fprintf(stderr, "\n[UT-TIMEOUT] test '%s' exceeded the %ds limit, aborting\n", currentTest_.c_str(),
                    g_testTimeoutSec.load());
            fflush(stderr);
            // 投递前重装信号处理器：被测代码/插件可能覆盖了 SIGABRT 处理器导致信号被吞
            SetupSignalHandlers();
            pthread_kill(testThread_, SIGABRT);
            // 兜底：若 SIGABRT 被屏蔽（无法跨线程解除）或被吞，强制退出并留下挂死线程线索
            std::this_thread::sleep_for(std::chrono::seconds(2));
            fprintf(stderr, "[UT-TIMEOUT] SIGABRT swallowed or blocked (hung thread tid=%d), force exit\n", testTid_);
            fflush(stderr);
            _exit(134);
        }
    }

    std::thread thread_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::string currentTest_;
    pthread_t testThread_{};
    pid_t testTid_ = -1;
    std::chrono::steady_clock::time_point start_;
    bool running_ = false;
    bool stopping_ = false;
};

class TimeoutListener : public testing::EmptyTestEventListener {
    void OnTestProgramStart(const testing::UnitTest& /*unitTest*/) override
    {
        TimeoutWatchdog::Instance().Start();
    }

    void OnTestStart(const testing::TestInfo& info) override
    {
        TimeoutWatchdog::Instance().OnTestStart(std::string(info.test_suite_name()) + "." + info.name());
    }

    void OnTestEnd(const testing::TestInfo& /*info*/) override
    {
        TimeoutWatchdog::Instance().OnTestEnd();
    }

    void OnTestProgramEnd(const testing::UnitTest& /*unitTest*/) override
    {
        TimeoutWatchdog::Instance().Stop();
    }
};
} // namespace

int main(int argc, char** argv)
{
#if !defined(__SANITIZE_ADDRESS__)
    SetupSignalHandlers();
#endif
    testing::InitGoogleTest(&argc, argv);
    if (g_testTimeoutSec > 0) {
        testing::UnitTest::GetInstance()->listeners().Append(new TimeoutListener);
    }
    return RUN_ALL_TESTS();
}