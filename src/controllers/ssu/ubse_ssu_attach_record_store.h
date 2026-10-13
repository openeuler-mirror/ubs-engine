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

#ifndef UBSE_SSU_ATTACH_RECORD_STORE_H
#define UBSE_SSU_ATTACH_RECORD_STORE_H

#include <atomic>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include "ubse_ssu_attach_record.h"

namespace ubse::ssu::persist {

// 本节点挂载记录持久化存储（单例）：内存视图 + 原子落盘。
// 记录文件是"本节点挂载了什么"的唯一事实源，master 账本是全局聚合视图，
// 两者通过恢复期重报对账收敛
class UbseSsuAttachRecordStore {
public:
    static UbseSsuAttachRecordStore &GetInstance();

    UbseSsuAttachRecordStore(const UbseSsuAttachRecordStore &) = delete;
    UbseSsuAttachRecordStore &operator=(const UbseSsuAttachRecordStore &) = delete;
    UbseSsuAttachRecordStore(UbseSsuAttachRecordStore &&) = delete;
    UbseSsuAttachRecordStore &operator=(UbseSsuAttachRecordStore &&) = delete;

    // 模块 Start 时调用；
    // 目录确保、Load 已有文件。可重复调用（重新解析路径并重载，便于测试）。
    // Load 按不可信输入处理，校验清单：
    //   0) stat 自检：属主为运行用户、mode 0600，不符则 WARN + 拒载（捕获权限漂移）
    //   1) 记录总数先 cap 再 reserve（上限 MAX_ATTACH_RECORDS，防篡改 count 内存耗尽）
    //   2) 单条记录字段级校验（nsDevPath 前缀白名单、devName 白名单、枚举范围等，
    //      见 DeserializeRecord），单条非法仅丢弃该条 + WARN
    //   3) nodeId 与本机比对，不一致（整机盘被挪用/误拷贝）则隔离拒载
    // 整体不可解析（半截 JSON/version 不符）时 rename 为 .bad.<ts> 隔离留档，从空开始
    uint32_t Init();

    // 惰性兜底初始化：已初始化成功则为no-op（不重载，保护内存视图）；
    // 未初始化（恢复事件早于模块Start）或上次Init失败（权限漂移/文件损坏已修复）时执行Init。
    // 供恢复流程入口调用，业务路径由模块Start先行Init保证
    uint32_t EnsureInited();

    // 读取全部记录（恢复用）。文件不存在视为空
    std::vector<UbseSsuAttachRecord> GetAll();

    // 读取单条记录；不存在返回std::nullopt（恢复流程锁内重取用，防快照过期）
    std::optional<UbseSsuAttachRecord> Get(const std::string &name);

    // Upsert：attach 入口写 PENDING，成功后转 ATTACHED；全量序列化 + 原子写。
    // 落盘失败返回错误但不回滚内存视图（挂载是主业务，记录是可靠性增强）
    uint32_t Upsert(const UbseSsuAttachRecord &record);

    // Remove：detach/free 成功后删除记录并落盘；记录不存在返回成功（幂等）
    uint32_t Remove(const std::string &name);

private:
    UbseSsuAttachRecordStore() = default;
    ~UbseSsuAttachRecordStore() = default;

    // 持有锁调用：加载记录文件
    uint32_t LoadLocked();

    // 持有锁调用：全量序列化 + 原子写（tmp + write + fsync文件 + fsync目录 + rename）
    uint32_t PersistLocked();

    // 持有锁调用：将损坏文件 rename 为 .bad.<ts> 隔离留档
    void IsolateBadFileLocked(const std::string &reason);

    mutable std::mutex mtx_;                             // 保护 records_ + 文件写
    std::map<std::string, UbseSsuAttachRecord> records_; // key: name
    std::string filePath_;
    std::atomic<bool> inited_{false};                    // Init成功置位；失败保持false允许恢复流程重试
};

} // namespace ubse::ssu::persist

#endif // UBSE_SSU_ATTACH_RECORD_STORE_H
