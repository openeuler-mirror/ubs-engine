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

// 复合键拼接：'\x1f'（ASCII单元分隔符）不会出现在name合法字符集[A-Za-z0-9_-]与
// UUID格式[0-9a-f-]中，分隔无歧义；空resourceUuid（遗留数据）与name直接拼接。
// 账本key与服务层对账/重建预检共用此定义，格式变更仅需改此一处
std::string ComposeResourceKey(const std::string &name, const std::string &resourceUuid);

// 复合键反解name/resourceUuid（与ComposeResourceKey分隔约定唯一对应，禁止裸写'\x1f'）。
// 无分隔符（遗留空uuid键）时name返回整键、uuid返回空串。
// 防抖表扩展键（如"extra"/"orphan"维度）也经ComposeResourceKey(outer, inner)拼装
std::string ExtractNameFromResourceKey(const std::string &key);
std::string ExtractUuidFromResourceKey(const std::string &key);

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

    // 复合键接口：账本key为(name, resourceUuid)。稳态（无脑裂）下同name仅一条；
    // 脑裂合并后同name多resourceUuid各自成条目，可区分、可精确释放。
    // resourceUuid为空表示遗留数据（旧版本分配），同name空uuid的NS归并为一条
    bool Put(const std::string &name, const std::string &resourceUuid,
             std::shared_ptr<const UbseSsuLedgerEntry> entry);

    // 插入或整体覆盖（原子），返回true表示覆盖了已存在的条目。
    // 仅Rebuild使用：主备回切再次触发Rebuild时，进程级单例仍驻留上一任期条目，
    // 必须以当前硬件清单刷新（state=CREATED、attachNodes空，由agent重报收敛），
    // 否则陈旧attachNodes会让对账跳过本应触发的REMOUNT。业务路径一律用只插不覆盖的Put
    bool Upsert(const std::string &name, const std::string &resourceUuid,
                std::shared_ptr<const UbseSsuLedgerEntry> entry);

    // 精确查询：(name, resourceUuid) 复合键命中
    std::shared_ptr<const UbseSsuLedgerEntry> Get(const std::string &name, const std::string &resourceUuid) const;

    // name维度查询：返回该name下全部条目（含不同resourceUuid）
    std::vector<std::shared_ptr<const UbseSsuLedgerEntry>> GetByName(const std::string &name) const;

    bool Modify(const std::string &name, const std::string &resourceUuid,
                std::function<void(UbseSsuLedgerEntry &)> modify);

    // 删除指定(name, resourceUuid)条目
    bool Remove(const std::string &name, const std::string &resourceUuid);

    // 删除name下全部条目（free未指定resourceUuid时清理全部）
    bool RemoveByName(const std::string &name);

    // ---- name维度便捷重载（供测试与name唯一场景使用；生产路径应优先复合键接口）----
    // name维度解析唯一条目：0条或多条均返回nullptr（多条为脑裂歧义，调用方须显式处理）
    std::shared_ptr<const UbseSsuLedgerEntry> Get(const std::string &name) const;
    // 以entry内携带的resourceUuid作为复合键入账
    bool Put(const std::string &name, std::shared_ptr<const UbseSsuLedgerEntry> entry);
    // name维度解析唯一条目后修改，0条或多条返回false
    bool Modify(const std::string &name, std::function<void(UbseSsuLedgerEntry &)> modify);
    // 等价RemoveByName：删除name下全部条目
    bool Remove(const std::string &name);

    std::vector<std::shared_ptr<const UbseSsuLedgerEntry>> GetAll() const;

    void Rebuild(const std::vector<UbseSsuDevInfo> &devices);

    // NODE_UP差异场景的增量收编：仅对账本缺失的硬件组入账（只插不覆盖），既有条目一律保留；
    // 收编期间置未就绪（防"未入账"被读侧误判已free/可重名），收编新条目时刷新收敛窗口起点。
    // 与全量Rebuild（主备回切专用）互补，返回新收编条目个数
    size_t AbsorbHardwareGroups(const std::vector<UbseSsuDevInfo> &devices);

    // 账本就绪标志
    // master首次硬件采集成功并完成Rebuild后置位；再次Rebuild（主备回切重选中）期间清位，
    // 重建期间收到的状态上报返回MASTER_NOT_READY（而非SPACE_NOT_FOUND），杜绝agent误判
    // space已free而错误自愈清理本地挂载，同时阻止对账/verify消费半新半旧的混合账本
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
