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
#include "ubse_common_def.h"
#include "ubse_mem_def.h"
#include "ubse_str_util.h"
#include "ubse_security_module.h"
#include "ubse_logger.h"

namespace ubse::mmi {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::security;
constexpr uint32_t HASH_MIN_VAL = 4;
constexpr uint32_t HASH_RANGE_SIZE = 14;
const size_t VALUE_COUNT = 2;

void CopyObmmMemDescValue(const ubse_mem_obmm_mem_desc &src, obmm_mem_desc *des, uint64_t hpa)
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

bool ParsePreOnlineEidStr(const std::string &eid, uint32_t &value)
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

void CopyObmmMemDescValue(const obmm_mem_desc *src, ubse_mem_obmm_mem_desc &des)
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

UbseResult CopyUbseMemAlgoResult(const UbseMemAlgoResult &algoResult,
    const std::string &name, UbseMemLocalObmmCustomMeta &customMeta, const bool isAppendNodeId)
{
    if (algoResult.exportNumaInfos.size() < algoResult.importNumaInfos.size() ||
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
    if (strcpy_s(customMeta.name, UBSE_MEM_MAX_NAME_LENGTH, copyName.c_str()) != EOK ||
        strcpy_s(customMeta.exportNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, exportNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.importNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, importNodeId.c_str()) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO
                       << "StrCopy fail when copy requestNodeId and name to meta, name=" << customMeta.name
                       << ", requestNodeId=" << customMeta.exportNodeId << ", importNodeId=" << importNodeId;
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult RmCommonUtils::GetFileFirstLine(const std::string &path, std::string &line)
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

UbseResult SetMaskFromRegionIndex(const std::vector<uint32_t> &regionNodeIndex, uint32_t &mask)
{
    for (int i = 0; i < regionNodeIndex.size(); i++) {
        if (regionNodeIndex[i] >= 0 && regionNodeIndex[i] < 32u) {
            UBSE_LOG_INFO << MMI_LOG_INFO << "Region bit[" << i << "]=" << regionNodeIndex[i];
            mask |= (1u << regionNodeIndex[i]); // 1 << bit 转为无符号操作
        } else {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Region bit[" << i << "]=" << regionNodeIndex[i];
            return UBSE_ERROR_INVAL;
        }
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "Region bit=" << mask;
    return UBSE_OK;
}

// 映射到 [4, 17] 之间，确保唯一性,生成远端numa
int RmCommonUtils::HashStringToByte(const std::string &input)
{
    static std::hash<std::string> hasher;
    const size_t hashValue = hasher(input);
    return HASH_MIN_VAL + (hashValue % HASH_RANGE_SIZE); // 这一步很容易重复
}

void RmCommonUtils::GenerateNodeChipPortStr(const NodeId &lendNodeId, const ChipId &lendChipId, const PortId &portId,
                                            std::string &nodeChipPortStr)
{
    std::ostringstream str;
    str << lendNodeId << "-" << lendChipId << "-" << portId;
    nodeChipPortStr = str.str();
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "GenerateNodeChipPortStr=" << nodeChipPortStr;
}
} // namespace ubse::mmi