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

#include "adapter_plugins/ssu/ubse_ssu_def.h"
#include "plugin_services/ssu/ubse_ssu_service.h"
#include "ubse_cli_ssu_limits.h"
#include "ubse_serial_util.h"

namespace ubse::cli::reg {
using ubse::plugin::service::ssu::UbseSsuAllocIdentityInfo;
using ubse::plugin::service::ssu::UbseSsuAllocStrategy;
using ubse::plugin::service::ssu::UbseSsuLBAFormat;

// 以下结构体为 CLI ↔ 服务端之间的线报文（wire）契约，字段顺序即序列化顺序，
// 客户端与服务端必须同步演进：增删字段或调整顺序都会破坏兼容性。
// 枚举统一以 uint32 上线（见 ubse_cli_ssu_struct.cpp 的 SerializeEnumAsUint32），
// 与底层 uint8_t 存储解耦，避免枚举底层类型变化导致线报文漂移。
// 字段以 src/include/plugin_services/ssu/ubse_ssu_service.h 为事实源，不在 CLI 侧
// 自定义服务层已移除的枚举（如旧版 UbseSsuUsingType）。

// 摘要查询请求：携带当前运行用户身份，对应 UbseSsuService::ListAllocInfo 的 identity 入参。
struct UbseCliSsuAllocSummaryReq {
    UbseSsuAllocIdentityInfo identityInfo;
    bool Serialize(ubse::serial::UbseSerialization &stream) const;
    bool Deserialize(ubse::serial::UbseDeSerialization &stream);
};

// 详情查询请求：携带 name 与当前运行用户身份，对应 UbseSsuService::GetAllocInfoByName 的入参。
struct UbseCliSsuAllocDetailReq {
    std::string name;
    UbseSsuAllocIdentityInfo identityInfo;
    bool Serialize(ubse::serial::UbseSerialization &stream) const;
    bool Deserialize(ubse::serial::UbseDeSerialization &stream);
};

// 分配创建请求：name/size 必填，其余字段在 CLI 侧带缺省值，运行态字段由 FillRuntimeUser 填充。
// 字段对齐服务层 UbseSsuAllocSpaceReq，tenant 当前为线报文字段，默认空字符串。
struct UbseCliSsuAllocCreateReq {
    std::string name;
    uint64_t nsSize = SSU_CLI_MIN_SIZE_BYTES;
    uint32_t nsNum = SSU_CLI_DEFAULT_NS_NUM;
    UbseSsuLBAFormat lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
    UbseSsuAllocStrategy strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuAllocIdentityInfo identityInfo;
    std::string tenant;
    bool Serialize(ubse::serial::UbseSerialization &stream) const;
    bool Deserialize(ubse::serial::UbseDeSerialization &stream);
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
    bool Serialize(ubse::serial::UbseSerialization &stream) const;
    bool Deserialize(ubse::serial::UbseDeSerialization &stream);
};

// 分配结果：详情查询与创建成功后共用此响应，nameSpaceList 为空时 CLI 输出 INFO 提示。
struct UbseCliSsuAllocResult {
    std::string name;
    UbseSsuAllocStrategy strategy = UbseSsuAllocStrategy::LINEAR;
    std::vector<UbseCliSsuNameSpaceInfo> nameSpaceList;
    bool Serialize(ubse::serial::UbseSerialization &stream) const;
    bool Deserialize(ubse::serial::UbseDeSerialization &stream);
};

// 摘要查询响应：allocations 为空表示系统中无分配，CLI 据此输出 INFO 提示。
struct UbseCliSsuAllocListRsp {
    std::vector<UbseCliSsuAllocResult> allocations;
    bool Serialize(ubse::serial::UbseSerialization &stream) const;
    bool Deserialize(ubse::serial::UbseDeSerialization &stream);
};
} // namespace ubse::cli::reg

#endif // UBSE_CLI_SSU_STRUCT_H
