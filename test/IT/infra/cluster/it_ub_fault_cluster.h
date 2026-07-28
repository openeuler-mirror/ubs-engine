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

#ifndef IT_UB_FAULT_CLUSTER_H
#define IT_UB_FAULT_CLUSTER_H

#include "it_cluster.h"
#include "it_console_log.h"

namespace ubse::it::infra {

/**
 * @brief UB 特性故障状态机 helper（无状态持有者，可被任意 UB 故障场景复用）。
 *
 * 从 ItScenarioFixture 中剥离的「故障注入/恢复 + 节点重启」逻辑。持有
 * ItCluster 引用 + 内部状态，提供 EnsureFaultInjected / EnsureNormal 两个
 * 幂等入口，避免每个 scenario 都重复实现一份相同的状态机。
 *
 * 状态机：
 *   - kNormal: 集群已启动、UB 特性全部可用
 *   - kFaultInjected: UB 特性故障已注入、所有节点已重启、维度记录在 currentDims_
 *
 * 复用规则：
 *   - 同维度故障态切换：直接返回，不重启（最大粒度复用）
 *   - 跨维度或从正常态进入故障态：注入 → 重启所有节点
 *   - 从故障态回正常态：恢复 → 重启所有节点
 *
 * 注意：内部使用 ASSERT_IT_OK，必须在 TEST_F / SetUpTestSuite 调用栈中
 * 调用（与原 scenario 实现一致）。
 */
class UbFaultCluster {
public:
    explicit UbFaultCluster(ItCluster& cluster) : cluster_(cluster) {}

    /**
     * @brief 进入指定故障态。
     *
     * - 当前已是同维度故障态：直接返回（复用，不重启）
     * - 当前是不同维度故障态或正常态：注入新维度 → 重启所有节点
     *
     * @param borrowDisabled  true: UB Memory Borrowing 改为 '-' (Borrowing 不可用)
     * @param shareNcDisabled true: UB Memory Sharing(Non Cacheable) 改为 '-' (Sharing NC 不可用)
     * @param shareCcDisabled true: UB Memory Sharing(Cacheable) 改为 '-' (Sharing CC 不可用)
     */
    void EnsureFaultInjected(bool borrowDisabled, bool shareNcDisabled, bool shareCcDisabled);

    /**
     * @brief 回到正常态。
     *
     * - 当前已是正常态：直接返回
     * - 当前是故障态：恢复 mock → 重启所有节点
     */
    void EnsureNormal();

private:
    enum class State
    {
        kNormal,
        kFaultInjected
    };

    struct UbFaultDims {
        bool borrowDisabled{false};
        bool shareNcDisabled{false};
        bool shareCcDisabled{false};
        bool operator==(const UbFaultDims& other) const
        {
            return borrowDisabled == other.borrowDisabled && shareNcDisabled == other.shareNcDisabled &&
                   shareCcDisabled == other.shareCcDisabled;
        }
    };

    void DoRestore();
    void RestartAll();

    ItCluster& cluster_;
    State state_{State::kNormal};
    UbFaultDims currentDims_{};
};

} // namespace ubse::it::infra

#endif // IT_UB_FAULT_CLUSTER_H
