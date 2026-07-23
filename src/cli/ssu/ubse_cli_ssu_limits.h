/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_CLI_SSU_LIMITS_H
#define UBSE_CLI_SSU_LIMITS_H

#include <cstddef>
#include <cstdint>

#include "adapter_plugins/ssu/ubse_ssu_def.h"
#include "plugin_services/ssu/ubse_ssu_service.h"

namespace ubse::cli::reg {
// CLI 输入契约限制。用户可见的校验范围和错误提示统一以此处定义为准。
constexpr uint64_t SSU_CLI_BYTES_PER_GIB = 1024ULL * 1024ULL * 1024ULL;
constexpr std::size_t SSU_CLI_MIN_SIZE_ARG_LENGTH = 2; // 至少为“<数字>G”。
constexpr uint32_t SSU_CLI_MAX_NAME_LENGTH = ubse::adapter_plugins::ssu::def::UBSE_SSU_MAX_NAME_LENGTH - 1;
constexpr uint32_t SSU_CLI_MAX_HOST_NQN_LENGTH = 68;
constexpr uint32_t SSU_CLI_MAX_SRC_EID_LENGTH = 16;
constexpr uint32_t SSU_CLI_MAX_DEV_NAME_LENGTH = 32;
constexpr uint32_t SSU_CLI_DEFAULT_NS_NUM = 1;
constexpr uint32_t SSU_CLI_MAX_NS_NUM = ubse::adapter_plugins::ssu::def::UBSE_SSU_MAX_HOST_NUM;
constexpr uint64_t SSU_CLI_MIN_SIZE_BYTES = SSU_CLI_BYTES_PER_GIB;
inline constexpr char SSU_CLI_DEFAULT_HOST_NQN[] = "";

// CLI 线协议契约限制。相关值必须与 SSU IPC 消息序列化约束保持一致。
constexpr uint32_t SSU_CLI_WIRE_MAX_NAME_LENGTH = SSU_CLI_MAX_NAME_LENGTH;
constexpr uint32_t SSU_CLI_WIRE_MAX_NQN_LENGTH = SSU_CLI_MAX_HOST_NQN_LENGTH + 1;
constexpr uint32_t SSU_CLI_WIRE_MAX_DEV_NAME_LENGTH = SSU_CLI_MAX_DEV_NAME_LENGTH + 1;
constexpr uint32_t SSU_CLI_WIRE_MAX_DEV_PATH_LENGTH = 63;
constexpr uint32_t SSU_CLI_WIRE_MAX_TENANT_LENGTH = 17;
constexpr uint32_t SSU_CLI_WIRE_MAX_EID_LENGTH = SSU_CLI_MAX_SRC_EID_LENGTH + 1;
constexpr uint32_t SSU_CLI_WIRE_MAX_UUID_LENGTH = 37;
constexpr uint32_t SSU_CLI_MAX_NAMESPACES = SSU_CLI_MAX_NS_NUM;
constexpr uint32_t SSU_CLI_MAX_ALLOWED_HOST_NQNS = ubse::adapter_plugins::ssu::def::UBSE_SSU_MAX_HOST_NUM;

// 防御性限制：避免不可信响应通过伪造超大列表长度触发巨量容器预分配。
// 该值仅作为 CLI 反序列化资源保护阈值，并非 SSU 业务容量或线协议上限。
constexpr uint32_t SSU_CLI_MAX_DESERIALIZED_ALLOCATIONS = 1U << 16;

// detach 命令不对用户暴露以下选项，但请求仍需携带这些线协议默认值。
inline constexpr char SSU_CLI_DETACH_SRC_EID[] = "";
constexpr ubse::plugin::service::ssu::UbseSsuAggregationRaidLevel SSU_CLI_DETACH_LEVEL =
    ubse::plugin::service::ssu::UbseSsuAggregationRaidLevel::RAID0;
constexpr ubse::plugin::service::ssu::UbseSsuChunkSize SSU_CLI_DETACH_CHUNK_SIZE =
    ubse::plugin::service::ssu::UbseSsuChunkSize::CHUNK_SIZE_4K;
} // namespace ubse::cli::reg

#endif // UBSE_CLI_SSU_LIMITS_H
