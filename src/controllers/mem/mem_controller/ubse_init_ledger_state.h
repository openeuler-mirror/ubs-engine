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

#ifndef UBS_ENGINE_UBSE_INIT_LEDGER_STATE_H
#define UBS_ENGINE_UBSE_INIT_LEDGER_STATE_H

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace ubse::mem::controller {

enum class MigrationRecoveryAction
{
    WAIT_FOR_RECONCILIATION,    // 等待本轮完成对账的节点达到配置数量
    REBUILD_MIGRATION_TASKS,    // 首次统一重建迁移任务
    RECOVERY_ALREADY_TRIGGERED, // 本轮恢复已触发，不再进行重建扫描
};

/**
 * UbseInitLedgerState 维护各节点是否已完成初始对账的状态，供控制器和调度器共享。
 * 状态变更由 LedgerHandler（集群状态通知处理器）驱动：
 * - 收到 INIT -> SetInitLedgerDone(nodeId, false)：标记初始对账未完成
 * - 收到 WORKING -> SetInitLedgerDone(nodeId, true)：标记初始对账已完成，唤醒等待线程
 */
class UbseInitLedgerState {
public:
    static UbseInitLedgerState& GetInstance()
    {
        static UbseInitLedgerState instance;
        return instance;
    }

    /**
     * 更新节点初始对账完成状态。
     * done=true: 将 nodeId 加入 set，表示初始对账已完成，释放锁后 notify_all 唤醒等待线程
     * done=false: 将 nodeId 从 set 移除，表示初始对账未完成（INIT/重入等场景）
     */
    void SetInitLedgerDone(const std::string& nodeId, bool done);

    /**
     * 查询节点是否已完成初始对账。
     * nodeId 在 set 中返回 true，不在 set 中返回 false
     */
    bool IsInitLedgerDone(const std::string& nodeId);

    // 返回当前已完成初始对账的节点列表快照（锁内拷贝）。
    std::vector<std::string> GetInitLedgerDoneNodes();

    /**
     * 等待节点完成初始对账
     * 若 predicate(initLedgerDoneSet_.count(nodeId) > 0) 入口为 true，立即返回 true
     * 若为 false，通过 condition_variable 睡眠等待，状态变更为 WORKING 时被唤醒重新检查
     * 超时后 predicate 仍为 false 则返回 false
     * @param nodeId    待等待的节点ID
     * @param timeoutMs 最大等待时间（毫秒）
     * @return true=初始对账已完成，false=超时未完成
     */
    bool WaitInitLedgerDone(const std::string& nodeId, uint32_t timeoutMs);

    // 主上线时同时清空旧对账记录、重置恢复入口，必须在本轮对账任务启动前调用。
    void ResetMigrationRecovery(bool leader);
    // 调用方确认主身份；完成对账的节点达到配置数量后，占用唯一重建入口。
    MigrationRecoveryAction TryStartMigrationRecovery(std::size_t expectedNodeCount);
    // 投递失败时释放入口，已经执行的重建不重置。
    void RetryMigrationRecovery();
    // 恢复扫描（RecoverAfterReconciliation）执行完毕后调用，消除"名单收齐但扫描未执行完"期间对账状态判定的竞态窗口。
    void MarkMigrationRecoveryCompleted();
    // 恢复扫描是否已执行完毕；名单收齐但扫描未完成期间，get/accept/cancel 仍按对账采集中拦截。
    bool IsMigrationRecoveryCompleted();

private:
    UbseInitLedgerState() = default;
    ~UbseInitLedgerState() = default;
    UbseInitLedgerState(const UbseInitLedgerState&) = delete;
    UbseInitLedgerState& operator=(const UbseInitLedgerState&) = delete;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::unordered_set<std::string> initLedgerDoneSet_;
    bool migrationRecoveryAllowed_{false}; // 是否允许启动本轮迁移恢复，默认不允许
    bool migrationRecoveryCompleted_{false}; // 本轮恢复扫描是否已执行完毕，用于消除对账完成判定的竞态窗口
};

} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_INIT_LEDGER_STATE_H