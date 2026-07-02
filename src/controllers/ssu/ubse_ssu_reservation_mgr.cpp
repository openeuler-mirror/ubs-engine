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
#include "ubse_ssu_reservation_mgr.h"

#include <mutex>

namespace ubse::ssu::service {
// 添加预扣除容量, 用于创建ns成功时记录预扣除容量
void UbseSsuReservationMgr::AddReserveSpace(const std::vector<std::pair<std::string, uint64_t>> &eidBytesList)
{
    std::lock_guard<std::mutex> lock(mtx_);
    for (const auto &[eid, bytes] : eidBytesList) {
        reservedBytes_[eid] += bytes;
    }
}

// 释放预扣除容量, 用于创建ns失败时回滚预扣除容量
void UbseSsuReservationMgr::ReleaseReservation(const std::vector<std::pair<std::string, uint64_t>> &eidBytesList)
{
    std::lock_guard<std::mutex> lock(mtx_);
    for (const auto &[eid, bytes] : eidBytesList) {
        auto it = reservedBytes_.find(eid);
        if (it != reservedBytes_.end()) {
            if (bytes >= it->second) {
                reservedBytes_.erase(it);
            } else {
                it->second -= bytes;
            }
        }
    }
}

// 添加预释放容量, 用于删除ns成功时记录预释放容量
void UbseSsuReservationMgr::AddReleasedSpace(const std::vector<std::pair<std::string, uint64_t>> &eidBytesList)
{
    std::lock_guard<std::mutex> lock(mtx_);
    for (const auto &[eid, bytes] : eidBytesList) {
        releasedBytes_[eid] += bytes;
    }
}

// 获取指定eid设备的预扣除和预释放的调整量，正值表示净预扣除，负值表示净预释放
int64_t UbseSsuReservationMgr::GetPendingAdjustment(const std::string &eid) const
{
    std::lock_guard<std::mutex> lock(mtx_);
    int64_t adjustment = 0;
    auto resIt = reservedBytes_.find(eid);
    if (resIt != reservedBytes_.end()) {
        adjustment += static_cast<int64_t>(resIt->second);
    }
    auto relIt = releasedBytes_.find(eid);
    if (relIt != releasedBytes_.end()) {
        adjustment -= static_cast<int64_t>(relIt->second);
    }
    return adjustment;
}

// collector更新时调用，清空预扣除和预释放所有记录（collector更新后可用容量已最新）
void UbseSsuReservationMgr::Clear()
{
    std::lock_guard<std::mutex> lock(mtx_);
    reservedBytes_.clear();
    releasedBytes_.clear();
}

} // namespace ubse::ssu::service
