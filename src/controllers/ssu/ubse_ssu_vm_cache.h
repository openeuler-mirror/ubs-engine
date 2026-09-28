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

#ifndef UBSE_SSU_VM_CACHE_H
#define UBSE_SSU_VM_CACHE_H

#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "plugin_services/ssu/ubse_ssu_service.h"

namespace ubse::ssu::controller {

using namespace ubse::plugin::service::ssu;

/**
 * @brief VM 模式本地只读缓存,存储 ub-device-manager 推送的分配信息与连接信息。
 *        支持并发读写(shared_mutex),分配/连接信息持久化到本地 JSON 文件以加速 VM 重启恢复。
 */
class UbseSsuVmCache {
public:
    UbseSsuVmCache() = default;
    ~UbseSsuVmCache() = default;

    UbseSsuVmCache(const UbseSsuVmCache&) = delete;
    UbseSsuVmCache& operator=(const UbseSsuVmCache&) = delete;

    // 设置缓存文件路径(默认 /var/lib/ubse/ssu_vm_cache.json)
    void SetCacheFilePath(const std::string& path);

    // --- 推送更新(写操作,unique_lock) ---
    // 更新/插入单条分配信息(按 name 覆盖)
    void UpdateAllocInfo(const UbseSsuAllocResult& allocResult);
    // 更新/插入单条连接信息(按 name 覆盖)
    void UpdateConnectInfo(const std::string& name, const std::vector<UbseSsuConnectInfo>& connectInfoList);
    // 原子替换全量同步快照,避免读者观察到清空与逐条更新之间的中间状态
    void ReplaceSnapshot(const std::vector<UbseSsuAllocResult>& allocList,
                         const std::vector<std::pair<std::string, std::vector<UbseSsuConnectInfo>>>& connectList);
    // 清空所有缓存(全量同步前调用)
    void Clear();

    // 推送入口(单条/全量)落盘前的校验:确认条目能被 LoadFromDisk 接受。
    // 取值域与 LoadFromDisk 的逐字段严格解码保持一致(name/strategy/lbaFormat),
    // 避免非法条目落盘后重启时整个缓存文件被拒绝并删除,导致已缓存分配信息全部丢失
    static bool IsAllocResultPersistable(const UbseSsuAllocResult& allocResult);

    // --- 查询(读操作,shared_lock) ---
    // 按名查询分配信息,返回 0=成功, 非0=未找到
    uint32_t GetAllocInfo(const std::string& name, UbseSsuAllocResult& result);
    // 列出所有分配信息
    uint32_t ListAllocInfo(std::vector<UbseSsuAllocResult>& result);
    // 按名查询连接信息
    uint32_t GetConnectInfo(const std::string& name, std::vector<UbseSsuConnectInfo>& list);

    // --- attach 标记(写操作) ---
    void MarkAttached(const std::string& name);
    void MarkDetached(const std::string& name);
    bool IsAttached(const std::string& name);

    // --- 持久化 ---
    // 从磁盘加载缓存(Initialize 阶段调用),文件不存在或损坏返回 false
    bool LoadFromDisk();
    // 将当前缓存快照同步落盘。单条推送由调用方投递线程池,全量同步和 Stop 直接调用。
    bool FlushToDisk();

private:
    std::string SerializeSnapshot(size_t& allocSize, size_t& connectSize);
    bool WriteCacheFileAtomically(const std::string& content);

    // 读写锁:读用 shared_lock,写用 unique_lock
    std::shared_mutex rwLock_;
    // 分配信息缓存: name → AllocResult
    std::unordered_map<std::string, UbseSsuAllocResult> allocInfoMap_;
    // 连接信息缓存: name → ConnectInfo 列表
    std::unordered_map<std::string, std::vector<UbseSsuConnectInfo>> connectInfoMap_;
    // 已 attach 集合(不持久化,VM 重启后留空)
    std::unordered_set<std::string> attachedNames_;
    // 缓存文件路径
    std::string cacheFilePath_{"/var/lib/ubse/ssu_vm_cache.json"};
    // 落盘串行化锁(与 rwLock_ 解耦)
    std::mutex flushMutex_;
};

} // namespace ubse::ssu::controller

#endif // UBSE_SSU_VM_CACHE_H
