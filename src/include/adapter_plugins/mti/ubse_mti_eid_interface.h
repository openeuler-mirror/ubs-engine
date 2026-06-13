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

#ifndef UBSE_URMA_UTIL_H
#define UBSE_URMA_UTIL_H

#include <cstdint>
#include <string>

namespace ubse::utils {

constexpr uint32_t BEID_PREFIX = 0x44494542;
constexpr uint32_t IPV6_FULL_FORMAT_LENGTH = 39;
constexpr uint32_t IPV6_BYTE_COUNT = 16;
constexpr size_t IPV6_SEGMENT_LENGTH = 4;

/*
 * @brief 生成 URMA 聚合设备 EID
 * @param superPodId 二层超节点 ID
 * @param nodeId 节点Id（serverIdx + 1）
 * @param fe0Id FE0 ID
 * @param fe1Id FE1 ID
 * @return 生成的 URMA 聚合设备 EID
 */
std::string GenerateUrmaDevEid(uint16_t superPodId, uint32_t nodeId, uint16_t fe0Id, uint16_t fe1Id);

/**
 * @brief 解析 EID 字符串 为128位bit字符串
 * @param baseEid 基础 EID
 * @param bitStr 128位bit字符串
 * @return 操作结果
 */
uint32_t ParseBaseEid(const std::string& baseEid, std::string& bitStr);

/**
 * @brief CLOS组网下，从EID字符串中解析CNA值
 * @param eid EID字符串
 * @param cna 解析出的CNA值
 * @return 操作结果
 */
uint32_t ParseCnaFromEid(const std::string& eid, std::string& cna);

/*
 * @brief CLOS组网下，重写目标节点 EID （仅用于 primaryEid 和 portEid）
 * @param targetNodeId 目标节点 ID
 * @param baseEid 基础 EID
 * @param result 重写后的 EID
 * @return 操作结果
 */
uint32_t OverwriteEid(uint32_t serverIdx, const std::string& baseEid, std::string& result);
} // namespace ubse::utils
#endif // UBSE_URMA_UTIL_H
