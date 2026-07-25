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
#include <mutex>
#include <string>
#include <unordered_set>

namespace ubse::mem::controller {

/**
 * UbseInitLedgerState 用于维护各节点是否已完成初始对账的状态。
 * 状态变更由 LedgerHandler（集群状态通知处理器）驱动：
 * - 收到 INIT   -> SetInitLedgerDone(nodeId, false)：标记初始对账未完成
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

private:
    UbseInitLedgerState() = default;
    ~UbseInitLedgerState() = default;
    UbseInitLedgerState(const UbseInitLedgerState&) = delete;
    UbseInitLedgerState& operator=(const UbseInitLedgerState&) = delete;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::unordered_set<std::string> initLedgerDoneSet_;
};

} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_INIT_LEDGER_STATE_H