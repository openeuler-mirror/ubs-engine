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

#include <utility>

#include "ubse_cli_ssu_limits.h"
#include "ubse_pack_util.h"

namespace ubse::cli::reg {
namespace {
using ubse::utils::UbsePackUtil;
using ubse::utils::UbseUnpackUtil;

uint32_t StringSize(const std::string &value)
{
    return static_cast<uint32_t>(sizeof(uint32_t) + value.size());
}

bool StringFits(const std::string &value, uint32_t maxLength)
{
    return value.size() <= maxLength;
}

bool PackString(UbsePackUtil &pack, const std::string &value, uint32_t maxLength)
{
    return StringFits(value, maxLength) && pack.UbsePackString(value, maxLength);
}

template <typename PackFields>
bool BuildPayload(uint32_t size, std::vector<uint8_t> &payload, PackFields packFields)
{
    std::vector<uint8_t> encoded(size);
    UbsePackUtil pack(encoded.data(), encoded.size());
    if (!packFields(pack)) {
        return false;
    }
    payload = std::move(encoded);
    return true;
}

bool IsValidLbaFormat(uint32_t raw)
{
    return raw == static_cast<uint32_t>(UbseSsuLBAFormat::LBA_FORMAT_512) ||
           raw == static_cast<uint32_t>(UbseSsuLBAFormat::LBA_FORMAT_4K);
}

bool IsValidStrategy(uint8_t raw)
{
    return raw == static_cast<uint8_t>(UbseSsuAllocStrategy::STRIPED) ||
           raw == static_cast<uint8_t>(UbseSsuAllocStrategy::LINEAR) ||
           raw == static_cast<uint8_t>(UbseSsuAllocStrategy::NORMAL);
}

bool IsValidRaidLevel(uint8_t raw)
{
    return raw == static_cast<uint8_t>(UbseSsuAggregationRaidLevel::RAID0) ||
           raw == static_cast<uint8_t>(UbseSsuAggregationRaidLevel::RAID5);
}

bool IsValidChunkSize(uint32_t raw)
{
    return raw == static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_4K) ||
           raw == static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_16K) ||
           raw == static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_32K) ||
           raw == static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_64K) ||
           raw == static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_128K) ||
           raw == static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_256K) ||
           raw == static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_512K);
}

bool PackSpaceFields(UbsePackUtil &pack, const std::string &name, const std::string &hostNqn, const std::string &srcEid)
{
    return PackString(pack, name, SSU_CLI_MAX_NAME_LENGTH) &&
           PackString(pack, hostNqn, SSU_CLI_MAX_HOST_NQN_LENGTH) &&
           PackString(pack, srcEid, SSU_CLI_MAX_SRC_EID_LENGTH);
}

bool SpaceFieldsFit(const std::string &name, const std::string &hostNqn, const std::string &srcEid)
{
    return StringFits(name, SSU_CLI_MAX_NAME_LENGTH) && StringFits(hostNqn, SSU_CLI_MAX_HOST_NQN_LENGTH) &&
           StringFits(srcEid, SSU_CLI_MAX_SRC_EID_LENGTH);
}

uint32_t SpaceFieldsSize(const std::string &name, const std::string &hostNqn, const std::string &srcEid)
{
    return StringSize(name) + StringSize(hostNqn) + StringSize(srcEid);
}

bool UnpackString(UbseUnpackUtil &unpack, std::string &value, uint32_t maxLength)
{
    return unpack.UnpackString(value, maxLength);
}

bool UnpackNsDevPaths(UbseUnpackUtil &unpack, std::vector<std::string> &value)
{
    uint32_t count = 0;
    if (!unpack.UnpackUint32(count) || count > SSU_CLI_MAX_NS_NUM) {
        return false;
    }

    std::vector<std::string> decoded;
    decoded.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        std::string path;
        if (!UnpackString(unpack, path, SSU_CLI_MAX_DEV_PATH_LENGTH)) {
            return false;
        }
        decoded.emplace_back(std::move(path));
    }
    value = std::move(decoded);
    return true;
}

bool UnpackNameSpace(UbseUnpackUtil &unpack, UbseCliSsuNameSpaceInfo &value)
{
    UbseCliSsuNameSpaceInfo decoded;
    uint32_t lbaFormat = 0;
    uint32_t hostNqnCount = 0;
    if (!UnpackString(unpack, decoded.tgtEid, SSU_CLI_MAX_SRC_EID_LENGTH) ||
        !UnpackString(unpack, decoded.tgtNqn, SSU_CLI_MAX_HOST_NQN_LENGTH) ||
        !UnpackString(unpack, decoded.nsUuid, SSU_CLI_MAX_UUID_LENGTH) ||
        !unpack.UnpackUint32(decoded.namespaceId) ||
        !UnpackString(unpack, decoded.nsDevPath, SSU_CLI_MAX_DEV_PATH_LENGTH) ||
        !unpack.UnpackUint64(decoded.nsSize) || !unpack.UnpackUint32(lbaFormat) || !IsValidLbaFormat(lbaFormat) ||
        !unpack.UnpackUint32(hostNqnCount) || hostNqnCount > SSU_CLI_MAX_DESERIALIZED_HOST_NQNS) {
        return false;
    }
    decoded.lbaFormat = static_cast<UbseSsuLBAFormat>(lbaFormat);
    decoded.allowHostNqnList.reserve(hostNqnCount);
    for (uint32_t i = 0; i < hostNqnCount; ++i) {
        std::string hostNqn;
        if (!UnpackString(unpack, hostNqn, SSU_CLI_MAX_HOST_NQN_LENGTH)) {
            return false;
        }
        decoded.allowHostNqnList.emplace_back(std::move(hostNqn));
    }
    value = std::move(decoded);
    return true;
}

bool UnpackAllocResult(UbseUnpackUtil &unpack, UbseCliSsuAllocResult &value)
{
    UbseCliSsuAllocResult decoded;
    uint8_t strategy = 0;
    uint32_t namespaceCount = 0;
    if (!UnpackString(unpack, decoded.name, SSU_CLI_MAX_NAME_LENGTH) || !unpack.UnpackUint8(strategy) ||
        !IsValidStrategy(strategy) || !unpack.UnpackUint32(namespaceCount) || namespaceCount > SSU_CLI_MAX_NS_NUM) {
        return false;
    }
    decoded.strategy = static_cast<UbseSsuAllocStrategy>(strategy);
    decoded.nameSpaceList.reserve(namespaceCount);
    for (uint32_t i = 0; i < namespaceCount; ++i) {
        UbseCliSsuNameSpaceInfo nameSpace;
        if (!UnpackNameSpace(unpack, nameSpace)) {
            return false;
        }
        decoded.nameSpaceList.emplace_back(std::move(nameSpace));
    }
    // 服务端 AllocResultPack 在 namespace 列表尾部追加 resourceUuid，此处顺序对齐。
    if (!UnpackString(unpack, decoded.resourceUuid, SSU_CLI_MAX_UUID_LENGTH)) {
        return false;
    }
    value = std::move(decoded);
    return true;
}

template <typename Decode>
bool DecodePayload(const uint8_t *buffer, uint32_t length, Decode decode)
{
    if (buffer == nullptr || length == 0) {
        return false;
    }
    UbseUnpackUtil unpack(buffer, length);
    return decode(unpack);
}
} // namespace

bool UbseCliSsuAllocDetailReq::Serialize(std::vector<uint8_t> &payload) const
{
    if (!StringFits(name, SSU_CLI_MAX_NAME_LENGTH)) {
        return false;
    }
    return BuildPayload(StringSize(name), payload,
                        [this](UbsePackUtil &pack) { return PackString(pack, name, SSU_CLI_MAX_NAME_LENGTH); });
}

bool UbseCliSsuFreeSpaceReq::Serialize(std::vector<uint8_t> &payload) const
{
    if (!StringFits(name, SSU_CLI_MAX_NAME_LENGTH) || !StringFits(resourceUuid, SSU_CLI_MAX_UUID_LENGTH)) {
        return false;
    }
    if (resourceUuid.empty()) {
        // 未指定 resourceUuid 时保持旧线报文格式（仅 name），服务端按删除 name 下全部空间处理。
        return BuildPayload(StringSize(name), payload,
                            [this](UbsePackUtil &pack) { return PackString(pack, name, SSU_CLI_MAX_NAME_LENGTH); });
    }
    const uint32_t size = StringSize(name) + StringSize(resourceUuid);
    return BuildPayload(size, payload, [this](UbsePackUtil &pack) {
        return PackString(pack, name, SSU_CLI_MAX_NAME_LENGTH) && PackString(pack, resourceUuid, SSU_CLI_MAX_UUID_LENGTH);
    });
}

bool UbseCliSsuAllocCreateReq::Serialize(std::vector<uint8_t> &payload) const
{
    const auto rawLbaFormat = static_cast<uint32_t>(lbaFormat);
    const auto rawStrategy = static_cast<uint8_t>(strategy);
    if (!StringFits(name, SSU_CLI_MAX_NAME_LENGTH) || !StringFits(tenant, SSU_CLI_MAX_TENANT_LENGTH) ||
        !IsValidLbaFormat(rawLbaFormat) || !IsValidStrategy(rawStrategy)) {
        return false;
    }
    const uint32_t size =
        StringSize(name) +
        static_cast<uint32_t>(sizeof(nsSize) + sizeof(nsNum) + sizeof(rawLbaFormat) + sizeof(rawStrategy)) +
        StringSize(tenant);
    return BuildPayload(size, payload, [this, rawLbaFormat, rawStrategy](UbsePackUtil &pack) {
        return PackString(pack, name, SSU_CLI_MAX_NAME_LENGTH) && pack.UbsePackUint64(nsSize) &&
               pack.UbsePackUint32(nsNum) && pack.UbsePackUint32(rawLbaFormat) && pack.UbsePackUint8(rawStrategy) &&
               PackString(pack, tenant, SSU_CLI_MAX_TENANT_LENGTH);
    });
}

bool UbseCliSsuAttachSpaceReq::Serialize(std::vector<uint8_t> &payload) const
{
    if (!SpaceFieldsFit(name, hostNqn, srcEid)) {
        return false;
    }
    return BuildPayload(SpaceFieldsSize(name, hostNqn, srcEid), payload,
                        [this](UbsePackUtil &pack) { return PackSpaceFields(pack, name, hostNqn, srcEid); });
}

bool UbseCliSsuAttachLinearReq::Serialize(std::vector<uint8_t> &payload) const
{
    if (!SpaceFieldsFit(name, hostNqn, srcEid) || !StringFits(devName, SSU_CLI_MAX_DEV_NAME_LENGTH)) {
        return false;
    }
    return BuildPayload(SpaceFieldsSize(name, hostNqn, srcEid) + StringSize(devName), payload,
                        [this](UbsePackUtil &pack) {
                            return PackSpaceFields(pack, name, hostNqn, srcEid) &&
                                   PackString(pack, devName, SSU_CLI_MAX_DEV_NAME_LENGTH);
                        });
}

bool UbseCliSsuAttachStripedReq::Serialize(std::vector<uint8_t> &payload) const
{
    const auto rawLevel = static_cast<uint8_t>(level);
    const auto rawChunkSize = static_cast<uint32_t>(chunkSize);
    if (!SpaceFieldsFit(name, hostNqn, srcEid) || !StringFits(devName, SSU_CLI_MAX_DEV_NAME_LENGTH) ||
        !IsValidRaidLevel(rawLevel) || !IsValidChunkSize(rawChunkSize)) {
        return false;
    }
    const uint32_t size =
        SpaceFieldsSize(name, hostNqn, srcEid) + StringSize(devName) + sizeof(rawLevel) + sizeof(rawChunkSize);
    return BuildPayload(size, payload, [this, rawLevel, rawChunkSize](UbsePackUtil &pack) {
        return PackSpaceFields(pack, name, hostNqn, srcEid) &&
               PackString(pack, devName, SSU_CLI_MAX_DEV_NAME_LENGTH) && pack.UbsePackUint8(rawLevel) &&
               pack.UbsePackUint32(rawChunkSize);
    });
}

bool UbseCliSsuAttachSpaceRsp::Deserialize(const uint8_t *buffer, uint32_t length)
{
    std::vector<std::string> decoded;
    if (!DecodePayload(buffer, length,
                       [&decoded](UbseUnpackUtil &unpack) { return UnpackNsDevPaths(unpack, decoded); })) {
        return false;
    }
    nsDevPaths = std::move(decoded);
    return true;
}

bool UbseCliSsuAttachAggregatedRsp::Deserialize(const uint8_t *buffer, uint32_t length)
{
    std::vector<std::string> decodedNsDevPaths;
    std::string decodedDevPath;
    if (!DecodePayload(buffer, length, [&decodedNsDevPaths, &decodedDevPath](UbseUnpackUtil &unpack) {
            return UnpackNsDevPaths(unpack, decodedNsDevPaths) &&
                   UnpackString(unpack, decodedDevPath, SSU_CLI_MAX_DEV_PATH_LENGTH);
        })) {
        return false;
    }
    nsDevPaths = std::move(decodedNsDevPaths);
    devPath = std::move(decodedDevPath);
    return true;
}

bool UbseCliSsuDetachSpaceReq::Serialize(std::vector<uint8_t> &payload) const
{
    if (!SpaceFieldsFit(name, hostNqn, srcEid) || !StringFits(resourceUuid, SSU_CLI_MAX_UUID_LENGTH)) {
        return false;
    }
    if (resourceUuid.empty()) {
        // 未指定 resourceUuid 时保持旧线报文格式，服务端按name解析（同名多份时拒绝）。
        return BuildPayload(SpaceFieldsSize(name, hostNqn, srcEid), payload,
                            [this](UbsePackUtil &pack) { return PackSpaceFields(pack, name, hostNqn, srcEid); });
    }
    return BuildPayload(SpaceFieldsSize(name, hostNqn, srcEid) + StringSize(resourceUuid), payload,
                        [this](UbsePackUtil &pack) {
                            return PackSpaceFields(pack, name, hostNqn, srcEid) &&
                                   PackString(pack, resourceUuid, SSU_CLI_MAX_UUID_LENGTH);
                        });
}

bool UbseCliSsuDetachLinearReq::Serialize(std::vector<uint8_t> &payload) const
{
    if (!SpaceFieldsFit(name, hostNqn, srcEid) || !StringFits(devName, SSU_CLI_MAX_DEV_NAME_LENGTH) ||
        !StringFits(resourceUuid, SSU_CLI_MAX_UUID_LENGTH)) {
        return false;
    }
    uint32_t size = SpaceFieldsSize(name, hostNqn, srcEid) + StringSize(devName);
    if (!resourceUuid.empty()) {
        // resourceUuid为尾部追加字段：非空时追加（精确卸载），空串不追加（服务端兼容旧格式）
        size += StringSize(resourceUuid);
    }
    return BuildPayload(size, payload, [this](UbsePackUtil &pack) {
        bool ret = PackSpaceFields(pack, name, hostNqn, srcEid) &&
                   PackString(pack, devName, SSU_CLI_MAX_DEV_NAME_LENGTH);
        return ret && (resourceUuid.empty() || PackString(pack, resourceUuid, SSU_CLI_MAX_UUID_LENGTH));
    });
}

bool UbseCliSsuDetachStripedReq::Serialize(std::vector<uint8_t> &payload) const
{
    const auto rawLevel = static_cast<uint8_t>(level);
    const auto rawChunkSize = static_cast<uint32_t>(chunkSize);
    // detach 不向用户暴露 level/chunkSize，不校验。
    if (!SpaceFieldsFit(name, hostNqn, srcEid) || !StringFits(devName, SSU_CLI_MAX_DEV_NAME_LENGTH) ||
        !StringFits(resourceUuid, SSU_CLI_MAX_UUID_LENGTH)) {
        return false;
    }
    uint32_t size =
        SpaceFieldsSize(name, hostNqn, srcEid) + StringSize(devName) + sizeof(rawLevel) + sizeof(rawChunkSize);
    if (!resourceUuid.empty()) {
        // resourceUuid为尾部追加字段：非空时追加（精确卸载），空串不追加（服务端兼容旧格式）
        size += StringSize(resourceUuid);
    }
    return BuildPayload(size, payload, [this, rawLevel, rawChunkSize](UbsePackUtil &pack) {
        bool ret = PackSpaceFields(pack, name, hostNqn, srcEid) &&
                   PackString(pack, devName, SSU_CLI_MAX_DEV_NAME_LENGTH) && pack.UbsePackUint8(rawLevel) &&
                   pack.UbsePackUint32(rawChunkSize);
        return ret && (resourceUuid.empty() || PackString(pack, resourceUuid, SSU_CLI_MAX_UUID_LENGTH));
    });
}

bool UbseCliSsuAllocResult::Deserialize(const uint8_t *buffer, uint32_t length)
{
    UbseCliSsuAllocResult decoded;
    if (!DecodePayload(buffer, length,
                       [&decoded](UbseUnpackUtil &unpack) { return UnpackAllocResult(unpack, decoded); })) {
        return false;
    }
    *this = std::move(decoded);
    return true;
}

bool UbseCliSsuAllocListRsp::Deserialize(const uint8_t *buffer, uint32_t length)
{
    std::vector<UbseCliSsuAllocResult> decoded;
    const bool success = DecodePayload(buffer, length, [&decoded](UbseUnpackUtil &unpack) {
        uint32_t allocationCount = 0;
        if (!unpack.UnpackUint32(allocationCount) || allocationCount > SSU_CLI_MAX_DESERIALIZED_ALLOCATIONS) {
            return false;
        }
        decoded.reserve(allocationCount);
        for (uint32_t i = 0; i < allocationCount; ++i) {
            UbseCliSsuAllocResult allocation;
            if (!UnpackAllocResult(unpack, allocation)) {
                return false;
            }
            decoded.emplace_back(std::move(allocation));
        }
        return true;
    });
    if (!success) {
        return false;
    }
    allocations = std::move(decoded);
    return true;
}

bool UbseCliSsuConnectInfoReq::Serialize(std::vector<uint8_t> &payload) const
{
    if (!StringFits(name, SSU_CLI_MAX_NAME_LENGTH)) {
        return false;
    }
    // 线报文与服务端 SsuGetConnectInfoUnpack 对齐：name + hasVfe(uint8)，CLI 固定 0 表示无 VFE 过滤
    return BuildPayload(StringSize(name) + sizeof(uint8_t), payload, [this](UbsePackUtil &pack) {
        return PackString(pack, name, SSU_CLI_MAX_NAME_LENGTH) && pack.UbsePackUint8(0);
    });
}

bool UbseCliSsuConnectInfoRsp::Deserialize(const uint8_t *buffer, uint32_t length)
{
    std::vector<UbseCliSsuConnectInfo> decoded;
    const bool success = DecodePayload(buffer, length, [&decoded](UbseUnpackUtil &unpack) {
        uint32_t infoCount = 0;
        if (!unpack.UnpackUint32(infoCount) || infoCount > SSU_CLI_MAX_DESERIALIZED_CONNECT_INFOS) {
            return false;
        }
        decoded.reserve(infoCount);
        for (uint32_t i = 0; i < infoCount; ++i) {
            UbseCliSsuConnectInfo info;
            // 字段顺序与服务端 ConnectInfoPack 一致：srcEid/tgtEid/tgtNqn/hostNqn/nsUuid + nsId
            if (!UnpackString(unpack, info.srcEid, SSU_CLI_MAX_SRC_EID_LENGTH) ||
                !UnpackString(unpack, info.tgtEid, SSU_CLI_MAX_SRC_EID_LENGTH) ||
                !UnpackString(unpack, info.tgtNqn, SSU_CLI_MAX_HOST_NQN_LENGTH) ||
                !UnpackString(unpack, info.hostNqn, SSU_CLI_MAX_HOST_NQN_LENGTH) ||
                !UnpackString(unpack, info.nsUuid, SSU_CLI_MAX_UUID_LENGTH) || !unpack.UnpackUint32(info.nsId)) {
                return false;
            }
            decoded.emplace_back(std::move(info));
        }
        return true;
    });
    if (!success) {
        return false;
    }
    connectInfoList = std::move(decoded);
    return true;
}
} // namespace ubse::cli::reg
