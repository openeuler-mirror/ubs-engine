/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_cpu_burst_util.h"

#include <dirent.h>
#include <fcntl.h>
#include <linux/sched.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <thread>

#include "ubse_logger.h"

namespace ubse::utils {
UBSE_DEFINE_THIS_MODULE("ubse");
namespace {
constexpr const char* SUDO_PATH = "/usr/bin/sudo";
constexpr const char* SYSTEMCTL_PATH = "/usr/bin/systemctl";
constexpr const char* UBSE_SERVICE = "ubse.service";
constexpr const char* QUOTA_BURST = "CPUQuota=2400%"; // 与 BURST_MEMCPY_COPY_THREADS 对应
constexpr const char* QUOTA_DAILY = "CPUQuota=200%";
constexpr useconds_t QUOTA_RETRY_DELAY_US = 50000; // 与 SEI 降级写重试保持一致
constexpr const char* TASK_DIR = "/proc/self/task";
constexpr const char* BURST_LOG = "[CpuBurst] ";
constexpr std::chrono::seconds IDLE_HOLD_TIMEOUT{10};
// 预开启兜底等待：覆盖预开通知下发到首块拷贝到达的最坏间隔，按最慢导出耗时估算：
// 4GB blocksize * 79ms/GB = 316ms/块，* 1024 最大导出块 ≈ 324s。
// 首块拷贝到达后由 Release 将 deadline 覆写为 IDLE_HOLD_TIMEOUT，不再受此值约束。
constexpr std::chrono::seconds PRE_OPEN_WARMUP_TIMEOUT{325};
constexpr std::chrono::seconds RESTORE_RETRY_TIMEOUT{5};
// 频率保活线程占空比约 2%：自旋 2ms + 休眠 98ms，配合 uclamp_min 将频域频率钉在最高档。
constexpr int KEEPALIVE_SPIN_MS = 2;
constexpr int KEEPALIVE_IDLE_MS = 98;

// uclamp 经 sched_setattr(2) 设置。flag 取值以内核 <linux/sched.h> 为准
// 仅当目标 kernel-headers 早于 5.3 缺失该宏时才使用硬编码兜底（与内核取值一致）。
constexpr uint32_t UCLAMP_MAX_UTIL = 1024;
#ifndef SCHED_FLAG_UTIL_CLAMP_MIN
#define SCHED_FLAG_UTIL_CLAMP_MIN 0x20
#endif
struct UclampAttr {
    uint32_t size;
    uint32_t schedPolicy;
    uint64_t schedFlags;
    int32_t schedNice;
    uint32_t schedPriority;
    uint64_t schedRuntime;
    uint64_t schedDeadline;
    uint64_t schedPeriod;
    uint32_t utilMin;
    uint32_t utilMax;
};

// 设置成功后回读校验：sched_getattr 的 sched_util_min 恒为内核实际生效值
// （flags 不回填 UTIL_CLAMP 位）；CONFIG_UCLAMP_TASK 未开时读 0，据此判定降级。
bool ReadBackUclampMin(uint32_t& utilMin)
{
    UclampAttr got{};
    got.size = sizeof(got);
    if (syscall(SYS_sched_getattr, 0, &got, sizeof(got), 0) != 0) {
        return false;
    }
    utilMin = got.utilMin;
    return true;
}

// 尽力为调用线程设置 uclamp_min=最高档；不可用（内核 < 5.3 或未开 CONFIG_UCLAMP_TASK）时
// 记录一次降级日志后退化为纯自旋。
void ApplyUclampMin()
{
    static std::atomic<bool> degradedLogged{false};
    UclampAttr attr{};
    attr.size = sizeof(attr);
    attr.schedPolicy = SCHED_OTHER;
    attr.schedFlags = SCHED_FLAG_UTIL_CLAMP_MIN;
    attr.utilMin = UCLAMP_MAX_UTIL;
    uint32_t gotMin = 0;
    if (syscall(SYS_sched_setattr, 0, &attr, 0) == 0 && ReadBackUclampMin(gotMin) && gotMin >= UCLAMP_MAX_UTIL) {
        return;
    }
    if (!degradedLogged.exchange(true)) {
        UBSE_LOG_WARN << BURST_LOG << "Uclamp unavailable (kernel <5.3 or CONFIG_UCLAMP_TASK unset), "
                      << "keepalive degrades to spin only";
    }
}
} // namespace

UbseCpuBurstUtil::UbseCpuBurstUtil()
    : idleHoldMs_(IDLE_HOLD_TIMEOUT),
      preOpenWarmupMs_(PRE_OPEN_WARMUP_TIMEOUT),
      restoreRetryMs_(RESTORE_RETRY_TIMEOUT)
{
}

UbseCpuBurstGuard::UbseCpuBurstGuard() : active_(UbseCpuBurstUtil::GetInstance().Acquire()) {}

UbseCpuBurstGuard::~UbseCpuBurstGuard()
{
    if (active_) {
        UbseCpuBurstUtil::GetInstance().Release();
    }
}

UbseResult UbseCpuBurstUtil::RunSystemctlSetProperty(const char* quota, int maxRetry)
{
    for (int retry = 0; retry < maxRetry; ++retry) {
        const pid_t pid = fork();
        if (pid < 0) {
            UBSE_LOG_ERROR << BURST_LOG << "Fork for systemctl set-property failed, retry=" << retry + 1 << "/"
                           << maxRetry << ", errno=" << errno;
            std::this_thread::sleep_for(std::chrono::microseconds(QUOTA_RETRY_DELAY_US));
            continue;
        }
        if (pid == 0) {
            // 子进程立即 exec，仅使用异步信号安全函数；输出丢弃，结果以退出码为准。
            const int devNull = open("/dev/null", O_WRONLY);
            if (devNull >= 0) {
                dup2(devNull, STDOUT_FILENO);
                dup2(devNull, STDERR_FILENO);
                close(devNull);
            }
            // 参数顺序与 sudoers 授权逐字一致，不可调整。
            execl(SUDO_PATH, "sudo", "-n", SYSTEMCTL_PATH, "set-property", "--runtime", UBSE_SERVICE, quota,
                  static_cast<char*>(nullptr));
            _exit(127);
        }
        int status = 0;
        int waitRet = 0;
        // EINTR（如本进程 SIGUSR1 无 SA_RESTART）时子进程可能已 exec 成功，必须重试回收，
        // 否则既留僵尸进程，又会把"实际已生效的 set-property"误判为失败，令 2400% 配额脱离看门狗管理；
        // EINTR 重试不消耗 maxRetry，避免信号风暴下无限重试 fork。
        do {
            waitRet = waitpid(pid, &status, 0);
        } while (waitRet < 0 && errno == EINTR);
        if (waitRet < 0) {
            UBSE_LOG_ERROR << BURST_LOG << "Wait systemctl set-property failed, errno=" << errno;
            continue;
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            UBSE_LOG_INFO << BURST_LOG << "Set-property success, " << quota;
            return UBSE_OK;
        }
        const int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        if (retry < maxRetry - 1) {
            UBSE_LOG_WARN << BURST_LOG << "Set-property failed, " << quota << ", retry=" << retry + 1 << "/" << maxRetry
                          << ", exit=" << exitCode;
            std::this_thread::sleep_for(std::chrono::microseconds(QUOTA_RETRY_DELAY_US));
        } else {
            UBSE_LOG_ERROR << BURST_LOG << "Set-property failed after " << maxRetry << " attempts, " << quota
                           << ", exit=" << exitCode;
        }
    }
    return UBSE_ERROR;
}

UbseResult UbseCpuBurstUtil::RaiseQuota()
{
    return RunSystemctlSetProperty(QUOTA_BURST, 1);
}

UbseResult UbseCpuBurstUtil::RestoreQuota()
{
    return RunSystemctlSetProperty(QUOTA_DAILY, 1);
}

UbseResult UbseCpuBurstUtil::WidenAffinity()
{
    // 目标掩码取全部在线 CPU；内核会自动与 cgroup cpuset 求交，越界部分无效，不产生错误。
    cpu_set_t target{};
    CPU_ZERO(&target);
    const int onlineCpus = sysconf(_SC_NPROCESSORS_ONLN);
    for (int cpu = 0; cpu < onlineCpus && cpu < CPU_SETSIZE; ++cpu) {
        CPU_SET(cpu, &target);
    }
    const int targetCount = CPU_COUNT(&target);
    if (targetCount <= 0) {
        UBSE_LOG_ERROR << BURST_LOG << "Invalid target cpu count=" << targetCount;
        return UBSE_ERROR;
    }

    // 记录调用线程的原始掩码，用于恢复 burst 期间新建的线程（继承自各创建线程，统一收敛回默认值）。
    savedMasks_.clear();
    preWideTids_.clear();
    defaultMaskValid_ = false;
    cpu_set_t callerMask{};
    CPU_ZERO(&callerMask);
    if (sched_getaffinity(0, sizeof(callerMask), &callerMask) == 0) {
        defaultMask_ = callerMask;
        defaultMaskValid_ = true;
    }

    DIR* dir = opendir(TASK_DIR);
    if (dir == nullptr) {
        UBSE_LOG_ERROR << BURST_LOG << "Open " << TASK_DIR << " failed, errno=" << errno;
        return UBSE_ERROR;
    }

    bool hardFail = false;
    size_t widened = 0;
    unsigned int nextCpu = 0;
    dirent* entry = nullptr;
    while (!hardFail && (entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        const pid_t tid = atoi(entry->d_name);
        if (tid <= 0) {
            continue;
        }
        cpu_set_t oldMask{};
        CPU_ZERO(&oldMask);
        if (sched_getaffinity(tid, sizeof(oldMask), &oldMask) != 0) {
            continue; // 线程在遍历期间退出，属正常竞争
        }
        if (CPU_COUNT(&oldMask) >= targetCount) {
            // 已足够宽（如未配置 CPUAffinity 的环境，或业务线程自绑全量在线 CPU）：
            // 不修改也无需快照，但须记录，恢复时不得将其收窄到默认掩码。
            preWideTids_.insert(tid);
            continue;
        }
        if (sched_setaffinity(tid, sizeof(target), &target) != 0) {
            if (errno == ESRCH) {
                continue;
            }
            UBSE_LOG_ERROR << BURST_LOG << "Widen affinity failed, tid=" << tid << ", errno=" << errno;
            hardFail = true;
            break;
        }
        // 强制立即摊开：这些线程长期挤在 0-1 上，仅放宽掩码要等内核负载均衡器
        // （PELT 衰减）花数秒重新分布，迁移前几个 block 会跑在收敛期。
        // 先临时绑到轮转的独立 CPU，内核立即迁移，再放回宽掩码，全程毫秒级。
        if (nextCpu < targetCount) {
            cpu_set_t single{};
            CPU_ZERO(&single);
            CPU_SET(nextCpu, &single);
            (void)sched_setaffinity(tid, sizeof(single), &single);
            (void)sched_setaffinity(tid, sizeof(target), &target);
        }
        ++nextCpu;
        savedMasks_[tid] = oldMask;
        ++widened;
    }
    closedir(dir);
    if (hardFail) {
        // 部分线程修改失败时回滚已放宽的线程，避免状态不明时按宽掩码拷贝。
        RestoreAffinityLocked();
        affinityWidened_ = false;
        return UBSE_ERROR;
    }
    affinityWidened_ = true;
    UBSE_LOG_INFO << BURST_LOG << "Affinity widened, threads=" << widened << ", targetCpus=" << targetCount;
    return UBSE_OK;
}

void UbseCpuBurstUtil::RestoreAffinityLocked()
{
    size_t restored = 0;
    size_t missed = 0;
    for (const auto& [tid, mask] : savedMasks_) {
        if (sched_setaffinity(tid, sizeof(mask), &mask) == 0) {
            ++restored;
        } else if (errno != ESRCH) {
            ++missed;
            UBSE_LOG_ERROR << BURST_LOG << "Restore affinity failed, tid=" << tid << ", errno=" << errno;
        }
    }
    // burst 期间新建的线程不在快照中，收敛回默认掩码。只处理宽于默认掩码的线程：
    // 恢复动作可能因重试或部分失败被再次执行，业务自绑核（不宽于默认）的线程不得被覆盖。
    if (defaultMaskValid_) {
        const int defaultCount = CPU_COUNT(&defaultMask_);
        DIR* dir = opendir(TASK_DIR);
        if (dir != nullptr) {
            dirent* entry = nullptr;
            while ((entry = readdir(dir)) != nullptr) {
                if (entry->d_name[0] == '.') {
                    continue;
                }
                const pid_t tid = atoi(entry->d_name);
                if (tid <= 0 || savedMasks_.count(tid) > 0 || preWideTids_.count(tid) > 0) {
                    continue;
                }
                cpu_set_t cur{};
                CPU_ZERO(&cur);
                if (sched_getaffinity(tid, sizeof(cur), &cur) != 0 || CPU_COUNT(&cur) <= defaultCount) {
                    continue;
                }
                if (sched_setaffinity(tid, sizeof(defaultMask_), &defaultMask_) == 0) {
                    ++restored;
                } else if (errno != ESRCH) {
                    ++missed;
                    UBSE_LOG_ERROR << BURST_LOG << "Restore default affinity failed, tid=" << tid
                                   << ", errno=" << errno;
                }
            }
            closedir(dir);
        } else {
            UBSE_LOG_ERROR << BURST_LOG << "Open " << TASK_DIR << " for restore failed, errno=" << errno;
        }
    }
    savedMasks_.clear();
    preWideTids_.clear();
    if (missed > 0) {
        UBSE_LOG_ERROR << BURST_LOG << "Affinity restored with failure, restored=" << restored << ", missed=" << missed;
    } else {
        UBSE_LOG_INFO << BURST_LOG << "Affinity restored, restored=" << restored;
    }
}

bool UbseCpuBurstUtil::Acquire()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (refCount_.load() > 0) {
        refCount_.fetch_add(1);
        return true;
    }
    if (state_ == State::Holding && resourcesOpen_) {
        // 资源在位：直接复用，savedMasks_/defaultMask_ 仍是首次进入时的原始快照。
        refCount_.store(1, std::memory_order_release);
        state_ = State::Active;
        StopKeepaliveLocked(); // 保活退出点：首块拷贝到达
        idleCv_.notify_all();
        UBSE_LOG_DEBUG << BURST_LOG << "Burst reused in hold, quota=" << QUOTA_BURST;
        return true;
    }
    if (state_ == State::Holding) {
        // 恢复重试中的 Holding（2400% 但亲和性已回滚为窄掩码）：不可复用，
        // 本次拷贝按日常资源执行，看门狗恢复完成后后续块经 Idle 冷开进入 burst。
        UBSE_LOG_WARN << BURST_LOG << "Restore in progress, copy runs with daily resources.";
        return false;
    }
    // Idle：同步冷开（一次性 ~200ms，仅块间 Idle 后的首块支付），失败时调用方按日常资源拷贝。
    // 先确认看门狗可创建——无人兜底恢复则不开启任何资源，杜绝 2400% 残留无主。
    if (!StartWatchdogLocked()) {
        UBSE_LOG_ERROR << BURST_LOG << "Watchdog unavailable, enter burst aborted.";
        return false;
    }
    if (EnterBurstLocked() != UBSE_OK) {
        UBSE_LOG_ERROR << BURST_LOG << "Enter burst failed, fallback to daily threads.";
        return false;
    }
    resourcesOpen_ = true;
    refCount_.store(1, std::memory_order_release);
    state_ = State::Active;
    idleCv_.notify_all();
    UBSE_LOG_INFO << BURST_LOG << "Burst entered, quota=" << QUOTA_BURST;
    return true;
}

bool UbseCpuBurstUtil::PreOpen()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == State::Active || (state_ == State::Holding && resourcesOpen_)) {
        // Active：置标记由 Release 保留预热窗口；资源在位的 Holding：直接续期并恢复保活。
        if (state_ == State::Active) {
            pendingPreOpen_ = true;
            UBSE_LOG_DEBUG << BURST_LOG << "PreOpen deferred to release, burst already active.";
            return true;
        }
        deadline_ = std::chrono::steady_clock::now() + preOpenWarmupMs_;
        StartKeepaliveLocked();
        idleCv_.notify_all();
        UBSE_LOG_DEBUG << BURST_LOG << "PreOpen refreshed warmup hold, quota=" << QUOTA_BURST;
        return true;
    }
    if (state_ == State::Holding) {
        // 恢复重试中的 Holding：不刷新窗口（保住 5s 恢复节奏），本次预开未生效。
        UBSE_LOG_WARN << BURST_LOG << "Restore in progress, pre-open skipped.";
        return false;
    }
    // 先确认看门狗可创建——无人兜底恢复则不开启任何资源（先于提配额检查，简化失败回滚）。
    if (!StartWatchdogLocked()) {
        UBSE_LOG_ERROR << BURST_LOG << "Watchdog unavailable, pre-open aborted, no resources raised.";
        return false;
    }
    if (EnterBurstLocked() != UBSE_OK) {
        // 回滚失败时状态已由 EnterBurstLocked 移交看门狗恢复重试，此处仅上报失败。
        UBSE_LOG_WARN << BURST_LOG << "Pre-open failed, copy will fallback to daily resources.";
        return false;
    }
    resourcesOpen_ = true;
    // 不占拷贝引用：进入预开等待，由看门狗按预热窗口兜底恢复。
    state_ = State::Holding;
    deadline_ = std::chrono::steady_clock::now() + preOpenWarmupMs_;
    StartKeepaliveLocked();
    idleCv_.notify_all();
    UBSE_LOG_INFO << BURST_LOG << "Pre-opened, warmup hold " << preOpenWarmupMs_.count() << "ms, quota=" << QUOTA_BURST;
    return true;
}

UbseResult UbseCpuBurstUtil::EnterBurstLocked()
{
    // 先提配额；失败则整体不进入 burst（调用方按原线程数拷贝）。
    if (RaiseQuota() != UBSE_OK) {
        return UBSE_ERROR;
    }
    quotaRaised_ = true;
    if (WidenAffinity() != UBSE_OK) {
        UBSE_LOG_ERROR << BURST_LOG << "Widen affinity failed, rollback quota.";
        resourcesOpen_ = false;
        if (RestoreQuota() == UBSE_OK) {
            quotaRaised_ = false;
        } else if (StartWatchdogLocked()) {
            // 回滚失败：不能让 2400% 残留无主，移交看门狗短周期恢复重试（亲和性已回滚，仅需补配额）。
            state_ = State::Holding;
            deadline_ = std::chrono::steady_clock::now() + restoreRetryMs_;
            idleCv_.notify_all();
            UBSE_LOG_ERROR << BURST_LOG << "Quota rollback failed, watchdog will retry restore.";
        } else {
            // 看门狗不可用且回滚失败：仅能依赖 unit 的 ExecStartPre/ExecStopPost 兜底。
            UBSE_LOG_ERROR << BURST_LOG << "CRITICAL: quota rollback failed and watchdog unavailable, " << UBSE_SERVICE
                           << " remains " << QUOTA_BURST;
        }
        return UBSE_ERROR;
    }
    resourcesOpen_ = true;
    return UBSE_OK;
}

void UbseCpuBurstUtil::Release()
{
    std::lock_guard<std::mutex> lock(mutex_);
    const int prev = refCount_.fetch_sub(1);
    if (prev > 1) {
        return;
    }
    // 防御：prev<=1 统一归零，避免异常路径出现负值导致后续 Acquire 判断错误。
    refCount_.store(0, std::memory_order_release);
    if (state_ != State::Active) {
        return;
    }
    // 不立即恢复：进入空闲保持期，由看门狗在超时后统一恢复，
    // 一次迁移的全部 block 因此只做一次资源切换。
    // Active 期间收到过预开时保留完整预热窗口，供下一轮迁移的首块直接复用。
    const bool warmupKept = pendingPreOpen_;
    state_ = State::Holding;
    const auto hold = warmupKept ? preOpenWarmupMs_ : idleHoldMs_;
    deadline_ = std::chrono::steady_clock::now() + hold;
    pendingPreOpen_ = false;
    if (warmupKept) {
        StartKeepaliveLocked(); // 预开保留的 325s 窗口内维持频率，供下一轮迁移首块直接全速拷贝
    }
    idleCv_.notify_all();
    UBSE_LOG_DEBUG << BURST_LOG << "Burst idle, hold " << hold.count() << "s before restore, warmupKept=" << warmupKept;
}

bool UbseCpuBurstUtil::StartWatchdogLocked()
{
    if (watchdogStarted_) {
        return true; // 已启动（detach 后 joinable 恒为 false，不能用 joinable 判重）
    }
    try {
        // 先构造成功再置位：构造失败（如线程数受限）不能留下"已启动"假象导致恢复永不再启动。
        std::thread t([this] { WatchdogLoop(); });
        watchdogStarted_ = true;
        t.detach(); // 进程生命周期线程，随进程退出，无静态析构问题（leaky 单例）
    } catch (...) {
        UBSE_LOG_ERROR << BURST_LOG << "Create watchdog thread failed";
        return false;
    }
    return true;
}

void UbseCpuBurstUtil::WatchdogLoop()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!watchdogStop_) {
        if (state_ != State::Holding) {
            idleCv_.wait(lock, [this] { return watchdogStop_ || state_ == State::Holding; });
            continue;
        }
        const auto waited = idleCv_.wait_until(lock, deadline_);
        if (watchdogStop_ || state_ != State::Holding || waited == std::cv_status::no_timeout) {
            continue;
        }
        StopKeepaliveLocked(); // 保活退出点：看门狗恢复
        // 恢复在锁内完成，与并发 Acquire 串行化，避免看门狗写回 200% 覆盖新会话已提高的 2400%。
        // 顺序必须先配额后亲和：配额失败时保持快照与宽亲和不动（复用路径不会拿窄掩码跑 24 线程），
        // 亲和性只在配额恢复成功后执行一次。
        if (quotaRaised_) {
            if (RestoreQuota() != UBSE_OK) {
                deadline_ = std::chrono::steady_clock::now() + restoreRetryMs_;
                UBSE_LOG_ERROR << BURST_LOG << "Restore " << QUOTA_DAILY << " failed, retry in "
                               << restoreRetryMs_.count() << "ms, " << UBSE_SERVICE << " remains " << QUOTA_BURST;
                continue;
            }
            quotaRaised_ = false;
        }
        if (affinityWidened_) {
            // 亲和性处于放宽态才执行恢复：放宽中途失败的路径（WidenAffinity 内部已回滚、
            // 快照已清空）只需重试降配，绝不能拿空快照把已恢复独立绑核的线程收窄到默认掩码。
            RestoreAffinityLocked();
            affinityWidened_ = false;
        }
        resourcesOpen_ = false;
        state_ = State::Idle;
        UBSE_LOG_INFO << BURST_LOG << "Hold timeout, restored quota=" << QUOTA_DAILY;
    }
}

void UbseCpuBurstUtil::StartKeepaliveLocked()
{
    if (keepaliveActive_) {
        return;
    }
    const long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus <= 0) {
        return;
    }
    keepaliveActive_ = true;
    // 每线程携带代数：stop/start 都递增，旧线程即使错过退出时机也会在下次唤醒时按代数退出，
    // 从机制上杜绝"stop 后 98ms 休眠内重启导致新旧两批线程并存"。
    const auto gen = keepaliveGen_.fetch_add(1, std::memory_order_relaxed) + 1;
    // 每 CPU 一个微占空比线程：频率按频域（cluster/policy）调节，单线程只能钉住所在频域，
    // 需覆盖全部 CPU 才能消除首块拷贝的频率爬坡；线程各自绑核，负载约 2%/CPU。
    const unsigned int count = static_cast<unsigned int>(std::min<long>(cpus, CPU_SETSIZE));
    unsigned int created = 0;
    try {
        for (unsigned int cpu = 0; cpu < count; ++cpu) {
            std::thread(&UbseCpuBurstUtil::KeepaliveWorker, this, cpu, gen).detach();
            ++created;
        }
    } catch (...) {
        // 部分创建失败不影响已创建线程（按代数正常退出），下轮 stop/start 重建。
        UBSE_LOG_ERROR << BURST_LOG << "Create keepalive threads incomplete, created=" << created << "/" << count;
    }
    UBSE_LOG_INFO << BURST_LOG << "Keepalive started, threads=" << created << "/" << count;
}

void UbseCpuBurstUtil::StopKeepaliveLocked()
{
    if (!keepaliveActive_) {
        return;
    }
    keepaliveActive_ = false;
    keepaliveGen_.fetch_add(1, std::memory_order_release); // 旧代线程自旋间隙或睡醒后自行退出，无需 join
    UBSE_LOG_DEBUG << BURST_LOG << "Keepalive stopped";
}

void UbseCpuBurstUtil::KeepaliveWorker(unsigned int cpu, uint32_t gen)
{
    cpu_set_t single{};
    CPU_ZERO(&single);
    CPU_SET(cpu, &single);
    (void)sched_setaffinity(0, sizeof(single), &single);
    ApplyUclampMin();
    // 微占空比自旋：uclamp_min 使所在 CPU 频率钉在最高档；uclamp 不可用时退化为纯自旋，
    // 频率在自旋间隙会回落，预热效果衰减但仍优于完全空闲。
    while (keepaliveGen_.load(std::memory_order_acquire) == gen) {
        const auto spinEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(KEEPALIVE_SPIN_MS);
        volatile uint64_t sink = 0;
        while (std::chrono::steady_clock::now() < spinEnd) {
            sink = sink + 6364136223846793005ULL + 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(KEEPALIVE_IDLE_MS));
    }
}
} // namespace ubse::utils
