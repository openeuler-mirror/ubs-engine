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

#ifndef UBS_ENGINE_UBSE_SSU_OBJ_MESSAGE_H
#define UBS_ENGINE_UBSE_SSU_OBJ_MESSAGE_H

#include "plugin_services/ssu/ubse_ssu_service.h"
#include "ubse_common_def.h"
#include "ubse_pack_util.h"

namespace ubse::ssu::ipc::message {

constexpr uint32_t MAX_NAME_LEN = 48;
constexpr uint32_t MAX_NQN_LEN = 69;
constexpr uint32_t MAX_DEV_NAME_LEN = 33;
constexpr uint32_t MAX_DEV_PATH_LEN = 63;
constexpr uint32_t MAX_TENANT_LEN = 17;
constexpr uint32_t MAX_EID_LEN = 17;
constexpr uint32_t MAX_UUID_LEN = 37;
constexpr uint32_t MAX_GUID_LEN = 32;

common::def::UbseResult StringPack(ubse::utils::UbsePackUtil &packUtil, const std::string &str, uint32_t maxLen);
common::def::UbseResult StringUnpack(ubse::utils::UbseUnpackUtil &unpackUtil, std::string &str, uint32_t maxLen);
uint32_t StringCalcSize(const std::string &str, uint32_t maxLen);

common::def::UbseResult VfePack(ubse::utils::UbsePackUtil &packUtil, const plugin::service::ssu::UbseSsuVfe &vfe);
common::def::UbseResult VfeUnpack(ubse::utils::UbseUnpackUtil &unpackUtil, plugin::service::ssu::UbseSsuVfe &vfe);
uint32_t VfeCalcSize(const plugin::service::ssu::UbseSsuVfe &vfe);

common::def::UbseResult NameSpaceInfoPack(ubse::utils::UbsePackUtil &packUtil,
                                          const plugin::service::ssu::UbseSsuNameSpaceInfo &info);
common::def::UbseResult NameSpaceInfoUnpack(ubse::utils::UbseUnpackUtil &unpackUtil,
                                            plugin::service::ssu::UbseSsuNameSpaceInfo &info);
uint32_t NameSpaceInfoCalcSize(const plugin::service::ssu::UbseSsuNameSpaceInfo &info);

common::def::UbseResult AllocResultPack(ubse::utils::UbsePackUtil &packUtil,
                                        const plugin::service::ssu::UbseSsuAllocResult &result);
uint32_t AllocResultCalcSize(const plugin::service::ssu::UbseSsuAllocResult &result);

common::def::UbseResult ConnectInfoPack(ubse::utils::UbsePackUtil &packUtil,
                                        const plugin::service::ssu::UbseSsuConnectInfo &info);
uint32_t ConnectInfoCalcSize(const plugin::service::ssu::UbseSsuConnectInfo &info);

common::def::UbseResult NsStatsPack(ubse::utils::UbsePackUtil &packUtil,
                                    const plugin::service::ssu::UbseSsuNsStats &stats);
uint32_t NsStatsCalcSize(const plugin::service::ssu::UbseSsuNsStats &stats);

common::def::UbseResult FePack(ubse::utils::UbsePackUtil &packUtil, const plugin::service::ssu::UbseSsuFe &fe);
uint32_t FeCalcSize(const plugin::service::ssu::UbseSsuFe &fe);

common::def::UbseResult SpaceReqUnpack(ubse::utils::UbseUnpackUtil &unpackUtil,
                                       plugin::service::ssu::UbseSsuSpaceReq &req);

common::def::UbseResult LinearSpaceReqUnpack(ubse::utils::UbseUnpackUtil &unpackUtil,
                                             plugin::service::ssu::UbseSsuLinearSpaceReq &req);

common::def::UbseResult StripedSpaceReqUnpack(ubse::utils::UbseUnpackUtil &unpackUtil,
                                              plugin::service::ssu::UbseSsuStripedSpaceReq &req);

inline bool IsValidLBAFormat(uint32_t lbaFormat)
{
    return lbaFormat == static_cast<uint32_t>(plugin::service::ssu::UbseSsuLBAFormat::LBA_FORMAT_512) ||
           lbaFormat == static_cast<uint32_t>(plugin::service::ssu::UbseSsuLBAFormat::LBA_FORMAT_4K);
}

inline bool IsValidAllocStrategy(uint8_t strategy)
{
    return strategy == static_cast<uint8_t>(plugin::service::ssu::UbseSsuAllocStrategy::STRIPED) ||
           strategy == static_cast<uint8_t>(plugin::service::ssu::UbseSsuAllocStrategy::LINEAR);
}

inline bool IsValidAggregationRaidLevel(uint8_t level)
{
    return level == static_cast<uint8_t>(plugin::service::ssu::UbseSsuAggregationRaidLevel::RAID0) ||
           level == static_cast<uint8_t>(plugin::service::ssu::UbseSsuAggregationRaidLevel::RAID5);
}

inline bool IsValidChunkSize(uint32_t chunkSize)
{
    return chunkSize == static_cast<uint32_t>(plugin::service::ssu::UbseSsuChunkSize::CHUNK_SIZE_4K) ||
           chunkSize == static_cast<uint32_t>(plugin::service::ssu::UbseSsuChunkSize::CHUNK_SIZE_16K) ||
           chunkSize == static_cast<uint32_t>(plugin::service::ssu::UbseSsuChunkSize::CHUNK_SIZE_32K) ||
           chunkSize == static_cast<uint32_t>(plugin::service::ssu::UbseSsuChunkSize::CHUNK_SIZE_64K) ||
           chunkSize == static_cast<uint32_t>(plugin::service::ssu::UbseSsuChunkSize::CHUNK_SIZE_128K) ||
           chunkSize == static_cast<uint32_t>(plugin::service::ssu::UbseSsuChunkSize::CHUNK_SIZE_256K) ||
           chunkSize == static_cast<uint32_t>(plugin::service::ssu::UbseSsuChunkSize::CHUNK_SIZE_512K);
}

} // namespace ubse::ssu::ipc::message

#endif // UBS_ENGINE_UBSE_SSU_OBJ_MESSAGE_H