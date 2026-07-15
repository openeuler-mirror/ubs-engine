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

#ifndef UBSE_CLI_SSU_STRUCT_H
#define UBSE_CLI_SSU_STRUCT_H

#include <cstdint>
#include <string>
#include <vector>

#include "plugin_services/ssu/ubse_ssu_service.h"
#include "ubse_cli_ssu_limits.h"

namespace ubse::cli::reg {
using ubse::plugin::service::ssu::UbseSsuAllocStrategy;
using ubse::plugin::service::ssu::UbseSsuLBAFormat;

// 以下结构是 CLI ↔ SSU handler 的 wire 契约。字段顺序和宽度必须与
// `Ssu*Unpack/Ssu*Pack` 保持一致；IPC body 为 UbsePackUtil 裸 payload，不带 serde 协议头。

// 详情查询请求：仅携带待查询的分配名称。
struct UbseCliSsuAllocDetailReq {
    std::string name;
    bool Serialize(std::vector<uint8_t> &payload) const;
};

// 分配创建请求：name/size 必填，其余字段在 CLI 侧带缺省值。
// 字段对齐服务层 UbseSsuAllocSpaceReq，tenant 当前为线报文字段，默认空字符串。
struct UbseCliSsuAllocCreateReq {
    std::string name;
    uint64_t nsSize = SSU_CLI_MIN_SIZE_BYTES;
    uint32_t nsNum = SSU_CLI_DEFAULT_NS_NUM;
    UbseSsuLBAFormat lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
    UbseSsuAllocStrategy strategy = UbseSsuAllocStrategy::LINEAR;
    std::string tenant;
    bool Serialize(std::vector<uint8_t> &payload) const;
};

// 单个命名空间信息：对应详情表中的一行，字段对齐服务层 UbseSsuNameSpaceInfo。
struct UbseCliSsuNameSpaceInfo {
    std::string tgtEid;
    std::string tgtNqn;
    std::string nsUuid;
    uint32_t namespaceId = 0;
    std::string nsDevPath;
    uint64_t nsSize = 0;
    UbseSsuLBAFormat lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
    std::vector<std::string> allowHostNqnList;
};

// 分配结果：详情查询与创建成功后共用此响应，nameSpaceList 为空时 CLI 输出 INFO 提示。
struct UbseCliSsuAllocResult {
    std::string name;
    UbseSsuAllocStrategy strategy = UbseSsuAllocStrategy::LINEAR;
    std::vector<UbseCliSsuNameSpaceInfo> nameSpaceList;
    bool Deserialize(const uint8_t *buffer, uint32_t length);
};

// 摘要查询响应：allocations 为空表示系统中无分配，CLI 据此输出 INFO 提示。
struct UbseCliSsuAllocListRsp {
    std::vector<UbseCliSsuAllocResult> allocations;
    bool Deserialize(const uint8_t *buffer, uint32_t length);
};
} // namespace ubse::cli::reg

#endif // UBSE_CLI_SSU_STRUCT_H
