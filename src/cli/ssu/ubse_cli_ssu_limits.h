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

#include "plugin_services/ssu/ubse_ssu_service.h"

namespace ubse::cli::reg {
// CLI 输入契约限制。用户可见的校验范围和错误提示统一以此处定义为准。
constexpr uint64_t SSU_CLI_BYTES_PER_GIB = 1024ULL * 1024ULL * 1024ULL; // 1GiB = 1024^3 bytes
constexpr std::size_t SSU_CLI_MIN_SIZE_ARG_LENGTH = 2; // 至少为“<数字>G”。
constexpr uint32_t SSU_CLI_DEFAULT_NS_NUM = 1; // 默认创建单个命名空间
constexpr uint32_t SSU_CLI_MAX_NS_NUM = 128;  // 最大命名空间数量
constexpr uint64_t SSU_CLI_MIN_SIZE_BYTES = SSU_CLI_BYTES_PER_GIB; // 默认最小创建容量为 1GiB
inline constexpr char SSU_CLI_DEFAULT_HOST_NQN[] = ""; // 默认host_nqn送空串

// CLI 侧 使用C++string, 无需考虑线协议的结尾'\0'，因此长度限制比 ssu C sdk 线协议约束少1。
constexpr uint32_t SSU_CLI_MAX_NAME_LENGTH =  48 - 1; // 请求标识最大长度, 不含结尾字符'\0'
constexpr uint32_t SSU_CLI_MAX_TENANT_LENGTH = 17 - 1; // 请求方UPI(租户隔离标识)最大长度, 不含结尾字符'\0'
constexpr uint32_t SSU_CLI_MAX_HOST_NQN_LENGTH = 69 - 1; // NVMe NQN最大长度, 不含结尾字符'\0'
constexpr uint32_t SSU_CLI_MAX_SRC_EID_LENGTH = 17 - 1; // EID最大长度, 不含结尾字符'\0'
constexpr uint32_t SSU_CLI_MAX_UUID_LENGTH = 37 - 1; // UUID标准长度, 不含结尾字符'\0'
constexpr uint32_t SSU_CLI_MAX_DEV_PATH_LENGTH = 63 - 1; // 设备路径最大长度, 不含结尾字符'\0'
constexpr uint32_t SSU_CLI_MAX_DEV_NAME_LENGTH = 33 - 1; // 聚合块设备名称最大长度, 不含结尾字符'\0'

// 防御性限制：避免不可信响应通过伪造超大列表长度触发巨量容器预分配。
// 该值仅作为 CLI 反序列化资源保护阈值，并非 SSU 业务容量或线协议上限。
constexpr uint32_t SSU_CLI_MAX_DESERIALIZED_ALLOCATIONS = 1U << 16;
constexpr uint32_t SSU_CLI_MAX_DESERIALIZED_HOST_NQNS = 1U << 16;

// detach 命令不对用户暴露以下选项，但请求仍需携带这些线协议默认值。
inline constexpr char SSU_CLI_DETACH_SRC_EID[] = "";
constexpr ubse::plugin::service::ssu::UbseSsuAggregationRaidLevel SSU_CLI_DETACH_LEVEL =
    ubse::plugin::service::ssu::UbseSsuAggregationRaidLevel::RAID0;
constexpr ubse::plugin::service::ssu::UbseSsuChunkSize SSU_CLI_DETACH_CHUNK_SIZE =
    ubse::plugin::service::ssu::UbseSsuChunkSize::CHUNK_SIZE_4K;
} // namespace ubse::cli::reg

#endif // UBSE_CLI_SSU_LIMITS_H
