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

#ifndef UBSE_SSU_RESERVATION_MGR_H
#define UBSE_SSU_RESERVATION_MGR_H

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace ubse::ssu::service {

// ReservationMgr 用于管理设备的预扣除和预释放容量
// 由于collector更新有时间间隔（30s），用于计算更新间隔内的设备可用容量计算
class UbseSsuReservationMgr {
public:
    UbseSsuReservationMgr() = default;
    ~UbseSsuReservationMgr() = default;

    UbseSsuReservationMgr(const UbseSsuReservationMgr &) = delete;
    UbseSsuReservationMgr &operator=(const UbseSsuReservationMgr &) = delete;

    // 添加预扣除容量
    void AddReserveSpace(const std::vector<std::pair<std::string, uint64_t>> &eidBytesList);
    // 添加预释放容量
    void AddReleasedSpace(const std::vector<std::pair<std::string, uint64_t>> &eidBytesList);

    // 释放预扣除容量，用于创建ns失败时回滚
    void ReleaseReservation(const std::vector<std::pair<std::string, uint64_t>> &eidBytesList);

    // 获取预扣除和预释放的调整量，正值表示净预扣除，负值表示净预释放
    int64_t GetPendingAdjustment(const std::string &eid) const;

    // collector更新时调用，清空预扣除和预释放所有记录
    void Clear();

private:
    // 记录eid对应设备的预扣除字节数
    std::unordered_map<std::string, uint64_t> reservedBytes_;
    // 记录eid对应设备的预释放字节数
    std::unordered_map<std::string, uint64_t> releasedBytes_;
    mutable std::mutex mtx_;
};

} // namespace ubse::ssu::service

#endif // UBSE_SSU_RESERVATION_MGR_H
