/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_mem_common_utils.h"
#include <cstring>
#include "ubse_common_def.h"
#include "ubse_logger.h"
#include "ubse_mem_def.h"
#include "ubse_security_module.h"
#include "ubse_str_util.h"

namespace ubse::mmi {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::security;
constexpr uint32_t HASH_MIN_VAL = 4;
constexpr uint32_t HASH_RANGE_SIZE = 14;
const size_t VALUE_COUNT = 2;

void CopyObmmMemDescValue(const ubse_mem_obmm_mem_desc& src, obmm_mem_desc* des, uint64_t hpa)
{
    if (des != nullptr) {
        des->addr = hpa;
        des->length = src.length;
        des->tokenid = src.tokenid;
        des->scna = src.scna;
        des->dcna = src.dcna;
        if (memcpy_s(des->seid, UBSE_EID_LENGTH, &src.seid, sizeof(src.seid)) != EOK) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "seid copy failed";
        }
        if (memcpy_s(des->deid, UBSE_EID_LENGTH, &src.deid, sizeof(src.deid)) != EOK) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "deid copy failed";
        }
    }
}

bool ParsePreOnlineEidStr(const std::string& eid, uint32_t& value)
{
    uint64_t temp;
    std::vector<std::string> vec;
    ubse::utils::Split(eid, ":", vec);
    if (vec.size() != VALUE_COUNT) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Parse eid str failed.";
        return false;
    }
    if (!RmCommonUtils::GetInstance().StrToULL(vec[1], temp, BASE_16)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Tran eid str to value failed.";
        return false;
    }
    value = static_cast<uint32_t>(temp);
    return true;
}

void CopyObmmMemDescValue(const obmm_mem_desc* src, ubse_mem_obmm_mem_desc& des)
{
    if (src != nullptr) {
        des.addr = src->addr;
        des.length = src->length;
        des.tokenid = src->tokenid;
        des.scna = src->scna;
        des.dcna = src->dcna;
        if (memcpy_s(&des.seid, sizeof(des.seid), src->seid, sizeof(des.seid)) != EOK) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "seid copy failed";
        }
        if (memcpy_s(&des.deid, sizeof(des.deid), src->deid, sizeof(des.deid)) != EOK) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "deid copy failed";
        }
    }
}

UbseResult CopyUbseMemAlgoResult(const UbseMemAlgoResult& algoResult, const std::string& name,
                                 UbseMemLocalObmmCustomMeta& customMeta, const bool isAppendNodeId)
{
    if (algoResult.exportNumaInfos.empty() || algoResult.exportNumaInfos.size() < algoResult.importNumaInfos.size() ||
        TOPOLOGY_MAX_NUMA_PER_SOCKET < algoResult.exportNumaInfos.size()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "exportNumaInfos.size=" << algoResult.exportNumaInfos.size()
                       << ", importNumaInfos.size=" << algoResult.importNumaInfos.size();
        return UBSE_ERROR_INVAL;
    }
    for (int i = 0; i < algoResult.exportNumaInfos.size(); i++) {
        customMeta.exportNumaIds[i] = algoResult.exportNumaInfos[i].numaId;
        customMeta.numaSizes[i] = algoResult.exportNumaInfos[i].size;
    }
    std::string importNodeId = "";
    for (int i = 0; i < algoResult.importNumaInfos.size(); i++) {
        customMeta.importNumaIds[i] = algoResult.importNumaInfos[i].numaId;
        customMeta.importSocket = algoResult.importNumaInfos[0].socketId;
        importNodeId = algoResult.importNumaInfos[0].nodeId;
        customMeta.chipId = algoResult.importNumaInfos[0].chipId;
        customMeta.portId = algoResult.importNumaInfos[0].portId;
    }
    customMeta.attachSocket = algoResult.attachSocketId;
    customMeta.exportSocket = algoResult.exportNumaInfos[0].socketId;
    std::string exportNodeId = algoResult.exportNumaInfos[0].nodeId;
    if (exportNodeId.empty()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "exportNodeId=" << exportNodeId << ", importNodeId=" << importNodeId;
        return UBSE_ERROR_INVAL;
    }
    auto copyName = name;
    if (isAppendNodeId) {
        copyName = GenerateExportKey(name, importNodeId);
    }
    if (strcpy_s(customMeta.name, UBSE_MEM_INTERNAL_KEY_CAPACITY, copyName.c_str()) != EOK ||
        strcpy_s(customMeta.exportNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, exportNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.importNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, importNodeId.c_str()) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO
                       << "StrCopy fail when copy requestNodeId and name to meta, name=" << customMeta.name
                       << ", requestNodeId=" << customMeta.exportNodeId << ", importNodeId=" << importNodeId;
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult RmCommonUtils::GetFileFirstLine(const std::string& path, std::string& line)
{
    std::vector<__u32> caps = {CAP_DAC_OVERRIDE};
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
    std::ifstream file(path);
    if (!file.is_open()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Open file " << path << " failed.";
        UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
        return UBSE_MMI_OPEN_FAILED;
    }
    if (!std::getline(file, line)) {
        file.close();
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get file context failed.";
        UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
        return UBSE_MMI_OPEN_FAILED;
    }
    file.close();
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
    return UBSE_OK;
}

UbseResult SetMaskFromRegionIndex(const std::vector<uint32_t>& regionNodeIndex, uint8_t (&mask)[OBMM_REGION_MASK_BYTES])
{
    for (int i = 0; i < regionNodeIndex.size(); i++) {
        if (regionNodeIndex[i] < MAX_NODE_NUM) {
            UBSE_LOG_INFO << MMI_LOG_INFO << "Region bit[" << i << "]=" << regionNodeIndex[i];
            mask[regionNodeIndex[i] / 8] |= (1u << (regionNodeIndex[i] % 8)); // 1 << bit 转为无符号操作
        } else {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Region bit[" << i << "]=" << regionNodeIndex[i];
            return UBSE_ERROR_INVAL;
        }
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "Constructed shared region bitmap, nodeCount=" << regionNodeIndex.size();
    return UBSE_OK;
}

// 映射到 [4, 17] 之间，确保唯一性,生成远端numa
int RmCommonUtils::HashStringToByte(const std::string& input)
{
    static std::hash<std::string> hasher;
    const size_t hashValue = hasher(input);
    return HASH_MIN_VAL + (hashValue % HASH_RANGE_SIZE); // 这一步很容易重复
}

void RmCommonUtils::GenerateNodeChipPortStr(const NodeId& lendNodeId, const ChipId& lendChipId, const PortId& portId,
                                            std::string& nodeChipPortStr)
{
    std::ostringstream str;
    str << lendNodeId << "-" << lendChipId << "-" << portId;
    nodeChipPortStr = str.str();
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "GenerateNodeChipPortStr=" << nodeChipPortStr;
}
namespace {
template <typename Meta>
bool HasValidName(const Meta& meta)
{
    return std::memchr(meta.name, '\0', sizeof(meta.name)) != nullptr;
}

UbseResult ConvertV1(const ObmmCustomMetaV1& source, UbseMemLocalObmmCustomMeta& target)
{
    // Copy the packed business fields separately from the aligned decoder result.
    constexpr size_t prefixLength = offsetof(ObmmCustomMetaV1, name);
    constexpr size_t oldTail = offsetof(ObmmCustomMetaV1, importNumaIds);
    constexpr size_t newTail = offsetof(UbseMemLocalObmmCustomMeta, importNumaIds);
    static_assert(prefixLength == offsetof(UbseMemLocalObmmCustomMeta, name), "Metadata prefix changed");
    constexpr size_t tailLength = offsetof(ObmmCustomMetaV1, requestSize) + sizeof(source.requestSize) - oldTail;
    static_assert(
        tailLength == offsetof(UbseMemLocalObmmCustomMeta, requestSize) + sizeof(target.requestSize) - newTail,
        "Metadata business fields changed");
    errno_t ret{};
    if ((ret = memcpy_s(&target, sizeof(target), &source, prefixLength)) != EOK ||
        (ret = memcpy_s(target.name, sizeof(target.name), source.name, sizeof(source.name))) != EOK ||
        (ret = memcpy_s(reinterpret_cast<uint8_t*>(&target) + newTail, sizeof(target) - newTail,
                        reinterpret_cast<const uint8_t*>(&source) + oldTail, tailLength)) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Copying V1 metadata into V2 layout failed, ret=" << ret;
        return UBSE_ERROR_INVAL;
    }
    target.memidCount = source.memidCount;
    for (size_t index = 0; index < OBMM_REGION_MASK_BYTES; ++index) {
        target.regionMask[index] =
            index < sizeof(source.regionMask) ? static_cast<uint8_t>(source.regionMask >> (index * 8)) : 0;
    }
    target.decoderResult = source.decoderResult;
    target.migrateInfo = 0; // V1没有迁移标记，不能从decoder尾部或填充字节读取。
    return UBSE_OK;
}
template <typename Source, typename Target>
UbseResult CopyV2Fields(const Source& source, Target& target)
{
    constexpr size_t fieldsSize = offsetof(Source, requestSize) + sizeof(source.requestSize);
    static_assert(fieldsSize == offsetof(Target, requestSize) + sizeof(target.requestSize),
                  "V2 business fields changed");
    const auto ret = memcpy_s(&target, sizeof(target), &source, fieldsSize);
    if (ret != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Copying V2 metadata fields failed, ret=" << ret;
        return UBSE_ERROR_INVAL;
    }
    target.decoderResult.marId = source.decoderResult.marId;
    target.decoderResult.hpa = source.decoderResult.hpa;
    target.decoderResult.handle = source.decoderResult.handle;
    target.decoderResult.staticHandle = source.decoderResult.staticHandle;
    target.decoderResult.state = source.decoderResult.state;
    target.migrateInfo = source.migrateInfo;
    return UBSE_OK;
}
} // namespace

UbseResult EncodeObmmCustomMeta(const UbseMemLocalObmmCustomMeta& meta, const UbMemPrivData& privData,
                                ObmmEncodedMeta& buffer, uint32_t& length)
{
    length = 0;
    if (!HasValidName(meta)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Unterminated OBMM metadata name, capacity=" << sizeof(meta.name);
        return UBSE_ERROR_INVAL;
    }
    ObmmCustomMetaV2 v2{};
    const auto convertRet = CopyV2Fields(meta, v2);
    if (convertRet != UBSE_OK) {
        return convertRet;
    }
    v2.version = OBMM_CUSTOM_META_V2;
    buffer.fill(0);
    errno_t ret{};
    if ((ret = memcpy_s(buffer.data(), buffer.size(), &privData, sizeof(privData))) != EOK ||
        (ret = memcpy_s(buffer.data() + sizeof(privData), buffer.size() - sizeof(privData), &v2, sizeof(v2))) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Copying V2 metadata into OBMM private buffer failed, ret=" << ret;
        return UBSE_ERROR_INVAL;
    }
    length = sizeof(privData) + sizeof(v2);
    return UBSE_OK;
}

UbseResult DecodeObmmCustomMeta(const uint8_t* buffer, uint64_t length, UbseMemLocalObmmCustomMeta& meta,
                                UbMemPrivData& privData)
{
    if (buffer == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Cannot decode OBMM metadata: null buffer, length=" << length;
        return UBSE_ERROR_NULLPTR;
    }
    if (length <= sizeof(UbMemPrivData) || length > OBMM_CUSTOM_META_MAX_LENGTH) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Invalid OBMM metadata length=" << length
                       << ", prefixLength=" << sizeof(UbMemPrivData) << ", maxLength=" << OBMM_CUSTOM_META_MAX_LENGTH;
        return UBSE_ERROR_INVAL;
    }
    const auto version = buffer[sizeof(UbMemPrivData)];
    UbseMemLocalObmmCustomMeta restored{};
    if (version == OBMM_CUSTOM_META_V1 && length == sizeof(UbMemPrivData) + sizeof(ObmmCustomMetaV1)) {
        ObmmCustomMetaV1 v1{};
        const auto ret = memcpy_s(&v1, sizeof(v1), buffer + sizeof(UbMemPrivData), sizeof(v1));
        if (ret != EOK) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Copying OBMM V1 metadata from private buffer failed, ret=" << ret;
            return UBSE_ERROR_INVAL;
        }
        if (!HasValidName(v1)) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Unterminated OBMM metadata name, version=" << unsigned(version)
                           << ", length=" << length << ", capacity=" << sizeof(v1.name);
            return UBSE_ERROR_INVAL;
        }
        const auto convertRet = ConvertV1(v1, restored);
        if (convertRet != UBSE_OK) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Converting OBMM V1 metadata failed, ret=" << convertRet;
            return convertRet;
        }
    } else if (version == OBMM_CUSTOM_META_V2 && length == sizeof(UbMemPrivData) + sizeof(ObmmCustomMetaV2)) {
        ObmmCustomMetaV2 v2{};
        const auto ret = memcpy_s(&v2, sizeof(v2), buffer + sizeof(UbMemPrivData), sizeof(v2));
        if (ret != EOK) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Copying OBMM V2 metadata from private buffer failed, ret=" << ret;
            return UBSE_ERROR_INVAL;
        }
        if (!HasValidName(v2)) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Unterminated OBMM metadata name, version=" << unsigned(version)
                           << ", length=" << length << ", capacity=" << sizeof(v2.name);
            return UBSE_ERROR_INVAL;
        }
        const auto convertRet = CopyV2Fields(v2, restored);
        if (convertRet != UBSE_OK) {
            return convertRet;
        }
    } else {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Invalid OBMM metadata version or length, version=" << unsigned(version)
                       << ", length=" << length
                       << ", expectedV1Length=" << sizeof(UbMemPrivData) + sizeof(ObmmCustomMetaV1)
                       << ", expectedV2Length=" << sizeof(UbMemPrivData) + sizeof(ObmmCustomMetaV2);
        return UBSE_ERROR_INVAL;
    }
    UbMemPrivData restoredPrivData{};
    const auto ret = memcpy_s(&restoredPrivData, sizeof(restoredPrivData), buffer, sizeof(restoredPrivData));
    if (ret != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Copying OBMM private prefix failed, ret=" << ret;
        return UBSE_ERROR_INVAL;
    }
    meta = restored;
    privData = restoredPrivData;
    return UBSE_OK;
}
} // namespace ubse::mmi
