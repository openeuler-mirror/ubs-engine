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

#include "ubse_ssu_obj_message.h"

#include "ubse_error.h"
#include "ubse_logger.h"

namespace ubse::ssu::ipc::message {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::plugin::service::ssu;
using namespace common::def;
UbseResult StringPack(ubse::utils::UbsePackUtil &packUtil, const std::string &str, uint32_t maxLen)
{
    if (str.length() > maxLen) {
        UBSE_LOG_ERROR << "string too long, len=" << str.length() << " maxLen=" << maxLen;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (!packUtil.UbsePackString(str, maxLen)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}
UbseResult StringUnpack(ubse::utils::UbseUnpackUtil &unpackUtil, std::string &str, uint32_t maxLen)
{
    if (!unpackUtil.UnpackString(str, maxLen)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return UBSE_OK;
}

uint32_t StringCalcSize(const std::string &str, uint32_t maxLen)
{
    auto len = str.length();
    if (len > maxLen) {
        len = maxLen;
    }
    return sizeof(uint32_t) + len;
}

UbseResult VfePack(ubse::utils::UbsePackUtil &packUtil, const UbseSsuVfe &vfe)
{
    if (!packUtil.UbsePackUint8(vfe.slotId)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (!packUtil.UbsePackUint8(vfe.chipId)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (!packUtil.UbsePackUint8(vfe.dieId)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (!packUtil.UbsePackUint16(vfe.pfeId)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (!packUtil.UbsePackUint16(vfe.vfeId)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (StringPack(packUtil, vfe.vfeGuid, MAX_GUID_LEN) != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return StringPack(packUtil, vfe.bindBusInstanceGuid, MAX_GUID_LEN);
}

UbseResult VfeUnpack(ubse::utils::UbseUnpackUtil &unpackUtil, UbseSsuVfe &vfe)
{
    if (!unpackUtil.UnpackUint8(vfe.slotId)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (!unpackUtil.UnpackUint8(vfe.chipId)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (!unpackUtil.UnpackUint8(vfe.dieId)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (!unpackUtil.UnpackUint16(vfe.pfeId)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (!unpackUtil.UnpackUint16(vfe.vfeId)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (StringUnpack(unpackUtil, vfe.vfeGuid, MAX_GUID_LEN) != UBSE_OK) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return StringUnpack(unpackUtil, vfe.bindBusInstanceGuid, MAX_GUID_LEN);
}
uint32_t VfeCalcSize(const UbseSsuVfe &vfe)
{
    return sizeof(uint8_t) * 3 + sizeof(uint16_t) * 2 + StringCalcSize(vfe.vfeGuid, MAX_GUID_LEN) +
           StringCalcSize(vfe.bindBusInstanceGuid, MAX_GUID_LEN);
}
UbseResult NameSpaceInfoPack(ubse::utils::UbsePackUtil &packUtil, const UbseSsuNameSpaceInfo &info)
{
    if (StringPack(packUtil, info.tgtEid, MAX_EID_LEN) != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (StringPack(packUtil, info.tgtNqn, MAX_NQN_LEN) != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (StringPack(packUtil, info.nsUuid, MAX_UUID_LEN) != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (!packUtil.UbsePackUint32(info.namespaceId)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (StringPack(packUtil, info.nsDevPath, MAX_DEV_PATH_LEN) != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (!packUtil.UbsePackUint64(info.nsSize)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    uint32_t lbaFormat = static_cast<uint32_t>(info.lbaFormat);
    if (!packUtil.UbsePackUint32(lbaFormat)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    uint32_t nqnCount = static_cast<uint32_t>(info.allowHostNqnList.size());
    if (!packUtil.UbsePackUint32(nqnCount)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    for (const auto &hostNqn : info.allowHostNqnList) {
        if (StringPack(packUtil, hostNqn, MAX_NQN_LEN) != UBSE_OK) {
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
    }
    return UBSE_OK;
}

UbseResult NameSpaceInfoUnpack(ubse::utils::UbseUnpackUtil &unpackUtil, UbseSsuNameSpaceInfo &info)
{
    if (StringUnpack(unpackUtil, info.tgtEid, MAX_EID_LEN) != UBSE_OK) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (StringUnpack(unpackUtil, info.tgtNqn, MAX_NQN_LEN) != UBSE_OK) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (StringUnpack(unpackUtil, info.nsUuid, MAX_UUID_LEN) != UBSE_OK) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (!unpackUtil.UnpackUint32(info.namespaceId)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (StringUnpack(unpackUtil, info.nsDevPath, MAX_DEV_PATH_LEN) != UBSE_OK) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (!unpackUtil.UnpackUint64(info.nsSize)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    uint32_t lbaFormat = 0;
    if (!unpackUtil.UnpackUint32(lbaFormat)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (!IsValidLBAFormat(lbaFormat)) {
        UBSE_LOG_ERROR << "invalid lbaFormat=" << lbaFormat;
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    info.lbaFormat = static_cast<UbseSsuLBAFormat>(lbaFormat);
        uint32_t nqnCount = 0;
    if (!unpackUtil.UnpackUint32(nqnCount)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    info.allowHostNqnList.clear();
    for (uint32_t i = 0; i < nqnCount; ++i) {
        std::string hostNqn;
        if (StringUnpack(unpackUtil, hostNqn, MAX_NQN_LEN) != UBSE_OK) {
            return UBSE_ERROR_DESERIALIZE_FAILED;
        }
        info.allowHostNqnList.emplace_back(std::move(hostNqn));
    }
    return UBSE_OK;
}

uint32_t NameSpaceInfoCalcSize(const UbseSsuNameSpaceInfo &info)
{
    uint32_t size = StringCalcSize(info.tgtEid, MAX_EID_LEN) + StringCalcSize(info.tgtNqn, MAX_NQN_LEN) +
           StringCalcSize(info.nsUuid, MAX_UUID_LEN) + sizeof(uint32_t) +
           StringCalcSize(info.nsDevPath, MAX_DEV_PATH_LEN) + sizeof(uint64_t) + sizeof(uint32_t) +
           sizeof(uint32_t); // nqnCount
    for (const auto &hostNqn : info.allowHostNqnList) {
        size += StringCalcSize(hostNqn, MAX_NQN_LEN);
    }
    return size;
}

UbseResult AllocResultPack(ubse::utils::UbsePackUtil &packUtil, const UbseSsuAllocResult &result)
{

    if (StringPack(packUtil, result.name, MAX_NAME_LEN) != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    uint8_t strategy = static_cast<uint8_t>(result.strategy);
    if (!packUtil.UbsePackUint8(strategy)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    uint32_t listSize = static_cast<uint32_t>(result.nameSpaceList.size());
    if (!packUtil.UbsePackUint32(listSize)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    for (const auto &nsInfo : result.nameSpaceList) {
        if (NameSpaceInfoPack(packUtil, nsInfo) != UBSE_OK) {
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
    }
    return UBSE_OK;
}

uint32_t AllocResultCalcSize(const UbseSsuAllocResult &result)
{
    uint32_t size = StringCalcSize(result.name, MAX_NAME_LEN) + sizeof(uint8_t) + sizeof(uint32_t);
    for (const auto &nsInfo : result.nameSpaceList) {
        size += NameSpaceInfoCalcSize(nsInfo);
    }
    return size;
}

UbseResult NsDevPathsPack(ubse::utils::UbsePackUtil &packUtil, const std::vector<std::string> &nsDevPaths)
{
    if (nsDevPaths.size() > UINT32_MAX) {
        UBSE_LOG_ERROR << "nsDevPaths size overflow, size=" << nsDevPaths.size();
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    uint32_t listSize = static_cast<uint32_t>(nsDevPaths.size());
    if (!packUtil.UbsePackUint32(listSize)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    for (const auto &nsDevPath : nsDevPaths) {
        if (StringPack(packUtil, nsDevPath, MAX_DEV_PATH_LEN) != UBSE_OK) {
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
    }
    return UBSE_OK;
}

size_t NsDevPathsCalcSize(const std::vector<std::string> &nsDevPaths)
{
    size_t total = sizeof(uint32_t); // listSize
    for (const auto &nsDevPath : nsDevPaths) {
        total += StringCalcSize(nsDevPath, MAX_DEV_PATH_LEN);
    }
    return total;
}

UbseResult ConnectInfoPack(ubse::utils::UbsePackUtil &packUtil, const UbseSsuConnectInfo &info)
{
    if (StringPack(packUtil, info.srcEid, MAX_EID_LEN) != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (StringPack(packUtil, info.tgtEid, MAX_EID_LEN) != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (StringPack(packUtil, info.tgtNqn, MAX_NQN_LEN) != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (StringPack(packUtil, info.hostNqn, MAX_NQN_LEN) != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (StringPack(packUtil, info.nsUuid, MAX_UUID_LEN) != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (!packUtil.UbsePackUint32(info.nsId)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}

uint32_t ConnectInfoCalcSize(const UbseSsuConnectInfo &info)
{
    return StringCalcSize(info.srcEid, MAX_EID_LEN) + StringCalcSize(info.tgtEid, MAX_EID_LEN) +
           StringCalcSize(info.tgtNqn, MAX_NQN_LEN) + StringCalcSize(info.hostNqn, MAX_NQN_LEN) +
           StringCalcSize(info.nsUuid, MAX_UUID_LEN) + sizeof(uint32_t);
}

UbseResult NsStatsPack(ubse::utils::UbsePackUtil &packUtil, const UbseSsuNsStats &stats)
{
    if (StringPack(packUtil, stats.nsUuid, MAX_UUID_LEN) != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (!packUtil.UbsePackUint32(stats.nsId)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (!packUtil.UbsePackUint64(stats.totalSize)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (!packUtil.UbsePackUint64(stats.usedSize)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}

uint32_t NsStatsCalcSize(const UbseSsuNsStats &stats)
{
    return StringCalcSize(stats.nsUuid, MAX_UUID_LEN) + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t);
}

UbseResult FePack(ubse::utils::UbsePackUtil &packUtil, const UbseSsuFe &fe)
{
    if (!packUtil.UbsePackUint8(fe.slotId)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (!packUtil.UbsePackUint8(fe.chipId)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (!packUtil.UbsePackUint8(fe.dieId)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (!packUtil.UbsePackUint16(fe.pfeId)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (StringPack(packUtil, fe.pfeGuid, MAX_GUID_LEN) != UBSE_OK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    uint32_t vfeCount = static_cast<uint32_t>(fe.vfeList.size());
    if (!packUtil.UbsePackUint32(vfeCount)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    for (const auto &vfe : fe.vfeList) {
        if (VfePack(packUtil, vfe) != UBSE_OK) {
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
    }
    return UBSE_OK;
}

uint32_t FeCalcSize(const UbseSsuFe &fe)
{
    uint32_t size = sizeof(uint8_t) * 3 + sizeof(uint16_t) + sizeof(uint32_t) +
                    StringCalcSize(fe.pfeGuid, MAX_GUID_LEN);
    for (const auto &vfe : fe.vfeList) {
        size += VfeCalcSize(vfe);
    }
    return size;
}

UbseResult SpaceReqUnpack(ubse::utils::UbseUnpackUtil &unpackUtil, UbseSsuSpaceReq &req)
{
    if (StringUnpack(unpackUtil, req.name, MAX_NAME_LEN) != UBSE_OK) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (StringUnpack(unpackUtil, req.nqn, MAX_NQN_LEN) != UBSE_OK) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return StringUnpack(unpackUtil, req.srcEid, MAX_EID_LEN);
}

UbseResult LinearSpaceReqUnpack(ubse::utils::UbseUnpackUtil &unpackUtil, UbseSsuLinearSpaceReq &req)
{
    if (SpaceReqUnpack(unpackUtil, req) != UBSE_OK) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return StringUnpack(unpackUtil, req.devName, MAX_DEV_NAME_LEN);
}

UbseResult StripedSpaceReqUnpack(ubse::utils::UbseUnpackUtil &unpackUtil, UbseSsuStripedSpaceReq &req)
{
    if (LinearSpaceReqUnpack(unpackUtil, req) != UBSE_OK) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    uint8_t level = 0;
    if (!unpackUtil.UnpackUint8(level)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (!IsValidAggregationRaidLevel(level)) {
        UBSE_LOG_ERROR << "invalid aggregation raid level=" << static_cast<uint32_t>(level);
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    req.level = static_cast<UbseSsuAggregationRaidLevel>(level);
    uint32_t chunkSize = 0;
    if (!unpackUtil.UnpackUint32(chunkSize)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (!IsValidChunkSize(chunkSize)) {
        UBSE_LOG_ERROR << "invalid chunkSize=" << chunkSize;
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    req.chunkSize = static_cast<UbseSsuChunkSize>(chunkSize);
    return UBSE_OK;
}

} // namespace ubse::ssu::ipc::message