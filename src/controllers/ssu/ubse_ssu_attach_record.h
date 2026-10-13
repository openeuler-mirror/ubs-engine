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

#ifndef UBSE_SSU_ATTACH_RECORD_H
#define UBSE_SSU_ATTACH_RECORD_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "message/ubse_ssu_attach_detach_verify_msg.h"
#include "plugin_services/ssu/ubse_ssu_service.h"
#include "rapidjson/document.h"
#include "ubse_ssu_def.h"

namespace ubse::ssu::persist {

using ubse::adapter_plugins::ssu::def::UbseCreateBlockDeviceOptions;
using ubse::plugin::service::ssu::UbseSsuAllocIdentityInfo;
using ubse::plugin::service::ssu::UbseSsuAllocStrategy;
using ubse::plugin::service::ssu::UbseSsuNameSpaceInfo;
using ubse::ssu::message::UbseSsuNsVerifyInfo;

// 记录状态：attach 入口先写 PENDING，本地挂载成功后转 ATTACHED；
// 崩溃窗口内残留的 PENDING 由恢复期探测收敛（已挂则补 ATTACHED，未挂则删记录）；
// 重挂连续失败达阈值转 SUSPECT 并告警（不停止重挂，对账定时器持续重试直到挂上），
// 探测命中/重挂成功后收敛回 ATTACHED
enum class UbseSsuAttachRecordState : uint8_t {
    PENDING = 0,  // 本地挂载执行中
    ATTACHED = 1, // 本地挂载成功
    SUSPECT = 2,  // 重挂连续失败疑似故障
};

// 本节点挂载记录：本节点挂载了什么的唯一事实源，持久化为 attach_records.json。
// 字段自足：nsList + nsVerifyList + nqn 覆盖 AgentAttachNs 回放所需全部输入，
// 重挂不依赖 verify RPC（账本可能尚未恢复，且 alreadyAttached 分支会短路）
struct UbseSsuAttachRecord {
    std::string name;
    UbseSsuAllocStrategy strategy{UbseSsuAllocStrategy::NORMAL};
    UbseSsuAttachRecordState recordState{UbseSsuAttachRecordState::PENDING};
    std::string devName;      // 聚合块设备名（NORMAL 为空）
    std::string nqn;          // attach 时实际使用的 nqn（ResolveNqn 后的最终值）
    UbseSsuAllocIdentityInfo identity; // uid + userName，重挂时用于 NS customData 身份匹配
    std::optional<UbseCreateBlockDeviceOptions> blockDeviceOptions; // NORMAL 无聚合设备
    std::vector<UbseSsuNameSpaceInfo> nsList;      // 命名空间列表
    std::vector<UbseSsuNsVerifyInfo> nsVerifyList; // 与 nsList 一一对应，AgentAttachNs 回放所需
    int64_t attachedAt{0};    // 记录落盘时间戳（秒）
};

// 单条记录序列化为 JSON 对象（文件级 version/nodeId 外壳由 AttachRecordStore 负责）
void SerializeRecord(const UbseSsuAttachRecord &record, rapidjson::Value &value,
                     rapidjson::Document::AllocatorType &allocator);

// 从 JSON 对象反序列化并做字段级校验
// 单条非法返回 false，由调用方仅丢弃该条；整体外壳（version/nodeId/count cap）由 store 校验
bool DeserializeRecord(const rapidjson::Value &value, UbseSsuAttachRecord &record);

} // namespace ubse::ssu::persist

#endif // UBSE_SSU_ATTACH_RECORD_H
