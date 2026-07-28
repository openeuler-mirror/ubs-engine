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

#ifndef TONGSUAN_1D_TWO_NODES_UB_FEATURE_FAULT_SCENARIO_H
#define TONGSUAN_1D_TWO_NODES_UB_FEATURE_FAULT_SCENARIO_H

#include <cstdint>
#include <memory>
#include <string>

#include <unistd.h>

#include "it_assertion.h"
#include "it_cluster.h"
#include "it_cluster_builder.h"
#include "it_console_log.h"
#include "it_scenario_fixture.h"
#include "ubs_engine.h"
#include "ubs_engine_mem.h"

namespace ubse::it::infra {

/**
 * @brief 双节点 UB 特性故障场景 fixture（状态化）。
 *
 * 继承 ItScenarioFixture，但因其需要状态机管理（故障注入/恢复 + 节点重启），实现 SetUpTestSuite/TearDownTestSuite。
 *
 * 状态机：
 *   - kNormal: 集群启动、UB 特性全部可用、基线借用已建立
 *   - kFaultInjected: UB 特性故障已注入、两节点已重启、故障维度记录在 currentDims_
 *
 * 关键约束：
 *   - 故障生效需重启节点进程让 UbseConfModule::LoadUbFeature() 重读
 *   - RestartNode() 不重建 sysfs 树，mock 内容保持
 *   - StopCluster() + StartCluster() 会覆盖 mock 内容，禁止在故障态使用
 *   - SetUp/TearDown 阶段都要求无故障：TearDown 时若仍处于故障态会先恢复
 *
 * 复用规则：
 *   - 同维度故障态切换：直接返回，不重启（最大粒度复用）
 *   - 跨维度或从正常态进入故障态：注入 → 重启两节点
 *   - 从故障态回正常态：恢复 → 重启两节点
 */
class Tongsuan1dTwoNodesUbFeatureFaultScenario : public ItScenarioFixture {
public:
    static constexpr const char* Name()
    {
        return "Tongsuan1dTwoNodesUbFeatureFaultScenario";
    }

    /**
     * @brief SetUpTestSuite: 启动双节点集群 → state_=kNormal。
     *
     * 基线借用用于 TearDown 时归还，验证故障恢复后内存归还能力。
     * SetUp 阶段保证无 UB 故障（mock 文件为默认全 '+'）。
     */
    static void SetUpTestSuite()
    {
        IT_LOG_INFO << Name() << ": starting cluster...";
        auto ret = MakeBuilder()
                       .Tongsuan()
                       .TwoNode()
                       .WithNodeConfig("1", "ubse.log", "log.level", "DEBUG")
                       .WithNodeConfig("2", "ubse.log", "log.level", "DEBUG")
                       .Start(cluster_);
        ASSERT_IT_OK(ret);
        workDir_ = cluster_->GetBaseWorkDir();
        IT_LOG_INFO << Name() << ": cluster started";
        state_ = UbFaultState::kNormal;
    }

    static void TearDownTestSuite()
    {
        if (cluster_) {
            IT_LOG_INFO << Name() << ": stopping cluster...";
            cluster_->StopCluster();
            cluster_.reset();
        }
        CleanupWorkDir(workDir_);
    }

    static ItCluster& Cluster()
    {
        return *cluster_;
    }
    /**
     * @brief 进入指定故障态。
     *
     * - 当前已是同维度故障态：直接返回（复用，不重启）
     * - 当前是不同维度故障态：注入新维度 → 重启
     * - 当前是正常态：注入 → 重启两节点
     *
     * @param borrowDisabled  true: UB Memory Borrowing 改为 '-' (Borrowing 不可用)
     * @param shareNcDisabled true: UB Memory Sharing(Non Cacheable) 改为 '-' (Sharing NC 不可用)
     * @param shareCcDisabled true: UB Memory Sharing(Cacheable) 改为 '-' (Sharing CC 不可用)
     */
    static void EnsureFaultInjected(bool borrowDisabled, bool shareNcDisabled, bool shareCcDisabled)
    {
        const UbFaultDims target{borrowDisabled, shareNcDisabled, shareCcDisabled};
        if (state_ == UbFaultState::kFaultInjected && currentDims_ == target) {
            IT_LOG_INFO << Name() << ": already in same fault state, reuse (no restart)";
            return;
        }
        // 当前是不同维度故障态或正常态，注入新故障维度
        IT_LOG_INFO << Name() << ": injecting fault (borrow=" << borrowDisabled << ", shareNc=" << shareNcDisabled
                    << ", shareCc=" << shareCcDisabled << ")";
        for (const auto& id : cluster_->GetNodeIds()) {
            cluster_->GetNode(id).SetUbFeatureFault(borrowDisabled, shareNcDisabled, shareCcDisabled);
        }
        RestartAllNodes();
        currentDims_ = target;
        state_ = UbFaultState::kFaultInjected;
    }

    /**
     * @brief 回到正常态。
     *
     * - 当前已是正常态：直接返回
     * - 当前是故障态：恢复 mock → 重启两节点
     */
    static void EnsureNormal()
    {
        if (state_ == UbFaultState::kNormal) {
            return;
        }
        DoRestoreUbFeature();
    }

private:
    enum class UbFaultState
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

    /**
     * @brief 恢复 UB 特性到默认全可用，并重启节点。
     */
    static void DoRestoreUbFeature()
    {
        IT_LOG_INFO << Name() << ": restoring ub_feature to default";
        for (const auto& id : cluster_->GetNodeIds()) {
            cluster_->GetNode(id).RestoreUbFeature();
        }
        RestartAllNodes();
        state_ = UbFaultState::kNormal;
    }

    /**
     * @brief 重启集群所有节点并等待选举收敛。
     */
    static void RestartAllNodes()
    {
        for (const auto& id : cluster_->GetNodeIds()) {
            auto ret = cluster_->RestartNode(id, true, 30000);
            ASSERT_IT_OK(ret);
        }
    }

    static std::unique_ptr<ItCluster> cluster_;
    static std::string workDir_;
    static UbFaultState state_;
    static UbFaultDims currentDims_;
};

std::unique_ptr<ItCluster> Tongsuan1dTwoNodesUbFeatureFaultScenario::cluster_;
std::string Tongsuan1dTwoNodesUbFeatureFaultScenario::workDir_;
Tongsuan1dTwoNodesUbFeatureFaultScenario::UbFaultState Tongsuan1dTwoNodesUbFeatureFaultScenario::state_ =
    Tongsuan1dTwoNodesUbFeatureFaultScenario::UbFaultState::kNormal;
Tongsuan1dTwoNodesUbFeatureFaultScenario::UbFaultDims Tongsuan1dTwoNodesUbFeatureFaultScenario::currentDims_;

} // namespace ubse::it::infra

#endif // TONGSUAN_1D_TWO_NODES_UB_FEATURE_FAULT_SCENARIO_H
