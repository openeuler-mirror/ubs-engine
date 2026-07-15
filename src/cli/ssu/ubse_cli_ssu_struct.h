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
using ubse::plugin::service::ssu::UbseSsuAggregationRaidLevel;
using ubse::plugin::service::ssu::UbseSsuAllocStrategy;
using ubse::plugin::service::ssu::UbseSsuChunkSize;
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

// 普通 attach 请求：只挂载已有命名空间，不创建聚合设备。
// hostNqn/srcEid 为空表示由服务端按当前连接与默认 EID 推导。
struct UbseCliSsuAttachSpaceReq {
    std::string name;
    std::string hostNqn = SSU_CLI_DEFAULT_HOST_NQN;
    std::string srcEid;
    bool Serialize(std::vector<uint8_t> &payload) const;
};

// Linear attach 请求：在普通挂载字段之外携带用户指定的聚合设备名。
// 线报文字段顺序必须与服务端 handler 使用的请求结构保持一致。
struct UbseCliSsuAttachLinearReq {
    std::string name;
    std::string hostNqn = SSU_CLI_DEFAULT_HOST_NQN;
    std::string srcEid;
    std::string devName;
    bool Serialize(std::vector<uint8_t> &payload) const;
};

// Striped attach 请求：在线性请求字段之外，显式声明 RAID 级别与 chunk 大小。
// level 按 uint8 编码，chunkSize 按 uint32 编码。
struct UbseCliSsuAttachStripedReq {
    std::string name;
    std::string hostNqn = SSU_CLI_DEFAULT_HOST_NQN;
    std::string srcEid;
    std::string devName;
    UbseSsuAggregationRaidLevel level = UbseSsuAggregationRaidLevel::RAID0;
    UbseSsuChunkSize chunkSize = UbseSsuChunkSize::CHUNK_SIZE_4K;
    bool Serialize(std::vector<uint8_t> &payload) const;
};

// 普通 attach 返回命名空间设备路径列表。
struct UbseCliSsuAttachSpaceRsp {
    std::vector<std::string> nsDevPaths;
    bool Deserialize(const uint8_t *buffer, uint32_t length);
};

// Linear/Striped attach 按服务接口输出参数顺序返回命名空间设备路径列表和聚合设备路径。
struct UbseCliSsuAttachAggregatedRsp {
    std::vector<std::string> nsDevPaths;
    std::string devPath;
    bool Deserialize(const uint8_t *buffer, uint32_t length);
};

struct UbseCliSsuDetachSpaceReq {
    std::string name;
    std::string hostNqn = SSU_CLI_DEFAULT_HOST_NQN;
    std::string srcEid = SSU_CLI_DETACH_SRC_EID;
    bool Serialize(std::vector<uint8_t> &payload) const;
};

struct UbseCliSsuDetachLinearReq {
    std::string name;
    std::string hostNqn = SSU_CLI_DEFAULT_HOST_NQN;
    std::string srcEid = SSU_CLI_DETACH_SRC_EID;
    std::string devName;
    bool Serialize(std::vector<uint8_t> &payload) const;
};

struct UbseCliSsuDetachStripedReq {
    std::string name;
    std::string hostNqn = SSU_CLI_DEFAULT_HOST_NQN;
    std::string srcEid = SSU_CLI_DETACH_SRC_EID;
    std::string devName;
    UbseSsuAggregationRaidLevel level = SSU_CLI_DETACH_LEVEL;
    UbseSsuChunkSize chunkSize = SSU_CLI_DETACH_CHUNK_SIZE;
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
