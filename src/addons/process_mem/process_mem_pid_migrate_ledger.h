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

#include "process_mem_pid_manager_def.h"

namespace process_mem::ledger {

// 迁移台账默认落点(tmpfs): ubse 进程/服务重启文件存活, 整机重启挂载点重建自动清空,
// 与"整机重启后 smap 内核迁移状态消失"语义自洽, 无需 fsync 与 boot_id 作废校验
inline constexpr char DEFAULT_MIGRATE_LEDGER_FILE[] = "/var/run/ubse/process_mem_migrate_ledger";

// 台账行: (pid, srcNumaId, destNumaId) -> bytes. 命令镜像语义: 每次成功的数据位移事件
// (migrate_out 分组全量声明/R2R 同步迁移) 按事件上下文全量覆盖, 不依赖 smap 查询(其无源节点维度)
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

    // 全量覆盖 pid: 清除该 pid 全部旧行后按分组声明重建 (每组 srcNumaId 下的
    // destNumaId->bytes, bytes 为 0 的项忽略), 一次持锁完成并单次原子落盘; 供 migrate_out
    // 分组全量声明成功后的命令镜像覆盖; 未知源组(src<0)沿用 0 占位键(与 r2r 行键兜底一致),
    // 占位键与真实 numa0 组撞键时累加, 避免覆盖丢量
    void ReplacePidRows(pid_t pid, const def::MigrateOutGroups& groups);

    // r2r 同步迁移完成记账: 旧 dest 行减 bytes, 不足或减至 0 时删行; 新 dest 行加 bytes;
    // 一次持锁 + 单次原子落盘, 不落中间态; old==new 或 bytes==0 时不动;
    // 未知源(旧账)行键占位 0 与债务 srcNuma 不同时, 按 (0, oldDest) 兜底, 仍找不到告警并新建
    void MoveRowBytes(pid_t pid, int32_t srcNumaId, int32_t oldDestNumaId, int32_t newDestNumaId, uint64_t bytes);

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
