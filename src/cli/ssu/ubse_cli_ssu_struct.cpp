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

#include "ubse_cli_ssu_struct.h"

#include <type_traits>
#include <utility>

namespace ubse::cli::reg {
namespace {
// allocation 列表反序列化上限，复用 ONCE_LIMIT_LEN。
constexpr size_t SSU_CLI_MAX_ALLOCATIONS = static_cast<size_t>(ubse::serial::ONCE_LIMIT_LEN);
// 单次 allocation 内 namespace 列表反序列化上限，复用 SSU_CLI_MAX_NS_NUM。
constexpr size_t SSU_CLI_MAX_NAMESPACES_PER_ALLOCATION = static_cast<size_t>(SSU_CLI_MAX_NS_NUM);

// 枚举以 uint32 上线：与枚举底层类型（uint8_t）解耦，稳定线报文布局，
// 即便 UbseSsu* 枚举底层类型调整也不影响已部署端点的兼容性。
template <typename Enum>
void SerializeEnumAsUint32(ubse::serial::UbseSerialization &stream, Enum value)
{
    static_assert(std::is_enum_v<Enum>, "SerializeEnumAsUint32 requires an enumeration type");
    static_assert(sizeof(std::underlying_type_t<Enum>) <= sizeof(uint32_t),
                  "Enum underlying type must fit in uint32_t to keep wire format stable");
    uint32_t raw = static_cast<uint32_t>(value);
    stream << raw;
}

bool IsValidLbaFormatRaw(uint32_t raw)
{
    return raw == static_cast<uint32_t>(UbseSsuLBAFormat::LBA_FORMAT_512) ||
           raw == static_cast<uint32_t>(UbseSsuLBAFormat::LBA_FORMAT_4K);
}

bool IsValidAllocStrategyRaw(uint32_t raw)
{
    return raw == static_cast<uint32_t>(UbseSsuAllocStrategy::STRIPED) ||
           raw == static_cast<uint32_t>(UbseSsuAllocStrategy::LINEAR);
}

// 反序列化枚举：先校验 uint32 原始值，避免底层窄类型枚举转换时截断非法值；
// 仅在流状态正常且取值合法时回写，避免读失败后留下被部分覆盖的脏值。
template <typename Enum, typename IsValidRaw>
bool DeserializeEnumFromUint32(ubse::serial::UbseDeSerialization &stream, Enum &value, IsValidRaw isValidRaw)
{
    static_assert(std::is_enum_v<Enum>, "DeserializeEnumFromUint32 requires an enumeration type");
    static_assert(sizeof(std::underlying_type_t<Enum>) <= sizeof(uint32_t),
                  "Enum underlying type must fit in uint32_t to keep wire format stable");
    uint32_t raw = 0;
    stream >> raw;
    if (!stream.Check() || !isValidRaw(raw)) {
        return false;
    }
    value = static_cast<Enum>(raw);
    return true;
}

// 向量序列化：先写长度前缀（array_len_insert）再逐元素序列化，构成"长度+载荷"帧。
// 任一元素失败即整体失败，由调用方决定是否提前返回。
template <typename T>
bool SerializeVector(ubse::serial::UbseSerialization &stream, const std::vector<T> &values)
{
    stream << ubse::serial::array_len_insert(values.size());
    for (const auto &value : values) {
        if (!value.Serialize(stream)) {
            return false;
        }
    }
    return stream.Check();
}

// 向量反序列化：先读长度前缀并校验调用点给出的上限，再逐元素反序列化到临时容器。
// 长度前缀损坏、超过上限或元素反序列化失败即整体失败，且不修改原容器。
template <typename T>
bool DeserializeVector(ubse::serial::UbseDeSerialization &stream, std::vector<T> &values, size_t maxCount)
{
    size_t size = 0;
    stream >> ubse::serial::array_len_capture(size);
    if (!stream.Check() || size > maxCount) {
        return false;
    }
    std::vector<T> decoded;
    decoded.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        T value;
        if (!value.Deserialize(stream)) {
            return false;
        }
        decoded.emplace_back(std::move(value));
    }
    values = std::move(decoded);
    return stream.Check();
}

// identityInfo 为服务层裸结构体（无 Serialize/Deserialize），CLI 线报文侧手动编解码其两个子字段，
// 字段顺序 userName → uid 与 UbseSsuAllocIdentityInfo 声明顺序一致。
void SerializeIdentityInfo(ubse::serial::UbseSerialization &stream, const UbseSsuAllocIdentityInfo &identity)
{
    stream << identity.userName << identity.uid;
}

bool DeserializeIdentityInfo(ubse::serial::UbseDeSerialization &stream, UbseSsuAllocIdentityInfo &identity)
{
    stream >> identity.userName >> identity.uid;
    return stream.Check();
}
} // namespace

// 各结构的 Serialize/Deserialize 成对实现且字段顺序严格一致：增删字段需同步修改两端，
// 否则线报文错位。返回 stream.Check() 以将底层流错误上抛为整体失败。

bool UbseCliSsuAllocSummaryReq::Serialize(ubse::serial::UbseSerialization &stream) const
{
    SerializeIdentityInfo(stream, identityInfo);
    return stream.Check();
}

bool UbseCliSsuAllocSummaryReq::Deserialize(ubse::serial::UbseDeSerialization &stream)
{
    return DeserializeIdentityInfo(stream, identityInfo);
}

bool UbseCliSsuAllocDetailReq::Serialize(ubse::serial::UbseSerialization &stream) const
{
    stream << name;
    SerializeIdentityInfo(stream, identityInfo);
    return stream.Check();
}

bool UbseCliSsuAllocDetailReq::Deserialize(ubse::serial::UbseDeSerialization &stream)
{
    stream >> name;
    return DeserializeIdentityInfo(stream, identityInfo) && stream.Check();
}

bool UbseCliSsuAllocCreateReq::Serialize(ubse::serial::UbseSerialization &stream) const
{
    stream << name << nsSize << nsNum;
    SerializeEnumAsUint32(stream, lbaFormat);
    SerializeEnumAsUint32(stream, strategy);
    SerializeIdentityInfo(stream, identityInfo);
    stream << tenant;
    return stream.Check();
}

bool UbseCliSsuAllocCreateReq::Deserialize(ubse::serial::UbseDeSerialization &stream)
{
    stream >> name >> nsSize >> nsNum;
    if (!DeserializeEnumFromUint32(stream, lbaFormat, IsValidLbaFormatRaw) ||
        !DeserializeEnumFromUint32(stream, strategy, IsValidAllocStrategyRaw)) {
        return false;
    }
    if (!DeserializeIdentityInfo(stream, identityInfo)) {
        return false;
    }
    stream >> tenant;
    return stream.Check();
}

// NameSpaceInfo 字段顺序对齐服务层 UbseSsuNameSpaceInfo 声明顺序：
// tgtEid → tgtNqn → nsUuid → namespaceId → nsDevPath → nsSize → lbaFormat。
bool UbseCliSsuNameSpaceInfo::Serialize(ubse::serial::UbseSerialization &stream) const
{
    stream << tgtEid << tgtNqn << nsUuid << namespaceId << nsDevPath << nsSize;
    SerializeEnumAsUint32(stream, lbaFormat);
    return stream.Check();
}

bool UbseCliSsuNameSpaceInfo::Deserialize(ubse::serial::UbseDeSerialization &stream)
{
    stream >> tgtEid >> tgtNqn >> nsUuid >> namespaceId >> nsDevPath >> nsSize;
    return DeserializeEnumFromUint32(stream, lbaFormat, IsValidLbaFormatRaw) && stream.Check();
}

bool UbseCliSsuAllocResult::Serialize(ubse::serial::UbseSerialization &stream) const
{
    stream << name;
    SerializeEnumAsUint32(stream, strategy);
    if (!SerializeVector(stream, nameSpaceList)) {
        return false;
    }
    return stream.Check();
}

bool UbseCliSsuAllocResult::Deserialize(ubse::serial::UbseDeSerialization &stream)
{
    stream >> name;
    if (!DeserializeEnumFromUint32(stream, strategy, IsValidAllocStrategyRaw)) {
        return false;
    }
    if (!DeserializeVector(stream, nameSpaceList, SSU_CLI_MAX_NAMESPACES_PER_ALLOCATION)) {
        return false;
    }
    return stream.Check();
}

bool UbseCliSsuAllocListRsp::Serialize(ubse::serial::UbseSerialization &stream) const
{
    return SerializeVector(stream, allocations);
}

bool UbseCliSsuAllocListRsp::Deserialize(ubse::serial::UbseDeSerialization &stream)
{
    return DeserializeVector(stream, allocations, SSU_CLI_MAX_ALLOCATIONS);
}
} // namespace ubse::cli::reg
