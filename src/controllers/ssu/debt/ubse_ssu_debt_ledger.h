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

#ifndef UBSE_SSU_DEBT_LEDGER_H
#define UBSE_SSU_DEBT_LEDGER_H

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "ubse_ssu_def.h"
#include "ubse_ssu_service.h"

namespace ubse::ssu::debt {
using namespace ubse::plugin::service::ssu;
using namespace ubse::adapter_plugins::ssu::def;

// 恢复收敛窗口时长：Rebuild完成后，各agent重报（5次×2s退避）
// 尚未收敛的时间窗口。窗口内独占策略（LINEAR/STRIPED）的硬件恢复空间拒绝其他节点首个attach，
// 防止"账本CREATED+attachNodes空"窗口期的独占空间双重挂载
inline constexpr int64_t SSU_RECONCILE_WINDOW_SECONDS = 60;

struct UbseSsuLedgerEntry {
    std::string name;
    std::string devName; // 聚合块设备名称，attach成功时写入；重启暂时无法恢复，待后续可靠性设计补充本地attach持久化功能
    UbseSsuAllocSpaceReq allocReq;
    UbseSsuNsState state{UbseSsuNsState::IDLE}; // 空间级聚合状态：任一节点挂载为ATTACHED，全部卸载为CREATED
    UbseSsuAllocResult allocResult;
    // NORMAL策略：多节点共挂记录（key为节点nodeId）；LINEAR/STRIPED独占策略：attacher记录（至多1个元素），供verify强校验
    std::unordered_set<std::string> attachNodes;
    // 硬件恢复标记：Rebuild从硬件customData重建的条目为true（ExecuteAlloc新建为false）。
    // 供恢复收敛窗口守卫区分"重启前已存在的空间"与"窗口内新分配的空间"（后者不受窗口限制）
    bool restoredFromHardware{false};
};

class UbseSsuDebtLedger {
public:
    UbseSsuDebtLedger(const UbseSsuDebtLedger &) = delete;
    UbseSsuDebtLedger &operator=(const UbseSsuDebtLedger &) = delete;

    static UbseSsuDebtLedger &GetInstance();

    bool Put(const std::string &name, std::shared_ptr<const UbseSsuLedgerEntry> entry);

    std::shared_ptr<const UbseSsuLedgerEntry> Get(const std::string &name) const;

    bool Modify(const std::string &name, std::function<void(UbseSsuLedgerEntry &)> modify);

    bool Remove(const std::string &name);

    std::vector<std::shared_ptr<const UbseSsuLedgerEntry>> GetAll() const;

    void Rebuild(const std::vector<UbseSsuDevInfo> &devices);

    // 账本就绪标志
    // master首次硬件采集成功并完成Rebuild后置位；该窗口内收到的状态上报返回
    // MASTER_NOT_READY（而非SPACE_NOT_FOUND），杜绝agent误判space已free而错误自愈清理本地挂载
    bool IsReady() const;
    void SetReady(bool ready);

    // 恢复收敛窗口判定当前时间距最近一次Rebuild完成
    // 是否在SSU_RECONCILE_WINDOW_SECONDS内。窗口内硬件恢复的独占空间可能尚未收到
    // attacher重报（attachNodes为空），verify需拒绝其他节点的首个attach防双重挂载
    bool IsInReconcileWindow() const;

    // 清零rebuildTime_使恢复收敛窗口立即失效（等效从未Rebuild）：用于测试隔离
    void ResetRebuildTime();

private:
    UbseSsuDebtLedger() = default;
    ~UbseSsuDebtLedger() = default;

    mutable std::shared_mutex mtx_;
    std::unordered_map<std::string, std::shared_ptr<const UbseSsuLedgerEntry>> ledger_;
    std::atomic<bool> ready_{false};
    std::chrono::steady_clock::time_point rebuildTime_{}; // 最近一次Rebuild完成时间（零值=从未Rebuild），读写经mtx_保护
};

} // namespace ubse::ssu::debt

#endif // UBSE_SSU_DEBT_LEDGER_H
