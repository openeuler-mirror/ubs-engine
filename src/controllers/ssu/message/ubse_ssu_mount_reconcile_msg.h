/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_SSU_MOUNT_RECONCILE_MSG_H
#define UBSE_SSU_MOUNT_RECONCILE_MSG_H

#include <cstdint>
#include <string>
#include <vector>
#include "ubse_com.h"
#include "ubse_ssu_attach_record.h"
#include "ubse_ssu_service.h"

namespace ubse::ssu::message {

using namespace ubse::plugin::service::ssu;
using UbseSsuAttachRecord = ubse::ssu::persist::UbseSsuAttachRecord;
using UbseSsuAttachRecordState = ubse::ssu::persist::UbseSsuAttachRecordState;

// 挂载记录摘要项：不含nsList明细，报文轻量
struct UbseSsuMountSummaryItem {
    std::string name;
    UbseSsuAttachRecordState recordState{UbseSsuAttachRecordState::PENDING};
    std::string devName; // 聚合块设备名（NORMAL为空）
};

// 摘要响应：agent/standby均可响应，返回本机AttachRecordStore全量摘要
struct UbseSsuMountSummaryResp {
    uint32_t errorCode{0};
    std::vector<UbseSsuMountSummaryItem> items;
};

// 平账动作：
// REMOUNT=agent侧漂移（误删记录/误detach）→ per-name锁下幂等重挂（复用RemountLocal）
// CLEANUP=账本无（已free）agent有残留 → 复用SPACE_NOT_FOUND清理语义（CleanupLocalMount+删记录）
// REPORT=上报丢失 → agent经SendStatusUpdate补报，master按幂等语义收敛
enum class UbseSsuReconcileAction : uint8_t {
    REMOUNT = 0,
    CLEANUP = 1,
    REPORT = 2,
};

// 平账指令项：首发只带name/action（轻量）；agent记录缺失回NEED_FULL_INFO后，
// master以账本allocResult+collector缓存补齐完整记录（hasRecord=true）重发
struct UbseSsuReconcileItem {
    std::string name;
    UbseSsuReconcileAction action{UbseSsuReconcileAction::REMOUNT};
    bool hasRecord{false};        // true时record携带master补齐的完整记录
    UbseSsuAttachRecord record{}; // hasRecord=false时为空壳
    uint32_t result{0};           // agent执行结果（响应方向有效）：UBSE_OK=已受理/执行成功
};

// 平账请求/响应：master按节点批量下发，agent逐项执行，结果经sync resp逐项返回
// （NEED_FULL_INFO必须同步返回，master据此补齐重发）
struct UbseSsuReconcileReq {
    std::vector<UbseSsuReconcileItem> items;
};

struct UbseSsuReconcileResp {
    uint32_t errorCode{0};
    std::vector<UbseSsuReconcileItem> items; // 与请求一一对应，携带逐项result
};

// master→agent 挂载摘要拉取请求（空请求体，占位对称）
class UbseSsuMountSummaryReqMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuMountSummaryReqMsg() = default;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;
};

// agent→master 挂载摘要响应（经sync resp返回）
class UbseSsuMountSummaryRespMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuMountSummaryRespMsg() = default;

    explicit UbseSsuMountSummaryRespMsg(const UbseSsuMountSummaryResp &resp);
    const UbseSsuMountSummaryResp &GetMountSummaryResp() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuMountSummaryResp resp_{};
};

// master→agent 平账指令请求
class UbseSsuReconcileReqMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuReconcileReqMsg() = default;

    explicit UbseSsuReconcileReqMsg(const UbseSsuReconcileReq &req);
    const UbseSsuReconcileReq &GetReconcileReq() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuReconcileReq req_{};
};

// agent→master 平账指令响应（经sync resp返回）
class UbseSsuReconcileRespMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuReconcileRespMsg() = default;

    explicit UbseSsuReconcileRespMsg(const UbseSsuReconcileResp &resp);
    const UbseSsuReconcileResp &GetReconcileResp() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuReconcileResp resp_{};
};

} // namespace ubse::ssu::message

#endif // UBSE_SSU_MOUNT_RECONCILE_MSG_H
