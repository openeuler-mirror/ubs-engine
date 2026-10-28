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

#ifndef PROCESS_MEM_PID_MIGRATE_LEDGER_H
#define PROCESS_MEM_PID_MIGRATE_LEDGER_H

#include <sys/types.h>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace process_mem::ledger {

// 迁移台账默认落点(tmpfs): ubse 进程/服务重启文件存活, 整机重启挂载点重建自动清空,
// 与"整机重启后 smap 内核迁移状态消失"语义自洽, 无需 fsync 与 boot_id 作废校验
inline constexpr char DEFAULT_MIGRATE_LEDGER_FILE[] = "/var/run/ubse/process_mem_migrate_ledger";

// 台账行: (pid, srcNumaId, destNumaId) -> bytes. 命令镜像语义: 每次成功的数据位移事件
// (migrate_out/R2R 同步迁移) 按事件上下文全量声明覆盖, 不依赖 smap 查询(其无源节点维度)
struct PidMigrateRow {
    pid_t pid{0};
    int32_t srcNumaId{-1};
    int32_t destNumaId{-1};
    uint64_t bytes{0};
};

// 内存行表: 外层按 pid, 内层键为 (src,dest) 编码后的 pairKey, 值为迁出字节数
using MigrateLedgerRows = std::unordered_map<int32_t, std::unordered_map<uint32_t, uint64_t>>;

class ProcessMemPidMigrateLedger {
public:
    static ProcessMemPidMigrateLedger& GetInstance();

    // 加载既有台账文件并打开落盘; 目录缺失自动创建, 文件不可读/格式损坏仅告警并跳过
    // 坏行, 极端异常(目录无法创建)时只禁用落盘, 内存记账照常可用
    uint32_t Init(const std::string& filePath = DEFAULT_MIGRATE_LEDGER_FILE);

    void UnInit();

    // 覆盖式更新 (srcNumaId/destNumaId 传 -1 表示无源/无目标); bytes == 0 删除该行;
    // 每次变更持锁整表原子落盘 (写 .tmp 后 rename), 调用成功后内存与文件一致
    void SetRow(pid_t pid, int32_t srcNumaId, int32_t destNumaId, uint64_t bytes);

    // 全量覆盖 pid: 清除该 pid 全部旧行后按 destRows 重建 (每项 destNumaId->bytes,
    // bytes 为 0 的项忽略), 一次持锁完成并单次原子落盘; 供 migrate_out 全量声明成功后的
    // 命令镜像覆盖 (当前声明不带源, srcNumaId 由调用方给定, 暂无源信息时传 0)
    void ReplacePidRows(pid_t pid, int32_t srcNumaId, const std::vector<std::pair<int32_t, uint64_t>>& destRows);

    // r2r 同步迁移完成记账: 旧 dest 行减 bytes, 不足或减至 0 时删行; 新 dest 行加 bytes;
    // 一次持锁 + 单次原子落盘, 不落中间态; old==new 或 bytes==0 时不动
    void MoveRowBytes(pid_t pid, int32_t srcNumaId, int32_t oldDestNumaId, int32_t newDestNumaId, uint64_t bytes);

    // 删除该 pid 全部行 (归还完成/进程退出/取消纳管时调用)
    void ClearPid(pid_t pid);

    // 以下查询接口无记录时返回 0
    uint64_t GetRowBytes(pid_t pid, int32_t srcNumaId, int32_t destNumaId) const;

    uint64_t GetPidSrcTotal(pid_t pid, int32_t srcNumaId) const;

    uint64_t GetPidTotal(pid_t pid) const;

    // 某本地源节点向所有远端迁出总量 (本地空闲恢复判定用)
    uint64_t GetNodeSrcTotal(int32_t srcNumaId) const;

    void GetPidRows(pid_t pid, std::vector<PidMigrateRow>& rows) const;

    size_t RowCount() const;

    // 立即把内存台账整表原子落盘; SetRow/ClearPid 已自动落盘, 一般无需显式调用
    uint32_t Persist();

private:
    uint32_t PersistLocked();

    // 供持锁调用方统计行数, 不复加锁
    size_t CountRowsLocked() const;

    MigrateLedgerRows rows_{};
    std::string filePath_{DEFAULT_MIGRATE_LEDGER_FILE};
    bool fileReady_{false};
    mutable std::mutex mutex_{};
};

} // namespace process_mem::ledger

#endif
