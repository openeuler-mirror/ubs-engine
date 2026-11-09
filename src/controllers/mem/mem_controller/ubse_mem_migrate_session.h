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

#ifndef UBSE_MEM_MIGRATE_SESSION_H
#define UBSE_MEM_MIGRATE_SESSION_H
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "adapter_plugins/mmi/ubse_mmi_def.h"
#include "ubs_engine_mem.h"

namespace ubse::mem::controller {
enum class UbseMemMigrateTargetKind : uint8_t
{
    NOT_IMPORT_NODE = 0,
    IMPORT_NODE = 1,
};

// 迁移会话（master 内存表，不持久化；master 重启即丢，可重新受理）
struct UbseMemShmMigrateSession {
    ubse::adapter_plugins::mmi::UbseUdsInfo owner{}; // 原共享内存创建者，终态鉴权继续使用
    std::string name{};                              // 会话唯一键
    std::string srcNodeId{};                         // 故障导出节点
    std::string dstNodeId{};                         // 目标节点
    UbseMemMigrateTargetKind targetKind{UbseMemMigrateTargetKind::NOT_IMPORT_NODE};
    ubs_mem_migrate_state_t state{UBS_MEM_MIGRATE_STATE_INIT};
    uint32_t totalMemids{0};                 // 需迁移块数(受理时从老ExportObj取)
    uint64_t totalSize{0};                   // 迁移总大小，受理时从源导出布局汇总(字节)
    std::optional<uint32_t> completedBlocks; // 无值=未启动块迁移；有值=已完成块数（终态保留）
    uint32_t failReason{0};                  // 细分失败码, 供 migrate_get
    bool cancelRequested{false};             // 取消已受理(锁内置位)
    bool rebuildRequired{false}; // 禁止新增使用/重新迁移，归还重建后解除；不阻止终态归还
};

// 会话状态是否终态（归还仅与非终态会话互斥）
bool IsMigrateTerminalState(ubs_mem_migrate_state_t state);

// 迁移会话表：name 为会话唯一键, 同 name 仅允许一个非终态会话（幂等键）
class UbseMemShmMigrateSessionManager {
public:
    static UbseMemShmMigrateSessionManager& GetInstance();

    UbseMemShmMigrateSessionManager(const UbseMemShmMigrateSessionManager&) = delete;
    UbseMemShmMigrateSessionManager& operator=(const UbseMemShmMigrateSessionManager&) = delete;

    // name 是否存在非终态会话（迁移受理幂等/跨域互斥判断）
    bool HasActiveSession(const std::string& name) const;
    void SetRebuildRequired(const std::string& name, bool required);

    // 原子创建：已有非终态或禁止重新迁移的会话时返回 {既有会话, false}，否则创建新会话。
    std::pair<std::shared_ptr<UbseMemShmMigrateSession>, bool> GetOrCreateSession(
        const std::string& name, const std::string& srcNodeId, const std::string& dstNodeId, uint32_t totalMemids,
        UbseMemMigrateTargetKind targetKind = UbseMemMigrateTargetKind::NOT_IMPORT_NODE, uint64_t totalSize = 0,
        const ubse::adapter_plugins::mmi::UbseUdsInfo& owner = {});

    // 按 name 获取会话身份；可变字段须通过锁内快照读取，不存在返回 nullptr
    std::shared_ptr<UbseMemShmMigrateSession> GetSession(const std::string& name) const;

    // 锁内读取快照及可选身份token；不存在返回false并清空token。
    bool ReadSessionSnapshot(const std::string& name, UbseMemShmMigrateSession& snapshot,
                             std::shared_ptr<UbseMemShmMigrateSession>* token = nullptr) const;

    // 取消受理：会话锁内判定状态并置 cancelRequested;
    // 仅 INIT/EXPORTING(新借出完成前)允许, 受理成功置cancelRequested; 重复取消幂等返回 0;
    // 返回 0 / UBSE_ERR_NOT_EXIST(无任何会话记录) / UBSE_ERR_MIGRATE_NOT_ALLOWED(阶段或终态不允许)
    uint32_t RequestMigrateCancel(const std::string& name);

    enum class CopyStartResult
    {
        STARTED,
        CANCELED,
        STALE
    };
    // 身份校验、取消判定及拷贝初始化在同一锁内完成。
    CopyStartResult TryStartCopy(const std::string& name, const std::shared_ptr<UbseMemShmMigrateSession>& expected);

    // 推进会话阶段并清零失败原因；终态及失败原因通过FinishSession设置。
    void UpdateSessionState(const std::string& name, ubs_mem_migrate_state_t state);

    // 串行完成进度；重复完成幂等，拒绝旧会话、越界和越序更新。
    bool MarkBlockCompleted(const std::string& name, const std::shared_ptr<UbseMemShmMigrateSession>& expected,
                            uint32_t blockIndex);

    // 终态落定：state + failReason
    void FinishSession(const std::string& name, ubs_mem_migrate_state_t state, uint32_t failReason = 0);

    // 同名资源全部释放后移除终态记录；非终态会话保留。
    void EraseTerminalSession(const std::string& name);

    // 仅测试使用：清空会话表
    void ClearSessions();

private:
    UbseMemShmMigrateSessionManager() = default;

    mutable std::mutex mutex_;
    // name -> session(会话唯一键; 同名仅 1 条: 新会话受理覆盖终态留存, 活跃/终态以 state 区分)
    std::unordered_map<std::string, std::shared_ptr<UbseMemShmMigrateSession>> sessions_{};
};
} // namespace ubse::mem::controller

#endif // UBSE_MEM_MIGRATE_SESSION_H
