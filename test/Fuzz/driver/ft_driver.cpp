/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ft_driver.h"

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>
#include <string>
#include <vector>

// harness 提供的入口（libFuzzer 约定）
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

// harness 可选提供的种子生成钩子（弱符号：未实现时为 nullptr，驱动自动跳过）
extern "C" int FtGenSeeds(const char* corpusDir) __attribute__((weak));

// 覆盖率构建（FT_COVERAGE=ON）时由 CMake 注入 FT_ENABLE_GCOV：
// 强引用 libgcov 的 __gcov_dump，驱动 fork 子进程在 _exit 前手动落盘 gcda
// （_exit 不走 atexit）。注意不能用弱符号——弱引用不会促使链接器从静态库
// libgcov.a 提取成员，导致 __gcov_dump 静默解析为 nullptr。
// 子进程不能用 exit()：避免父进程注册的 atexit/静态析构在副本中重放。
#ifdef FT_ENABLE_GCOV
extern "C" void __gcov_dump(void);
#define FT_GCOV_DUMP() __gcov_dump()
#else
#define FT_GCOV_DUMP() ((void)0)
#endif

namespace {

constexpr uint64_t DEFAULT_RUNS = 30000000;      // 3000万次
constexpr uint64_t DEFAULT_TIME_SECONDS = 10800; // 3小时
constexpr size_t DEFAULT_MAX_LEN = 4096;
constexpr uint64_t DEFAULT_HANG_TIMEOUT_MS = 10000; // 单次执行无进度超过此时长判定挂起
constexpr uint64_t BATCH_EXECS = 10000;             // 每个子进程执行的批次数（fork 开销摊薄）
constexpr uint64_t MAX_SAVED_INPUTS = 1000; // 崩溃/挂起输入落盘上限（防磁盘写爆，超出只计数）
constexpr int EXIT_FOUND_BUG = 77;

struct Options {
    uint64_t maxRuns = DEFAULT_RUNS;
    uint64_t maxSeconds = DEFAULT_TIME_SECONDS;
    size_t maxLen = DEFAULT_MAX_LEN;
    uint64_t hangTimeoutMs = DEFAULT_HANG_TIMEOUT_MS;
    bool quiet = false;
    std::string corpusDir;
    std::string crashDir;
    std::string replayFile;
};

// fork 前分配的匿名共享内存（父子映射同一份）：
// 子进程推进执行计数与进度时间戳，父进程据此对齐进度并判定挂起。
// 成员均为对齐 8 字节的单写者读-改-写，ARM64/x86_64 上天然原子。
struct FtShared {
    volatile uint64_t execs;          // 总执行数（子进程写）
    volatile uint64_t batchExecs;     // 当前批次执行数（子进程写）
    volatile uint64_t lastProgressMs; // 最近一次执行开始时间（子进程写，父进程读）
};

struct CrashGuard {
    // MAP_SHARED 崩溃现场文件：每次执行前把当前输入写入，
    // 进程被 ASan/信号杀死后文件内容仍在，用于事后复现（无 msync 开销）。
    int fd = -1;
    uint8_t* mapped = nullptr;
    size_t mappedSize = 0;
    std::string path;
};

Options g_opt;
CrashGuard g_guard;
uint64_t g_execs = 0;
uint64_t g_crashes = 0;
uint64_t g_hangs = 0;
uint64_t g_savedInputs = 0;     // 已落盘的 crash/hang 输入数（上限保护）
volatile pid_t g_childPid = -1; // 父进程当前子进程（Ctrl-C 兜底清理）

// 常用"有趣"字节：长度字段、符号边界附近值的高发区
const uint8_t kInterestingBytes[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x07, 0x08, 0x0f, 0x10,
                                     0x1f, 0x20, 0x3f, 0x40, 0x7f, 0x80, 0x81, 0xfe, 0xff};

std::vector<std::string> ListFiles(const std::string& dir)
{
    std::vector<std::string> files;
    DIR* d = opendir(dir.c_str());
    if (d == nullptr) {
        return files;
    }
    struct dirent* ent = nullptr;
    while ((ent = readdir(d)) != nullptr) {
        if (ent->d_type == DT_REG) {
            files.emplace_back(ent->d_name);
        }
    }
    closedir(d);
    std::sort(files.begin(), files.end());
    return files;
}

bool ReadFileBytes(const std::string& path, std::vector<uint8_t>& out)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    out.clear();
    uint8_t buf[8192];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        out.insert(out.end(), buf, buf + n);
    }
    fclose(f);
    return true;
}

bool WriteFileBytes(const std::string& path, const uint8_t* data, size_t size)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    return written == size;
}

// 将守护区中的当前输入落盘（fork 模式下由父进程调用；replay 模式下在信号上下文调用，
// 故只用 async-signal-safe 接口 open/write/close）
void SaveGuardInput(const char* prefix)
{
    if (g_guard.mapped == nullptr || g_guard.mappedSize < sizeof(uint64_t)) {
        return;
    }
    uint64_t len = 0;
    memcpy(&len, g_guard.mapped, sizeof(len));
    if (len > g_guard.mappedSize - sizeof(uint64_t)) {
        len = g_guard.mappedSize - sizeof(uint64_t);
    }
    if (g_savedInputs >= MAX_SAVED_INPUTS) {
        return; // 超出落盘上限：只计数不写文件，防止磁盘写爆
    }
    char crashPath[512];
    const int n = snprintf(crashPath, sizeof(crashPath), "%s/%s-%llu", g_opt.crashDir.c_str(), prefix,
                           static_cast<unsigned long long>(g_execs));
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(crashPath)) {
        return;
    }
    const int fd = open(crashPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return;
    }
    if (len > 0) {
        (void)write(fd, g_guard.mapped + sizeof(uint64_t), len);
    }
    (void)close(fd);
    ++g_savedInputs;
}

void SignalHandler(int sig)
{
    SaveGuardInput("crash");
    // 恢复默认处理并重发，保证 ASan/core dump 语义不变
    signal(sig, SIG_DFL);
    raise(sig);
}

// Ctrl-C/kill 兜底：清理当前子进程后退出（避免孤儿进程继续跑）
void ParentSignalHandler(int sig)
{
    if (g_childPid > 0) {
        kill(g_childPid, SIGKILL);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

uint64_t NowMs()
{
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000 + static_cast<uint64_t>(ts.tv_nsec) / 1000000;
}

void UpdateCurrentInput(const uint8_t* data, size_t size)
{
    if (g_guard.mapped == nullptr) {
        return;
    }
    size_t capped = std::min(size, g_guard.mappedSize - sizeof(uint64_t));
    const uint64_t len = capped;
    memcpy(g_guard.mapped, &len, sizeof(len));
    if (capped > 0) {
        memcpy(g_guard.mapped + sizeof(uint64_t), data, capped);
    }
}

// ---------------- 变异引擎 ----------------

class Mutator {
public:
    explicit Mutator(uint64_t seed) : rng_(seed) {}

    // 基于 seeds 变异产生新输入
    std::vector<uint8_t> Mutate(const std::vector<std::vector<uint8_t>>& seeds)
    {
        std::vector<uint8_t> buf;
        // 15% 完全随机
        if (Percent(15)) {
            const size_t len = RandomLen();
            buf.resize(len);
            for (size_t i = 0; i < len; ++i) {
                buf[i] = static_cast<uint8_t>(rng_() & 0xff);
            }
            return buf;
        }
        if (seeds.empty()) {
            const size_t len = RandomLen();
            buf.resize(len);
            for (size_t i = 0; i < len; ++i) {
                buf[i] = static_cast<uint8_t>(rng_() & 0xff);
            }
            return buf;
        }
        buf = seeds[rng_() % seeds.size()];
        // 连续变异 1~8 次
        const int rounds = 1 + static_cast<int>(rng_() % 8);
        for (int r = 0; r < rounds; ++r) {
            ApplyOneMutation(buf, seeds);
        }
        if (buf.size() > g_opt.maxLen) {
            buf.resize(g_opt.maxLen);
        }
        return buf;
    }

private:
    std::mt19937_64 rng_;

    bool Percent(int p)
    {
        return (rng_() % 100) < static_cast<uint64_t>(p);
    }

    size_t RandomLen()
    {
        // 2/3 概率小输入（0~256，快速穿透短字段），1/3 概率大输入（最深 4096）
        if (Percent(67)) {
            return rng_() % 256;
        }
        return rng_() % (g_opt.maxLen + 1);
    }

    uint8_t InterestingByte()
    {
        return kInterestingBytes[rng_() % (sizeof(kInterestingBytes) / sizeof(uint8_t))];
    }

    void ApplyOneMutation(std::vector<uint8_t>& buf, const std::vector<std::vector<uint8_t>>& seeds)
    {
        const int op = static_cast<int>(rng_() % 100);
        if (buf.empty() || op < 5) { // 插入
            const size_t pos = rng_() % (buf.size() + 1);
            const size_t cnt = 1 + rng_() % 16;
            std::vector<uint8_t> ins(cnt);
            for (size_t i = 0; i < cnt; ++i) {
                ins[i] = Percent(50) ? InterestingByte() : static_cast<uint8_t>(rng_() & 0xff);
            }
            buf.insert(buf.begin() + static_cast<long>(pos), ins.begin(), ins.end());
            return;
        }
        if (op < 10) { // 删除
            const size_t pos = rng_() % buf.size();
            const size_t cnt = 1 + rng_() % 16;
            const size_t real = std::min(cnt, buf.size() - pos);
            buf.erase(buf.begin() + static_cast<long>(pos), buf.begin() + static_cast<long>(pos + real));
            return;
        }
        if (op < 20) { // 位翻转
            const size_t pos = rng_() % buf.size();
            buf[pos] ^= static_cast<uint8_t>(1u << (rng_() % 8));
            return;
        }
        if (op < 35) { // 有趣字节替换
            const size_t pos = rng_() % buf.size();
            buf[pos] = InterestingByte();
            return;
        }
        if (op < 45) { // 算术加减
            const size_t pos = rng_() % buf.size();
            const int delta = static_cast<int>(rng_() % 35) + 1;
            buf[pos] = static_cast<uint8_t>(buf[pos] + (Percent(50) ? delta : -delta));
            return;
        }
        if (op < 65) { // 多字节整数写入（大/小端）：针对长度/计数类字段
            const size_t pos = rng_() % buf.size();
            const int width = (rng_() % 4 == 0) ? 8 : (rng_() % 3 == 0) ? 4 : 2;
            const uint64_t val = rng_();
            const bool bigEndian = Percent(50);
            for (int i = 0; i < width; ++i) {
                const size_t idx = pos + static_cast<size_t>(i);
                if (idx >= buf.size()) {
                    break;
                }
                const int shift = bigEndian ? (width - 1 - i) : i;
                buf[idx] = static_cast<uint8_t>((val >> (8 * shift)) & 0xff);
            }
            return;
        }
        if (op < 75) { // 块重复（放大结构）
            const size_t pos = rng_() % buf.size();
            const size_t cnt = std::min<size_t>(1 + rng_() % 32, buf.size() - pos);
            const std::vector<uint8_t> block(buf.begin() + static_cast<long>(pos),
                                             buf.begin() + static_cast<long>(pos + cnt));
            const size_t at = rng_() % (buf.size() + 1);
            buf.insert(buf.begin() + static_cast<long>(at), block.begin(), block.end());
            return;
        }
        if (op < 85) { // 跨种子块拷贝（crossover）
            if (seeds.size() > 1) {
                const std::vector<uint8_t>& other = seeds[rng_() % seeds.size()];
                if (!other.empty()) {
                    const size_t srcPos = rng_() % other.size();
                    const size_t cnt = std::min<size_t>(1 + rng_() % 64, other.size() - srcPos);
                    const size_t dstPos = rng_() % (buf.size() + 1);
                    buf.insert(buf.begin() + static_cast<long>(dstPos), other.begin() + static_cast<long>(srcPos),
                               other.begin() + static_cast<long>(srcPos + cnt));
                    return;
                }
            }
            const size_t pos = rng_() % buf.size();
            buf[pos] = static_cast<uint8_t>(rng_() & 0xff);
            return;
        }
        if (op < 95) { // 截断
            if (buf.size() > 1) {
                buf.resize(1 + rng_() % buf.size());
            }
            return;
        }
        // 追加
        const size_t cnt = 1 + rng_() % 32;
        for (size_t i = 0; i < cnt; ++i) {
            buf.push_back(Percent(50) ? InterestingByte() : static_cast<uint8_t>(rng_() & 0xff));
        }
    }
};

bool ParseULong(const char* s, uint64_t& out)
{
    char* end = nullptr;
    errno = 0;
    const unsigned long long v = strtoull(s, &end, 10);
    if (end == s || *end != '\0' || errno != 0) {
        return false;
    }
    out = v;
    return true;
}

void PrintUsage(const char* prog)
{
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  --runs=N          max executions (default 30000000)\n"
            "  --time=S          max seconds (default 10800)\n"
            "  --corpus=DIR      corpus directory\n"
            "  --crash-dir=DIR   crash input directory\n"
            "  --max-len=N       max input length (default 4096)\n"
            "  --hang-timeout=MS single-exec no-progress timeout in ms (default 10000)\n"
            "  --replay=FILE     execute one file only (crash reproduction)\n"
            "  --quiet           no progress output\n",
            prog);
}

std::string Basename(const char* path)
{
    const char* slash = strrchr(path, '/');
    return slash != nullptr ? std::string(slash + 1) : std::string(path);
}

bool InitCrashGuard()
{
    g_guard.path = g_opt.crashDir + "/current_input";
    g_guard.mappedSize = sizeof(uint64_t) + g_opt.maxLen + 64;
    g_guard.fd = open(g_guard.path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (g_guard.fd < 0) {
        return false;
    }
    if (ftruncate(g_guard.fd, static_cast<off_t>(g_guard.mappedSize)) != 0) {
        close(g_guard.fd);
        g_guard.fd = -1;
        return false;
    }
    g_guard.mapped =
        static_cast<uint8_t*>(mmap(nullptr, g_guard.mappedSize, PROT_READ | PROT_WRITE, MAP_SHARED, g_guard.fd, 0));
    if (g_guard.mapped == MAP_FAILED) {
        g_guard.mapped = nullptr;
        close(g_guard.fd);
        g_guard.fd = -1;
        return false;
    }
    return true;
}

// 递归创建目录（等价 mkdir -p），保证 --corpus=/deep/path 存在
void MkdirP(const std::string& dir)
{
    std::string cur;
    for (size_t i = 0; i < dir.size(); ++i) {
        if (dir[i] == '/' && !cur.empty()) {
            mkdir(cur.c_str(), 0755);
        }
        cur += dir[i];
    }
    if (!cur.empty()) {
        mkdir(cur.c_str(), 0755);
    }
}

bool LoadCorpus(std::vector<std::vector<uint8_t>>& seeds)
{
    MkdirP(g_opt.corpusDir);
    auto files = ListFiles(g_opt.corpusDir);
    if (files.empty()) {
        if (FtGenSeeds != nullptr) {
            const int n = FtGenSeeds(g_opt.corpusDir.c_str());
            if (n > 0) {
                fprintf(stdout, "[ft] generated %d seed(s) via FtGenSeeds\n", n);
            }
        }
        files = ListFiles(g_opt.corpusDir);
    }
    for (const auto& f : files) {
        std::vector<uint8_t> data;
        if (ReadFileBytes(g_opt.corpusDir + "/" + f, data)) {
            if (data.size() > g_opt.maxLen) {
                data.resize(g_opt.maxLen);
            }
            if (!data.empty()) {
                seeds.push_back(std::move(data));
            }
        }
    }
    return !seeds.empty();
}

// ---------------- fork 批处理模式 ----------------
// 子进程：执行一批（batchTarget 次）后退出。崩溃即死（SIG_DFL，不装 handler），
// 输入现场已写入 MAP_SHARED 守护区，由父进程负责计数与落盘。
void RunBatchChild(const std::vector<std::vector<uint8_t>>& seeds, FtShared* shared, uint64_t batchTarget,
                   uint64_t rngSeed)
{
    Mutator mutator(rngSeed);
    for (uint64_t i = 0; i < batchTarget; ++i) {
        std::vector<uint8_t> input = mutator.Mutate(seeds);
        UpdateCurrentInput(input.data(), input.size());
        shared->execs = shared->execs + 1;
        shared->batchExecs = shared->batchExecs + 1;
        shared->lastProgressMs = NowMs();
        if (LLVMFuzzerTestOneInput(input.data(), input.size()) != 0) {
            FT_GCOV_DUMP(); // 覆盖率构建：harness 自报 bug 也保留 gcda
            _exit(201);     // harness 主动上报 bug：父进程按崩溃处理
        }
    }
    FT_GCOV_DUMP(); // 覆盖率构建：_exit 不走 atexit，手动落盘 gcda
    _exit(0);
}

// 父进程：fork 子进程并监控至其退出。
// 返回值：0=正常退出批次；1=崩溃（已计数）；2=挂起（已计数）。
// g_execs 在返回前已对齐 shared->execs。
int RunAndMonitorBatch(const std::vector<std::vector<uint8_t>>& seeds, FtShared* shared, uint64_t batchTarget,
                       uint64_t rngSeed, uint64_t deadlineMs)
{
    shared->batchExecs = 0;
    shared->lastProgressMs = NowMs(); // 子进程启动/懒初始化期间不判挂起

    // fork 前清空 stdio 缓冲：否则子进程会继承未落盘的缓冲内容，
    // 其退出时的 flush 会把父进程已打印的行重复写入日志（表现为 generated/corpus 行出现多份）
    fflush(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "[ft] fork failed: %s\n", strerror(errno));
        return -1; // 外层收到 -1 结束运行，避免无限重试
    }
    if (pid == 0) {
        RunBatchChild(seeds, shared, batchTarget, rngSeed);
        return 0; // 不可达（_exit）
    }
    g_childPid = pid;

    bool killedAsHang = false;
    bool killedForTime = false;
    int status = 0;
    while (true) {
        const pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            break; // 子进程已退出
        }
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        // 子进程仍在运行：挂起判定（进度时间戳超时未推进）
        if (NowMs() - shared->lastProgressMs > g_opt.hangTimeoutMs) {
            kill(pid, SIGKILL);
            killedAsHang = true;
            (void)waitpid(pid, &status, 0);
            break;
        }
        // 总时间限额到达：杀掉但不计入挂起
        if (NowMs() >= deadlineMs) {
            kill(pid, SIGKILL);
            killedForTime = true;
            (void)waitpid(pid, &status, 0);
            break;
        }
        usleep(20 * 1000); // 20ms 轮询
    }
    g_childPid = -1;

    g_execs = shared->execs; // 对齐执行计数（崩溃/挂起的那次已计入）

    if (killedAsHang) {
        ++g_hangs;
        SaveGuardInput("hang");
        if (!g_opt.quiet) {
            fprintf(stdout, "[ft] hang detected at exec #%llu, input saved\n",
                    static_cast<unsigned long long>(g_execs));
            fflush(stdout);
        }
        return 2;
    }
    if (killedForTime) {
        return 0; // 总时限到达，外层自然结束
    }
    if (WIFSIGNALED(status)) {
        ++g_crashes;
        SaveGuardInput("crash");
        if (!g_opt.quiet) {
            fprintf(stdout, "[ft] crash detected (signal %d) at exec #%llu, input saved\n", WTERMSIG(status),
                    static_cast<unsigned long long>(g_execs));
            fflush(stdout);
        }
        return 1;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        // harness 返回非 0（自报 bug）
        ++g_crashes;
        SaveGuardInput("crash");
        if (!g_opt.quiet) {
            fprintf(stdout, "[ft] harness reported a bug at exec #%llu, input saved\n",
                    static_cast<unsigned long long>(g_execs));
            fflush(stdout);
        }
        return 1;
    }
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    const std::string progName = Basename(argv[0]);
    g_opt.corpusDir = "ft_corpus_" + progName;
    g_opt.crashDir = "ft_crash_" + progName;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind("--runs=", 0) == 0) {
            if (!ParseULong(arg.c_str() + 7, g_opt.maxRuns)) {
                PrintUsage(argv[0]);
                return 2;
            }
        } else if (arg.rfind("--time=", 0) == 0) {
            if (!ParseULong(arg.c_str() + 7, g_opt.maxSeconds)) {
                PrintUsage(argv[0]);
                return 2;
            }
        } else if (arg.rfind("--max-len=", 0) == 0) {
            uint64_t v = 0;
            if (!ParseULong(arg.c_str() + 10, v) || v == 0 || v > 10 * 1024 * 1024) {
                PrintUsage(argv[0]);
                return 2;
            }
            g_opt.maxLen = static_cast<size_t>(v);
        } else if (arg.rfind("--hang-timeout=", 0) == 0) {
            uint64_t v = 0;
            if (!ParseULong(arg.c_str() + 15, v) || v == 0) {
                PrintUsage(argv[0]);
                return 2;
            }
            g_opt.hangTimeoutMs = v;
        } else if (arg.rfind("--corpus=", 0) == 0) {
            g_opt.corpusDir = arg.substr(9);
        } else if (arg.rfind("--crash-dir=", 0) == 0) {
            g_opt.crashDir = arg.substr(12);
        } else if (arg.rfind("--replay=", 0) == 0) {
            g_opt.replayFile = arg.substr(9);
        } else if (arg == "--quiet") {
            g_opt.quiet = true;
        } else {
            PrintUsage(argv[0]);
            return 2;
        }
    }

    // 复现模式：只跑一个文件（进程内直接执行，崩溃走信号兜底落盘）
    if (!g_opt.replayFile.empty()) {
        signal(SIGSEGV, SignalHandler);
        signal(SIGBUS, SignalHandler);
        signal(SIGILL, SignalHandler);
        signal(SIGFPE, SignalHandler);
        signal(SIGABRT, SignalHandler);
        MkdirP(g_opt.crashDir);
        if (!InitCrashGuard()) {
            fprintf(stderr, "[ft] cannot init crash guard at %s\n", g_opt.crashDir.c_str());
            return 2;
        }
        std::vector<uint8_t> data;
        if (!ReadFileBytes(g_opt.replayFile, data)) {
            fprintf(stderr, "[ft] cannot read replay file: %s\n", g_opt.replayFile.c_str());
            return 2;
        }
        fprintf(stdout, "[ft] replay %s (%zu bytes)\n", g_opt.replayFile.c_str(), data.size());
        UpdateCurrentInput(data.data(), data.size());
        ++g_execs;
        const int ret = LLVMFuzzerTestOneInput(data.data(), data.size());
        fprintf(stdout, "[ft] replay done, harness returns %d\n", ret);
        return ret == 0 ? 0 : EXIT_FOUND_BUG;
    }

    MkdirP(g_opt.crashDir);
    if (!InitCrashGuard()) {
        fprintf(stderr, "[ft] cannot init crash guard at %s\n", g_opt.crashDir.c_str());
        return 2;
    }

    std::vector<std::vector<uint8_t>> seeds;
    if (!LoadCorpus(seeds)) {
        fprintf(stderr, "[ft] no usable seeds in %s (FtGenSeeds not provided or failed)\n", g_opt.corpusDir.c_str());
        return 2;
    }
    if (!g_opt.quiet) {
        fprintf(stdout, "[ft] corpus: %zu seed(s), max-runs=%llu, max-time=%llus, max-len=%zu, hang-timeout=%llums\n",
                seeds.size(), static_cast<unsigned long long>(g_opt.maxRuns),
                static_cast<unsigned long long>(g_opt.maxSeconds), g_opt.maxLen,
                static_cast<unsigned long long>(g_opt.hangTimeoutMs));
    }

    // 父进程兜底：Ctrl-C/kill 时清理子进程
    signal(SIGINT, ParentSignalHandler);
    signal(SIGTERM, ParentSignalHandler);

    // 匿名共享内存：fork 后父子映射同一份，子进程推进计数/进度
    auto* shared = static_cast<FtShared*>(
        mmap(nullptr, sizeof(FtShared), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0));
    if (shared == MAP_FAILED) {
        fprintf(stderr, "[ft] cannot alloc shared state\n");
        return 2;
    }
    shared->execs = 0;
    shared->batchExecs = 0;
    shared->lastProgressMs = NowMs();

    const uint64_t startMs = NowMs();
    const uint64_t deadlineMs = startMs + g_opt.maxSeconds * 1000;
    const uint64_t baseSeed =
        static_cast<uint64_t>(getpid()) * 6364136223846793005ULL + static_cast<uint64_t>(time(nullptr));
    uint64_t batchIndex = 0;
    uint64_t lastReportMs = startMs;
    uint32_t abnormalBatches = 0; // 连续"批次开头即崩溃/挂起"计数（种子自身有缺陷时空转）

    while (g_execs < g_opt.maxRuns) {
        if (NowMs() >= deadlineMs) {
            break;
        }
        const uint64_t batchTarget = std::min<uint64_t>(BATCH_EXECS, g_opt.maxRuns - g_execs);
        // 每批次独立变异流，避免 fork 副本导致序列重复
        const uint64_t rngSeed = baseSeed + batchIndex * 0x9E3779B97F4A7C15ULL;
        const int rc = RunAndMonitorBatch(seeds, shared, batchTarget, rngSeed, deadlineMs);
        if (rc < 0) {
            break; // fork 失败等致命错误
        }
        ++batchIndex;

        // 空转保护：整批执行不足 10 次即异常终止（种子/生成的语料本身必然触发
        // 崩溃或挂起），连续 20 批说明继续跑没有意义，提示后退出
        if (rc != 0 && shared->batchExecs < 10) {
            if (++abnormalBatches >= 20) {
                fprintf(stderr,
                        "[ft] aborting: %u consecutive batches died within the first 10 execs "
                        "(seed/FtGenSeeds is likely broken; check %s)\n",
                        abnormalBatches, g_opt.crashDir.c_str());
                break;
            }
        } else {
            abnormalBatches = 0;
        }

        if (!g_opt.quiet) {
            const uint64_t nowMs = NowMs();
            if (nowMs - lastReportMs >= 5000) {
                lastReportMs = nowMs;
                const double sec = static_cast<double>(nowMs - startMs) / 1000.0 + 1.0;
                fprintf(stdout, "[ft] execs=%llu crashes=%llu hangs=%llu rate=%.0f/s elapsed=%llus\n",
                        static_cast<unsigned long long>(g_execs), static_cast<unsigned long long>(g_crashes),
                        static_cast<unsigned long long>(g_hangs), static_cast<double>(g_execs) / sec,
                        static_cast<unsigned long long>((nowMs - startMs) / 1000));
                fflush(stdout);
            }
        }
    }

    // 汇总（对齐 AFL 关键指标口径；--quiet 只关周期进度，汇总始终输出）
    {
        const uint64_t runMs = NowMs() - startMs;
        const double runSec = static_cast<double>(runMs) / 1000.0;
        fprintf(stdout,
                "[ft] run_time      : %llus\n"
                "[ft] execs_done    : %llu\n"
                "[ft] execs_per_sec : %.0f\n"
                "[ft] saved_crashes : %llu\n"
                "[ft] saved_hangs   : %llu\n",
                static_cast<unsigned long long>(runMs / 1000), static_cast<unsigned long long>(g_execs),
                runSec > 0.0 ? static_cast<double>(g_execs) / runSec : 0.0, static_cast<unsigned long long>(g_crashes),
                static_cast<unsigned long long>(g_hangs));
        fprintf(stdout, "[ft] crash dir: %s\n", g_opt.crashDir.c_str());
        fflush(stdout);
    }
    munmap(shared, sizeof(FtShared));
    return (g_crashes > 0 || g_hangs > 0) ? EXIT_FOUND_BUG : 0;
}
