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

#include "ubse_obmm_utils.h"
#include <dlfcn.h>
#include <iostream>
#include "ubse_mem_def.h"
#include "ubse_mem_common_utils.h"
#include "securec.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_inner.h"
#include "ubse_node_controller.h"
#include "ubse_str_util.h"

namespace ubse::mmi {
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::nodeController;
using namespace ubse::mem::strategy;

void RmObmmUtils::CopyObmmMemDescValue(const ubse_mem_obmm_mem_desc &src, obmm_mem_desc *des, uint64_t hpa)
{
    if (des != nullptr) {
        des->addr = hpa;
        des->length = src.length;
        des->tokenid = src.tokenid;
        des->scna = src.scna;
        des->dcna = src.dcna;
        if (memcpy_s(des->seid, UBSE_EID_LENGTH, &src.seid, sizeof(src.seid)) != EOK) {
            UBSE_LOG_ERROR << "seid copy failed";
        }
        if (memcpy_s(des->deid, UBSE_EID_LENGTH, &src.deid, sizeof(src.deid)) != EOK) {
            UBSE_LOG_ERROR << "deid copy failed";
        }
    }
}

bool RmObmmUtils::ParsePreOnlineEidStr(const std::string &eid, uint32_t &value)
{
    uint64_t temp;
    std::vector<std::string> vec;
    ubse::utils::Split(eid, ":", vec);
    constexpr uint32_t BASE_16 = 16;
    if (vec.size() != 2) {
        UBSE_LOG_ERROR << "Parse eid str failed.";
        return false;
    }
    if (!RmCommonUtils::GetInstance().StrToULL(vec[1], temp, BASE_16)) {
        UBSE_LOG_ERROR << "Tran eid str to value failed.";
        return false;
    }
    value = static_cast<uint32_t>(temp);
    return true;
}

void RmObmmUtils::CopyObmmMemDescValue(const obmm_mem_desc *src, ubse_mem_obmm_mem_desc &des)
{
    if (src != nullptr) {
        des.addr = src->addr;
        des.length = src->length;
        des.tokenid = src->tokenid;
        des.scna = src->scna;
        des.dcna = src->dcna;
        if (memcpy_s(&des.seid, sizeof(des.seid), src->seid, sizeof(des.seid)) != EOK) {
            UBSE_LOG_ERROR << "seid copy failed";
        }
        if (memcpy_s(&des.deid, sizeof(des.deid), src->deid, sizeof(des.deid)) != EOK) {
            UBSE_LOG_ERROR << "deid copy failed";
        }
    }
}

void RmObmmUtils::ConstructUbMemPrivData(UbMemPrivData &ubPrivData, const uint16_t marId)
{
    ubPrivData.one_pth = 0u;
    ubPrivData.wr_delay_comp = 0u;
    ubPrivData.reduce_delay_comp = 0u;
    ubPrivData.cmo_delay_comp = 0u;
    ubPrivData.so = 0u;
    ubPrivData.ad_tr_ochip = 0u;
    ubPrivData.cacheable_flag = 1u;
    ubPrivData.mar_id = marId;
    ubPrivData.rsv0 = 0u;
}

obmm_mem_desc *RmObmmUtils::ConstructExportMemDesc(const UbseMemLocalObmmCustomMeta &customMeta)
{
    UbMemPrivData ubMemPrivData{};
    ConstructUbMemPrivData(ubMemPrivData, 0);
    uint32_t priLen = sizeof(ubMemPrivData) + sizeof(customMeta);
    auto obmmMemDesc = static_cast<obmm_mem_desc *>(malloc(sizeof(obmm_mem_desc) + priLen));
    if (obmmMemDesc == nullptr) {
        UBSE_LOG_ERROR << "Malloc return null.";
        return obmmMemDesc;
    }
    obmmMemDesc->addr = 0;
    obmmMemDesc->length = 0;
    obmmMemDesc->tokenid = 0;
    obmmMemDesc->scna = 0;
    obmmMemDesc->dcna = 0;
    obmmMemDesc->priv_len = priLen;

    auto ret = memcpy_s(obmmMemDesc->priv, priLen, &ubMemPrivData, sizeof(ubMemPrivData));
    if (ret != EOK) {
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        UBSE_LOG_ERROR << "Memory copy failed.";
        return nullptr;
    }
    ret = memcpy_s(obmmMemDesc->priv + sizeof(ubMemPrivData), sizeof(customMeta), &customMeta, sizeof(customMeta));
    if (ret != EOK) {
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        UBSE_LOG_ERROR << "Memory copy failed.";
        return nullptr;
    }
    for (int i = 0; i < UBSE_EID_LENGTH; i++) {
        obmmMemDesc->deid[i] = 0;
    }
    uint32_t deid;
    ret = UbseNodeController::GetInstance().GetLocalEidBySocket(customMeta.exportSocket, deid);
    if (ret != UBSE_OK) {
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        UBSE_LOG_ERROR << "GetLocalEidBySocket error: " << ret << ", exportSocketId is " << customMeta.exportSocket;
        return nullptr;
    }
    UBSE_LOG_INFO << "exportSocketId is " << customMeta.exportSocket << ", deid is " << deid;
    ret = memcpy_s(obmmMemDesc->deid, UBSE_EID_LENGTH, &deid, sizeof(deid));
    if (ret != EOK) {
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        UBSE_LOG_ERROR << "obmmMemDesc->deid copy failed.";
        return nullptr;
    }
    return obmmMemDesc;
}

obmm_mem_desc *RmObmmUtils::ConstructImportMemDesc(const UbseMemLocalObmmCustomMeta &customMeta,
                                                   const ubse_mem_obmm_mem_desc &desc)
{
    uint16_t marId = desc.marId;
    UBSE_LOG_DEBUG << "The marId=" << marId;
    UbMemPrivData ubMemPrivData{};
    ConstructUbMemPrivData(ubMemPrivData, marId);
    uint32_t priLen = sizeof(ubMemPrivData) + sizeof(customMeta);
    auto obmmMemDesc = static_cast<obmm_mem_desc *>(malloc(sizeof(obmm_mem_desc) + priLen));
    if (obmmMemDesc != nullptr) {
        auto ret = memset_s(obmmMemDesc, sizeof(obmm_mem_desc) + priLen, 0, sizeof(obmm_mem_desc) + priLen);
        if (ret != EOK) {
            RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
            return nullptr;
        }
    }
    if (obmmMemDesc == nullptr) {
        UBSE_LOG_ERROR << "Malloc return null.";
        return obmmMemDesc;
    }
    UBSE_LOG_INFO << "desc.seid: " << desc.seid;
    UBSE_LOG_INFO << "desc.deid: " << desc.deid;
    CopyObmmMemDescValue(desc, obmmMemDesc, customMeta.decoderResult.hpa);
    if (!RmCommonUtils::GetInstance().IsValidUint16(priLen)) {
        UBSE_LOG_ERROR << "PriLen is invalid, value = " << priLen;
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        return nullptr;
    }
    obmmMemDesc->priv_len = priLen;
    auto ret = memcpy_s(obmmMemDesc->priv, priLen, &ubMemPrivData, sizeof(ubMemPrivData));
    if (ret != EOK) {
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        UBSE_LOG_ERROR << "Memory copy failed.";
        return nullptr;
    }
    ret = memcpy_s(obmmMemDesc->priv + sizeof(ubMemPrivData), sizeof(customMeta), &customMeta, sizeof(customMeta));
    if (ret != EOK) {
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        UBSE_LOG_ERROR << "Memory copy failed.";
        return nullptr;
    }
    return obmmMemDesc;
}

UbseResult RmObmmUtils::GetCustomMetaFromNumaExportObj(const UbseMemNumaBorrowExportObj &exportObj,
                                                       UbseMemLocalObmmCustomMeta &customMeta)
{
    customMeta.version = 1;
    std::string requestNodeId = exportObj.req.requestNodeId;
    std::string peerNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    customMeta.type = static_cast<uint8_t>(UbseBorrowType::NUMA_BORROW);
    customMeta.uid = exportObj.req.udsInfo.uid;
    customMeta.gid = exportObj.req.udsInfo.gid;
    customMeta.pid = exportObj.req.udsInfo.pid; // 通过pid来判断是否app借用
    std::string name = exportObj.req.name;
    if (strcpy_s(customMeta.name, UBSE_MEM_MAX_NAME_LENGTH, name.c_str()) != EOK ||
        strcpy_s(customMeta.requestNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, requestNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.exportNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, peerNodeId.c_str()) != EOK) {
        UBSE_LOG_ERROR << "StrCopy fail when copy requestNodeId and name to meta, name: " << customMeta.name
                     << " requestNodeId: " << customMeta.exportNodeId;
        return UBSE_ERROR_INVAL;
    }
    if (exportObj.algoResult.exportNumaInfos.size() != exportObj.algoResult.importNumaInfos.size()) {
        UBSE_LOG_ERROR << "export numaInfos size is not equal import numaInfos size, import obj is "
                     << RmCommonUtils::GetInstance().TranStructToStr(exportObj);
        return UBSE_ERROR_INVAL;
    }
    for (int i = 0; i < exportObj.algoResult.exportNumaInfos.size(); i++) {
        if (!RmCommonUtils::GetInstance().IsValidUint8(exportObj.algoResult.importNumaInfos[i].numaId) ||
            !RmCommonUtils::GetInstance().IsValidUint8(exportObj.algoResult.exportNumaInfos[i].numaId)) {
            UBSE_LOG_ERROR << "NumaId is invalid, import numaId = " << exportObj.algoResult.importNumaInfos[i].numaId
                         << ", export numaId = " << exportObj.algoResult.exportNumaInfos[i].numaId;
            return UBSE_ERROR_INVAL;
        }
        customMeta.importNumaIds[i] = exportObj.algoResult.importNumaInfos[i].numaId;
        customMeta.exportNumaIds[i] = exportObj.algoResult.exportNumaInfos[i].numaId;
        customMeta.numaSizes[i] = exportObj.algoResult.exportNumaInfos[i].size;
    }
    return CopyUbseMemAlgoResult(exportObj.algoResult, exportObj.req.name, customMeta);
}
UbseResult RmObmmUtils::GetCustomMetaFromNumaImportObj(const UbseMemNumaBorrowImportObj &importObj,
                                                       UbseMemLocalObmmCustomMeta &customMeta)
{
    customMeta.version = 1;
    std::string requestNodeId = importObj.req.requestNodeId;
    std::string peerNodeId = importObj.algoResult.exportNumaInfos[0].nodeId;
    customMeta.type = static_cast<uint8_t>(UbseBorrowType::NUMA_BORROW);
    customMeta.uid = importObj.req.udsInfo.uid;
    customMeta.gid = importObj.req.udsInfo.gid;
    customMeta.pid = importObj.req.udsInfo.pid;
    std::string name = importObj.req.name;
    if (strcpy_s(customMeta.name, UBSE_MEM_MAX_NAME_LENGTH, name.c_str()) != EOK ||
        strcpy_s(customMeta.requestNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, requestNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.exportNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, peerNodeId.c_str()) != EOK) {
        UBSE_LOG_ERROR << "StrCopy fail when copy requestNodeId and name to meta, name: " << customMeta.name
                     << " requestNodeId: " << customMeta.exportNodeId;
        return UBSE_ERROR_INVAL;
    }
    if (importObj.algoResult.exportNumaInfos.size() != importObj.algoResult.importNumaInfos.size()) {
        UBSE_LOG_ERROR << "export numaInfos size is not equal import numaInfos size, import obj is "
                     << RmCommonUtils::GetInstance().TranStructToStr(importObj);
        return UBSE_ERROR_INVAL;
    }
    for (int i = 0; i < importObj.algoResult.exportNumaInfos.size(); i++) {
        if (!RmCommonUtils::GetInstance().IsValidUint8(importObj.algoResult.importNumaInfos[i].numaId) ||
            !RmCommonUtils::GetInstance().IsValidUint8(importObj.algoResult.exportNumaInfos[i].numaId)) {
            UBSE_LOG_ERROR << "NumaId is invalid, import numaId = " << importObj.algoResult.importNumaInfos[i].numaId
                         << ", export numaId = " << importObj.algoResult.exportNumaInfos[i].numaId;
            return UBSE_ERROR_INVAL;
        }
        customMeta.importNumaIds[i] = importObj.algoResult.importNumaInfos[i].numaId;
        customMeta.exportNumaIds[i] = importObj.algoResult.exportNumaInfos[i].numaId;
        customMeta.numaSizes[i] = importObj.algoResult.exportNumaInfos[i].size;
    }
    customMeta.attachSocket = importObj.algoResult.attachSocketId;
    return CopyUbseMemAlgoResult(importObj.algoResult, importObj.req.name, customMeta);
}

UbseResult RmObmmUtils::CopyUbseMemAlgoResult(const UbseMemAlgoResult &algoResult, const std::string &name,
                                              UbseMemLocalObmmCustomMeta &customMeta)
{
    if (algoResult.exportNumaInfos.size() < algoResult.importNumaInfos.size() ||
        UBSE_MEM_MAX_EXPORT_NUMA_SIZE < algoResult.exportNumaInfos.size()) {
        UBSE_LOG_ERROR << "exportNumaInfos.size=" << algoResult.exportNumaInfos.size()
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
    }
    customMeta.attachSocket = algoResult.attachSocketId;
    customMeta.exportSocket = algoResult.exportNumaInfos[0].socketId;
    std::string exportNodeId = algoResult.exportNumaInfos[0].nodeId;
    if (exportNodeId.empty()) {
        UBSE_LOG_ERROR << "exportNodeId=" << exportNodeId << ", importNodeId=" << importNodeId;
        return UBSE_ERROR_INVAL;
    }
    if (strcpy_s(customMeta.name, UBSE_MEM_MAX_NAME_LENGTH, name.c_str()) != EOK ||
        strcpy_s(customMeta.exportNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, exportNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.importNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, importNodeId.c_str()) != EOK) {
        UBSE_LOG_ERROR << "StrCopy fail when copy requestNodeId and name to meta, name=" << customMeta.name
                     << " requestNodeId=" << customMeta.exportNodeId << ", importNodeId=" << importNodeId;
        return UBSE_ERROR_INVAL;
    }
    return HOK;
}

UbseResult RmObmmUtils::GetCustomMetaFromFdExportObj(const UbseMemFdBorrowExportObj &exportObj,
                                                     UbseMemLocalObmmCustomMeta &customMeta)
{
    customMeta.version = 1;
    std::string requestNodeId = exportObj.req.requestNodeId;
    customMeta.type = static_cast<uint8_t>(UbseBorrowType::FD_BORROW);
    customMeta.uid = exportObj.req.udsInfo.uid;
    customMeta.gid = exportObj.req.udsInfo.gid;
    customMeta.pid = exportObj.req.udsInfo.pid; // 通过pid来判断是否app借用
    if (strcpy_s(customMeta.requestNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, requestNodeId.c_str()) != EOK) {
        UBSE_LOG_ERROR << "StrCopy fail when copy requestNodeId and name to meta, name: " << customMeta.name
                     << " requestNodeId: " << customMeta.exportNodeId;
        return UBSE_ERROR_INVAL;
    }
    return CopyUbseMemAlgoResult(exportObj.algoResult, exportObj.req.name, customMeta);
}

UbseResult RmObmmUtils::GetCustomMetaFromFdImportObj(const UbseMemFdBorrowImportObj &importObj,
                                                     UbseMemLocalObmmCustomMeta &customMeta)
{
    customMeta.version = 1;
    std::string requestNodeId = importObj.req.requestNodeId;
    customMeta.type = static_cast<uint8_t>(UbseBorrowType::FD_BORROW);
    customMeta.uid = importObj.req.udsInfo.uid;
    customMeta.gid = importObj.req.udsInfo.gid;
    customMeta.pid = importObj.req.udsInfo.pid; // 通过pid来判断是否app借用
    if (strcpy_s(customMeta.requestNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, requestNodeId.c_str()) != EOK) {
        UBSE_LOG_ERROR << "StrCopy fail when copy requestNodeId and name to meta, name: " << customMeta.name
                     << " requestNodeId: " << customMeta.exportNodeId;
        return UBSE_ERROR_INVAL;
    }
    return CopyUbseMemAlgoResult(importObj.algoResult, importObj.req.name, customMeta);
}

bool RmObmmUtils::SplitObmmExportSize(const size_t size[MAX_NUMA_NODES], const size_t blockSize,
                                      std::vector<std::array<size_t, MAX_NUMA_NODES>> &result)
{
    size_t blockNum[MAX_NUMA_NODES]{};
    size_t blockAll{0};
    if (blockSize == 0u) {
        UBSE_LOG_ERROR << "Block size is 0, is invalid.";
        return false;
    }
    for (int i = 0; i < MAX_NUMA_NODES; ++i) {
        blockNum[i] = (size[i] + blockSize - 1) / blockSize;
        blockAll += blockNum[i];
    }
    result.clear();
    result.reserve(blockAll);
    for (int i = 0; i < blockAll; ++i) {
        result.push_back({});
    }
    int index = 0;
    for (int i = 0; i < MAX_NUMA_NODES; ++i) {
        for (int j = 0; j < blockNum[i]; ++j) {
            result[index++][i] = blockSize;
        }
    }
    return !result.empty();
}

uint32_t RmObmmUtils::GetBlockSize(uint64_t &blockSize)
{
    auto module = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "Failed to get config module";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto ret =
        module->GetConf<uint64_t>(OBMM_MEM_MANAGER_CONFIG_SECTION_NAME, OBMM_MEMORY_BLOCK_SIZE_CONFIG_KEY, blockSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Block size invalid, will reset to default: " << DEFAULT_BLOCK_SIZE << " MB.";
        blockSize = RmCommonUtils::GetInstance().SizeMb2Byte(blockSize);
        return UBSE_OK;
    }
    if (blockSize < MIN_BLOCK_SIZE || blockSize > MAX_BLOCK_SIZE || (blockSize % MIN_BLOCK_SIZE != 0u)) {
        UBSE_LOG_ERROR << "Block size is invalid, size = " << blockSize;
        return UBSE_ERROR_INVAL;
    }
    blockSize = RmCommonUtils::GetInstance().SizeMb2Byte(blockSize);
    return UBSE_OK;
}

} // namespace ubse::mmi