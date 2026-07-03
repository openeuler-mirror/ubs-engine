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

#ifndef UBSE_SSU_COLLECTOR_H
#define UBSE_SSU_COLLECTOR_H

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "ubse_ssu_def.h"
#include "ubse_ssu_reservation_mgr.h"

namespace ubse::ssu::service {

using namespace ubse::adapter_plugins::ssu::def;

using UbseSsuDevInfoPtr = std::shared_ptr<const UbseSsuDevInfo>;

class UbseSsuCollector {
public:
    UbseSsuCollector() = default;
    ~UbseSsuCollector() = default;

    UbseSsuCollector(const UbseSsuCollector &) = delete;
    UbseSsuCollector &operator=(const UbseSsuCollector &) = delete;

    // 启动收集器，注册定时设备列表收集任务
    uint32_t Start();
    // 停止收集器，解注册定时设备列表收集任务
    void Stop();
    // 获取缓存的设备列表
    std::vector<UbseSsuDevInfo> GetCachedDevList();
    // 获取缓存的设备map
    std::unordered_map<std::string, UbseSsuDevInfoPtr> GetCachedDevMap();
    // 读取设备列表并叠加预留调整量，避免读取cachedDevMap_和reservationMgr_之间的并发不一致
    std::vector<UbseSsuDevInfo> GetDevListWithReservations();

    // 添加预扣除容量
    void AddReserveSpace(const std::vector<std::pair<std::string, uint64_t>> &eidBytesList);
    // 添加预释放容量
    void AddReleasedSpace(const std::vector<std::pair<std::string, uint64_t>> &eidBytesList);
    // 释放预扣除容量，用于创建ns失败时回滚
    void ReleaseReservation(const std::vector<std::pair<std::string, uint64_t>> &eidBytesList);

    // 标记预留操作开始/结束，CollectDeviceList 仅在无进行中的操作(创建ns/删除ns)时清空预留
    // 防止误清预留导致超分/不足分
    // OnReserveBegin和OnReserveEnd必须成对调用
    void OnReserveBegin();
    void OnReserveEnd();

private:
    // 收集设备列表并更新缓存，作为定时任务
    uint32_t CollectDeviceList();
    // 定时器是否已启动
    std::atomic<bool> timerStarted_{false};
    // 缓存的设备列表，用于快速查找设备信息
    std::unordered_map<std::string, UbseSsuDevInfoPtr> cachedDevMap_;
    // 设备预留管理器，用于记录预扣除及预释放容量
    UbseSsuReservationMgr reservationMgr_;
    
    // 设备列表读写锁，保护并发读取
    mutable std::shared_mutex devListCacheMtx_;
    // 进行中的操作数量，用于判断是否清空预留
    std::atomic<int32_t> pendingOps_{0};
};

} // namespace ubse::ssu::service

#endif // UBSE_SSU_COLLECTOR_H
