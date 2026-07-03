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
#include "ubse_ssu_collector.h"

#include <mutex>
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_ssu_adapter_interface.h"
#include "ubse_timer.h"

namespace ubse::ssu::service {

using namespace ubse::log;
using namespace ubse::adapter_plugins::ssu::def;
using namespace ubse::context;
using namespace ubse::timer;

UBSE_DEFINE_THIS_MODULE("ubse");

constexpr uint32_t DEVICE_COLLECT_INTERVAL_SECONDS = 30;
constexpr const char *DEVICE_COLLECT_TIMER_NAME = "SsuDeviceCollector";

// 启动设备收集器，注册定时器，用于定期更新设备状态
uint32_t UbseSsuCollector::Start()
{
    // 原子地完成check-and-set
    bool expected = false;
    if (!timerStarted_.compare_exchange_strong(expected, true)) {
        UBSE_LOG_INFO << "SsuDeviceCollector: timer already started";
        return UBSE_OK;
    }

    // 注册之前先采集一次设备列表，避免首次采集延迟
    auto ret = CollectDeviceList();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SsuDeviceCollector Start: CollectDeviceList failed, ret=" << FormatRetCode(ret);
        timerStarted_ = false; // 回滚状态，允许后续重试
        return ret;
    }
    ret = UbseTimerHandlerRegister(
        DEVICE_COLLECT_TIMER_NAME,
        [this]() {
            auto collectRet = CollectDeviceList();
            if (collectRet != UBSE_OK) {
                UBSE_LOG_ERROR << "SsuDeviceCollector: CollectDeviceList failed, ret=" << FormatRetCode(collectRet);
            }
            return UBSE_OK;
        },
        DEVICE_COLLECT_INTERVAL_SECONDS);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SsuDeviceCollector Start: Register timer failed, ret=" << FormatRetCode(ret);
        timerStarted_ = false; // 回滚状态，允许后续重试
        return ret;
    }
    UBSE_LOG_INFO << "SsuDeviceCollector started, interval=" << DEVICE_COLLECT_INTERVAL_SECONDS << "s";
    return UBSE_OK;
}

// 停止设备收集器，注销定时器
void UbseSsuCollector::Stop()
{
    // 仅在曾启动过时执行反注册，避免重复Stop()
    if (!timerStarted_.exchange(false)) {
        return;
    }
    UbseTimerHandlerUnregister(DEVICE_COLLECT_TIMER_NAME);
    {
        std::unique_lock<std::shared_mutex> lock(devListCacheMtx_);
        cachedDevMap_.clear();
    }
    UBSE_LOG_INFO << "SsuDeviceCollector stopped";
}

// 采集设备列表，更新缓存，作为定时任务
uint32_t UbseSsuCollector::CollectDeviceList()
{
    if (g_globalStop.load()) {
        return UBSE_OK;
    }
    std::vector<UbseSsuDevInfo> devList;
    auto ret = UbseSsuAdapterInterface::GetInstance().GetDevList(devList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SsuDeviceCollector: GetDevList failed, ret=" << ret;
        return ret;
    }
    std::unordered_map<std::string, UbseSsuDevInfoPtr> newMap;
    newMap.reserve(devList.size());
    for (auto &dev : devList) {
        newMap[dev.subSystem.eid] = std::make_shared<const UbseSsuDevInfo>(std::move(dev));
    }
    
    {
        std::unique_lock<std::shared_mutex> lock(devListCacheMtx_);
        // 当采集到空列表时，且缓存不为空时，保持缓存不变
        if (newMap.empty() && !cachedDevMap_.empty()) {
            UBSE_LOG_WARN << "SsuDeviceCollector: collected empty dev list, keep previous cache size="
                          << cachedDevMap_.size();
            return UBSE_OK;
        }
        cachedDevMap_ = std::move(newMap);
        // 仅当没有进行中的预留操作时才清空预留；否则保留现有预留，
        // 由于 pendingOps_ 的保护，即使延迟一个收集周期才 Clear，硬件usedBytes，在下次收集时已更新，延迟清除不会造成错误。
        if (pendingOps_.load(std::memory_order_acquire) == 0) {
            reservationMgr_.Clear();
        }
        UBSE_LOG_INFO << "SsuDeviceCollector: device list updated, count=" << cachedDevMap_.size();
    }
    return UBSE_OK;
}

// 预留操作开始时调用，增加pendingOps_计数
void UbseSsuCollector::OnReserveBegin()
{
    // 加共享锁与CollectDeviceList 的"检查 pendingOps_ == 0 -> reservationMgr_.Clear()"互斥，
    // 防止在检查通过后，Clear执行前插入自增，导致OnReserveBegin后仍被误清而引发超额分配。
    std::shared_lock<std::shared_mutex> lock(devListCacheMtx_);
    pendingOps_.fetch_add(1, std::memory_order_acq_rel);
}

// 预留操作结束时调用，减少pendingOps_计数
void UbseSsuCollector::OnReserveEnd()
{
    std::shared_lock<std::shared_mutex> lock(devListCacheMtx_);
    pendingOps_.fetch_sub(1, std::memory_order_acq_rel);
}

// 获取缓存的设备列表, 不包含预留容量调整后的usedBytes
// 缓存为空时返回空列表，调用方应确保 Start() 已成功完成首次采集
std::vector<UbseSsuDevInfo> UbseSsuCollector::GetCachedDevList()
{
    std::shared_lock<std::shared_mutex> lock(devListCacheMtx_);
    std::vector<UbseSsuDevInfo> result;
    result.reserve(cachedDevMap_.size());
    for (const auto &[_, devPtr] : cachedDevMap_) {
        result.push_back(*devPtr);
    }
    return result;
}

// 获取缓存的设备列表，包含预留容量调整后的usedBytes
// 缓存为空时返回空列表，调用方应确保 Start() 已成功完成首次采集
std::vector<UbseSsuDevInfo> UbseSsuCollector::GetDevListWithReservations()
{
    std::shared_lock<std::shared_mutex> lock(devListCacheMtx_);
    std::vector<UbseSsuDevInfo> result;
    result.reserve(cachedDevMap_.size());
    for (const auto &[_, devPtr] : cachedDevMap_) {
        UbseSsuDevInfo dev = *devPtr;
        int64_t adjustment = reservationMgr_.GetPendingAdjustment(dev.subSystem.eid);
        if (adjustment >= 0) {
            dev.usedBytes += static_cast<uint64_t>(adjustment);
        } else {
            uint64_t decrease = static_cast<uint64_t>(-adjustment);
            dev.usedBytes -= std::min(decrease, dev.usedBytes);
        }
        result.push_back(std::move(dev));
    }
    return result;
}

void UbseSsuCollector::AddReserveSpace(const std::vector<std::pair<std::string, uint64_t>> &eidBytesList)
{
    reservationMgr_.AddReserveSpace(eidBytesList);
}

// 释放预扣除容量，用于创建ns失败时回滚
void UbseSsuCollector::ReleaseReservation(const std::vector<std::pair<std::string, uint64_t>> &eidBytesList)
{
    reservationMgr_.ReleaseReservation(eidBytesList);
}

void UbseSsuCollector::AddReleasedSpace(const std::vector<std::pair<std::string, uint64_t>> &eidBytesList)
{
    reservationMgr_.AddReleasedSpace(eidBytesList);
}

std::unordered_map<std::string, UbseSsuDevInfoPtr> UbseSsuCollector::GetCachedDevMap()
{
    std::shared_lock<std::shared_mutex> lock(devListCacheMtx_);
    return cachedDevMap_;
}

} // namespace ubse::ssu::service
