/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_SSU_RECONCILE_STATE_H
#define UBSE_SSU_RECONCILE_STATE_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace ubse::ssu::service {

// 疑似本地残留挂载空间名集合（master账本恢复后反查探测构建）。
// 记录丢失但本机存在NS设备的独占空间，attacher未知，用于拒绝首个attach防双重挂载。
// 线程安全：恢复流程构建与verify/attach/free并发访问
class UbseSsuSuspectedMountTable {
public:
    UbseSsuSuspectedMountTable() = default;
    ~UbseSsuSuspectedMountTable() = default;
    UbseSsuSuspectedMountTable(const UbseSsuSuspectedMountTable &) = delete;
    UbseSsuSuspectedMountTable &operator=(const UbseSsuSuspectedMountTable &) = delete;

    // 是否在疑似集合中
    bool Contains(const std::string &name) const;
    // 全量重建替换（每次恢复整表重建）
    void Swap(std::unordered_set<std::string> &&suspected);
    // 单个摘除（space被free后自愈出口）
    void Erase(const std::string &name);

private:
    mutable std::mutex mtx_;
    std::unordered_set<std::string> suspected_;
};

// 设备故障防抖计数（空间名→连续缺失采集周期数）。
// 由collector周期对账维护：硬件重现即清零，连续缺失达阈值才判FAULTED
class UbseSsuFaultDebouncer {
public:
    UbseSsuFaultDebouncer() = default;
    ~UbseSsuFaultDebouncer() = default;
    UbseSsuFaultDebouncer(const UbseSsuFaultDebouncer &) = delete;
    UbseSsuFaultDebouncer &operator=(const UbseSsuFaultDebouncer &) = delete;

    // 递增连续缺失计数，返回递增后的值
    uint32_t Increment(const std::string &name);
    // 查询当前计数（无记录返回0），供跨对账权责判断（如name级回正前检查身份级故障是否在途）
    uint32_t GetCount(const std::string &name) const;
    // 空间恢复在线：计数清零
    void Reset(const std::string &name);
    // 清理已不在账本管理集合中的空间的残留计数（free/Remove后防抖表不泄漏）
    void PruneStale(const std::unordered_set<std::string> &managedNames);

private:
    mutable std::mutex mtx_;
    std::unordered_map<std::string, uint32_t> counters_;
};

// 重挂连续失败计数（空间名→次数，内存级，进程重启清零）。
// 线程安全：恢复流程（executor线程）与本地挂载对账定时器（timer线程）并发访问
class UbseSsuRemountFailCounter {
public:
    UbseSsuRemountFailCounter() = default;
    ~UbseSsuRemountFailCounter() = default;
    UbseSsuRemountFailCounter(const UbseSsuRemountFailCounter &) = delete;
    UbseSsuRemountFailCounter &operator=(const UbseSsuRemountFailCounter &) = delete;

    // 递增失败计数，返回递增后的值
    uint32_t Increment(const std::string &name);
    // 重挂成功即清零（偶发失败不累积）
    void Reset(const std::string &name);

private:
    mutable std::mutex mtx_;
    std::unordered_map<std::string, uint32_t> counts_;
};

// 节点离线追踪：首次离线时刻（幂等，抖动重复事件不刷新）与已达阈值完成处理记录
// （防周期重复告警/摘除）。仅master维护
class UbseSsuNodeOfflineTracker {
public:
    UbseSsuNodeOfflineTracker() = default;
    ~UbseSsuNodeOfflineTracker() = default;
    UbseSsuNodeOfflineTracker(const UbseSsuNodeOfflineTracker &) = delete;
    UbseSsuNodeOfflineTracker &operator=(const UbseSsuNodeOfflineTracker &) = delete;

    // 节点离线：记录首次离线时刻（已存在则保持原时刻）
    void OnDown(const std::string &nodeId);
    // 节点回归：清除离线与已处理记录（回归后重报自动补回挂载关系）
    void OnUp(const std::string &nodeId);
    // 当前处于离线记录中的节点数（日志观测用）
    size_t DownCount() const;
    // 锁内快照挑出离线持续超阈值且未处理过的节点并标记已处理，
    // 调用方在锁外做账本操作避免锁交叉
    std::unordered_set<std::string> TakeExpired(int64_t thresholdSeconds);

private:
    mutable std::mutex mtx_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> since_; // 节点首次离线时刻
    std::unordered_set<std::string> processed_; // 已达阈值完成处理的离线节点
};

// 对账定时器启停/单轮防重入标志对：started_保证注册幂等（失败可回滚重试），
// running_保证单轮执行防重入（RPC阻塞/重试退避sleep期间跳过后续tick）
class UbseSsuReconcileTimerFlags {
public:
    UbseSsuReconcileTimerFlags() = default;
    ~UbseSsuReconcileTimerFlags() = default;
    UbseSsuReconcileTimerFlags(const UbseSsuReconcileTimerFlags &) = delete;
    UbseSsuReconcileTimerFlags &operator=(const UbseSsuReconcileTimerFlags &) = delete;

    // CAS标记已启动：返回true表示此前未启动（调用方应执行注册），false表示已启动跳过
    bool TryMarkStarted();
    // 注册失败回滚，允许后续重试
    void ClearStarted();
    // 反注册时消费启动标志：返回true表示此前已启动（调用方应执行反注册），false表示未启动跳过
    bool TryStop();
    // 单轮防重入进入：返回true表示本轮获得执行权，false表示上轮未结束跳过
    bool TryEnterRound();
    // 单轮结束释放（RAII guard析构/异常路径复位）
    void ExitRound();

private:
    std::atomic<bool> started_{false}; // 定时器是否已注册
    std::atomic<bool> running_{false}; // 单轮执行防重入
};

// 平账批量执行单飞守卫：对账批量项移交executor异步执行后，300s运营对账定时器
// 下一轮可能在上一批未执行完时重复下发，对同一空间重复REMOUNT/CLEANUP。
// handler受理前CAS抢占，整批执行完毕（含异常路径）释放
class UbseSsuReconcileBatchGuard {
public:
    UbseSsuReconcileBatchGuard() = default;
    ~UbseSsuReconcileBatchGuard() = default;
    UbseSsuReconcileBatchGuard(const UbseSsuReconcileBatchGuard &) = delete;
    UbseSsuReconcileBatchGuard &operator=(const UbseSsuReconcileBatchGuard &) = delete;

    // CAS进入批量执行：返回true表示获得执行权，false表示上批仍在执行（应拒绝受理）
    bool TryEnter();
    // 整批执行完毕释放
    void Exit();

private:
    std::atomic<bool> running_{false}; // 批量执行单飞标志
};
} // namespace ubse::ssu::service
#endif // UBSE_SSU_RECONCILE_STATE_H
