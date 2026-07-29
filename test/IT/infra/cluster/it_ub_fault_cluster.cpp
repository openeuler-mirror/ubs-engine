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

#include "it_ub_fault_cluster.h"

#include "it_assertion.h"

namespace ubse::it::infra {

void UbFaultCluster::EnsureFaultInjected(bool borrowDisabled, bool shareNcDisabled, bool shareCcDisabled)
{
    const UbFaultDims target{borrowDisabled, shareNcDisabled, shareCcDisabled};
    if (state_ == State::kFaultInjected && currentDims_ == target) {
        IT_LOG_INFO << "UbFaultCluster: already in same fault state, reuse (no restart)";
        return;
    }
    IT_LOG_INFO << "UbFaultCluster: injecting fault (borrow=" << borrowDisabled << ", shareNc=" << shareNcDisabled
                << ", shareCc=" << shareCcDisabled << ")";
    for (const auto& id : cluster_.GetNodeIds()) {
        cluster_.GetNode(id).SetUbFeatureFault(borrowDisabled, shareNcDisabled, shareCcDisabled);
    }
    RestartAll();
    currentDims_ = target;
    state_ = State::kFaultInjected;
}

void UbFaultCluster::EnsureNormal()
{
    if (state_ == State::kNormal) {
        return;
    }
    DoRestore();
}

void UbFaultCluster::DoRestore()
{
    IT_LOG_INFO << "UbFaultCluster: restoring ub_feature to default";
    for (const auto& id : cluster_.GetNodeIds()) {
        cluster_.GetNode(id).RestoreUbFeature();
    }
    RestartAll();
    state_ = State::kNormal;
}

void UbFaultCluster::RestartAll()
{
    for (const auto& id : cluster_.GetNodeIds()) {
        auto ret = cluster_.RestartNode(id, true, 30000);
        ASSERT_IT_OK(ret);
    }
}

} // namespace ubse::it::infra
