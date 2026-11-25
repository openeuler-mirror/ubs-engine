/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBS_ENGINE_UBSE_MEM_CABINET_LEDGER_H
#define UBS_ENGINE_UBSE_MEM_CABINET_LEDGER_H

#include <chrono>
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>

#include "ubse_common_def.h"
#include "ubse_mmi_interface.h"

namespace ubse::mem::controller {
using ubse::adapter_plugins::mmi::NodeMemDebtInfo;

// A 侧跨柜并发闸门上限：进程级 permit，写死常量不做配置项，PT 后可调（设计文档 §5.1/§8.1）
constexpr uint32_t kCrossCabinetConcurrency = 2;
// 单个 T 的对账轮预算，与既有 60s 重试节奏对齐（设计文档 §7.3）
constexpr int64_t kCrossCabinetRoundBudgetMs = 60000;

 /**
 * @brief A 侧跨柜轮闸门许可（RAII）
 *        作用域=整个跨柜分支（采集+装配+修复），由 LedgerHandler 持有至本轮结束；
 *        BUSY/超时/异常路径均经析构归还，禁止按柜释放（设计文档 §5.1/§8.1）
 */
class CrossCabinetRoundPermit {
public:
    CrossCabinetRoundPermit() = default;
    ~CrossCabinetRoundPermit();
    CrossCabinetRoundPermit(const CrossCabinetRoundPermit&) = delete;
    CrossCabinetRoundPermit& operator=(const CrossCabinetRoundPermit&) = delete;

    /**
    * @brief 申请许可，已持有时直接返回 true
    * @param waitMs [in] 最长等待毫秒数（本 T 剩余轮预算）
    * @return true 已获得许可；false 等待超时，本轮所有外柜按 SKIPPED 收尾
    */
    bool Acquire(uint32_t waitMs);

    /**
    * @brief 是否已持有许可
    */
    bool Held() const
    {
        return held_;
    }

private:
    void Release();

    bool held_{false};
};

 /**
 * @brief 单个 T 的跨柜对账轮上下文：轮级 deadline + 跨柜闸门许可
 *        deadline 于进入外柜处理循环前记录，检查点位于每柜处理循环开头（设计文档 §7.3）
 */
struct CrossCabinetRoundCtx {
    std::chrono::steady_clock::time_point deadline;
    CrossCabinetRoundPermit permit;
};

 /**
 * @brief 跨柜采集编排入口（设计文档 §5.1/§7.1）
 *        提取本 T 的外柜对端 -> 闸门 -> 逐外柜发起 0x001A 聚合查询 -> 覆盖度校验 -> 装配 allDebtInfoMap
 * @param allDebtInfoMap [in,out] 账本集合，SUCCESS 条目按 B 柜节点 nodeId 写入
 * @param masterDebtInfo [in] T 的柜主侧账本视图（外柜对端提取来源之一）
 * @param agentDebtInfo [in] T 的实际上报账本（外柜对端提取来源之二）
 * @param selfGroupId [in] 本柜号（A 侧，请求报文的 requesterGroupId）
 * @param selfNodeId [in] 本节点（A 柜主）nodeId，仅用于请求报文与日志
 * @param round [in,out] 轮上下文，持有跨柜闸门许可直到本轮结束
 * @param deferredPeers [out] 本轮需遮蔽的对端 nodeId（SKIPPED/BUSY/超时/覆盖度缺失）
 */
void CollectCrossCabinetLedgers(std::unordered_map<std::string, NodeMemDebtInfo>& allDebtInfoMap,
                                const NodeMemDebtInfo& masterDebtInfo, const NodeMemDebtInfo& agentDebtInfo,
                                uint32_t selfGroupId, const std::string& selfNodeId, CrossCabinetRoundCtx& round,
                                std::set<std::string>& deferredPeers);

 /**
 * @brief 关系遮蔽：把 deferredPeers 相关条目从 T 的账本视图剔除（设计文档 §5.1）
 *        只过滤本轮内存视图——不删除、不置 DESTROYED、不写持久化；其它对端与本柜关系照常对账
 * @param masterDebtInfo [in,out] T 的柜主侧账本视图
 * @param agentDebtInfo [in,out] T 的实际上报账本
 * @param deferredPeers [in] 本轮 SKIPPED 对端集合，为空时不做任何处理
 */
void ShadowFilterDeferredRelations(NodeMemDebtInfo& masterDebtInfo, NodeMemDebtInfo& agentDebtInfo,
                                   const std::set<std::string>& deferredPeers);
} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_MEM_CABINET_LEDGER_H
