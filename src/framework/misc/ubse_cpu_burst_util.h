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

#ifndef UBSE_CPU_BURST_UTIL_H
#define UBSE_CPU_BURST_UTIL_H

#include <sched.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <thread>

#include "ubse_common_def.h"
#include "ubse_error.h"

namespace ubse::utils {
using ubse::common::def::UbseResult;
class UbseCpuBurstUtil;

/**
 * 迁移拷贝 CPU burst 作用域守卫（RAII）。
 * 构造时进入 burst（引用计数 0→1 时真正执行资源切换），析构时退出；
 * 最后一个拷贝退出后不立即恢复，由内部看门狗在空闲超时后自动恢复，
 */
class UbseCpuBurstGuard {
public:
    UbseCpuBurstGuard();
    ~UbseCpuBurstGuard();
    UbseCpuBurstGuard(const UbseCpuBurstGuard&) = delete;
    UbseCpuBurstGuard(UbseCpuBurstGuard&&) = delete;
    UbseCpuBurstGuard& operator=(const UbseCpuBurstGuard&) = delete;
    UbseCpuBurstGuard& operator=(UbseCpuBurstGuard&&) = delete;

    // 是否成功进入 burst（false 时调用方按原线程数拷贝）
    bool IsActive() const
    {
        return active_;
    }

private:
    bool active_;
};

/**
 * 迁移拷贝 CPU burst 管理：
 * 进入时通过 systemctl set-property --runtime 临时提高 ubse.service 的 CPUQuota，
 * 并放宽本进程全部线程的 CPU 亲和性（快照原始掩码）。
 * 最后一个拷贝退出后进入空闲保持期，超过空闲超时仍无新拷贝则自动恢复配额与亲和性。
 * 运行时配额的跨重启残留由 ubse.service 的 ExecStartPre/ExecStopPost 以 root 恢复，
 */
class UbseCpuBurstUtil {
public:
    // leaky 单例：实例与看门狗线程均不参与静态析构，生命周期为整个进程
    static UbseCpuBurstUtil& GetInstance()
    {
        static UbseCpuBurstUtil* instance = new UbseCpuBurstUtil();
        return *instance;
    }

    // 进入 burst：Holding 复用或引用叠加时返回 true；Idle 时同步冷开（一次性 ~200ms），失败降级返回 false。
    bool Acquire();
    // 退出 burst：引用计数 1→0 时进入空闲保持期；空闲超时后由看门狗恢复。
    void Release();
    // 预开启（幂等）：Accept 成功后由迁移通知路径调用，提前完成配额提升与亲和性放宽，
    // 使首块拷贝免于资源切换开销。资源在位时刷新预热窗口；恢复重试中的 Holding 不刷新。
    bool PreOpen();
    // 是否处于 burst（供拷贝路径选择线程数，无锁读取）。
    bool IsActive() const
    {
        return refCount_.load(std::memory_order_acquire) > 0;
    }

    UbseCpuBurstUtil(const UbseCpuBurstUtil&) = delete;
    UbseCpuBurstUtil(UbseCpuBurstUtil&&) = delete;
    UbseCpuBurstUtil& operator=(const UbseCpuBurstUtil&) = delete;
    UbseCpuBurstUtil& operator=(UbseCpuBurstUtil&&) = delete;

private:
    UbseCpuBurstUtil();
    ~UbseCpuBurstUtil() = default;

    // 状态机：Idle（未 burst）→ Active（有拷贝引用）→ Holding（空闲保持/预开等待，看门狗计时）
    // → Idle（看门狗恢复）；Holding 期间新 Acquire 回到 Active 并取消计时。
    enum class State : uint8_t
    {
        Idle,
        Active,
        Holding
    };

    // 以下三个接口为 virtual 仅供 UT 注入桩实现（脱离 sudo/systemctl/真实 sched 依赖），生产行为固定。
    virtual UbseResult RaiseQuota();
    virtual UbseResult RestoreQuota();
    // 提配额 + 放宽亲和性（失败时处理回滚与恢复重试接管），需在持有 mutex_ 时调用。
    UbseResult EnterBurstLocked();
    // 快照并放宽本进程全部线程的亲和性；部分失败时回滚已修改的线程。
    virtual UbseResult WidenAffinity();
    // 恢复亲和性：快照内线程恢复各自原始掩码，快照外仅收敛宽于默认掩码的线程（可安全重入）。
    // 需在持有 mutex_ 时调用。
    void RestoreAffinityLocked();
    // 执行固定参数的 sudo systemctl set-property --runtime，maxRetry 次尝试。
    static UbseResult RunSystemctlSetProperty(const char* quota, int maxRetry);
    // 懒启动看门狗线程（需在持有 mutex_ 时调用）；线程创建失败返回 false，由调用方兜底。
    bool StartWatchdogLocked();
    void WatchdogLoop();
    // 频率保活线程：每 CPU 一个微占空比自旋线程 + uclamp_min 钳位，
    // 仅存在于预开等待窗口；退出点为首块拷贝到达与看门狗恢复。
    void StartKeepaliveLocked();
    void StopKeepaliveLocked();
    void KeepaliveWorker(unsigned int cpu, uint32_t gen);

    std::mutex mutex_;
    std::atomic<int> refCount_{0};
    State state_{State::Idle};
    std::condition_variable idleCv_;
    std::chrono::steady_clock::time_point deadline_{}; // Holding 态的超时点（预开等待或空闲保持）
    // 保持/重试窗口时长：生产由构造函数取固定常量（10s/325s/5s），UT 注入毫秒级窗口加速看门狗路径验证。
    std::chrono::milliseconds idleHoldMs_{};
    std::chrono::milliseconds preOpenWarmupMs_{};
    std::chrono::milliseconds restoreRetryMs_{};
    bool watchdogStarted_{false}; // 看门狗为 detach 线程，joinable 判重恒 false，需独立标记
    bool watchdogStop_{false}; // 看门狗循环退出条件受 mutex_ 保护：置位并 notify 后循环安全退出
    bool quotaRaised_{false};
    bool resourcesOpen_{false};   // burst 资源完整在位（2400%+宽亲和+快照有效）；
                                  // 区分"恢复重试中的 Holding"，该状态下不可复用、预开不续时
    bool affinityWidened_{false}; // 亲和性处于放宽态；放宽中途失败经内部回滚后为 false，
                                  // 看门狗据此跳过亲和恢复，避免空快照覆盖业务绑核
    bool pendingPreOpen_{false};  // Active 期间收到预开：Release 时保留完整预热窗口
    bool keepaliveActive_{false};
    std::atomic<uint32_t> keepaliveGen_{0};   // 保活线程代数：start/stop 均递增，杜绝旧线程复活
    std::map<pid_t, cpu_set_t> savedMasks_{}; // 进入 burst 时各线程的原始亲和性掩码
    std::set<pid_t> preWideTids_{}; // burst 前已宽于等于目标全集的存量线程（未修改、无需快照），
        // 恢复收窄快照外线程时排除，避免把业务自绑全量 CPU 的线程收窄到默认掩码
    cpu_set_t defaultMask_{}; // 进入 burst 时调用线程的掩码，用于恢复新建线程
    bool defaultMaskValid_{false};
};
} // namespace ubse::utils
#endif // UBSE_CPU_BURST_UTIL_H
