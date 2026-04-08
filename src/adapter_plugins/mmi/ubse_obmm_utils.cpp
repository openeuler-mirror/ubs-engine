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
#include <securec.h>

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_common_utils.h"
#include "ubse_mem_constants.h"
#include "ubse_mem_def.h"
#include "ubse_node_controller.h"
#include "ubse_obmm_executor.h"
#include "ubse_str_util.h"

namespace ubse::mmi {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::nodeController;
using namespace ubse::mem::strategy;
using namespace ubse::mmi;

constexpr uint32_t INVALID_VALUE_CNA = 0;
constexpr int CNA_FIRST_SHIFT = 24;
constexpr int CNA_PER_SHIFT = 8;

obmm_mem_desc *ConstructExportMemDesc(
    const UbseMemLocalObmmCustomMeta &customMeta, const UbMemPrivData &ubMemPrivData)
{
    uint32_t priLen = sizeof(ubMemPrivData) + sizeof(customMeta);
    auto obmmMemDesc = static_cast<obmm_mem_desc *>(malloc(sizeof(obmm_mem_desc) + priLen));
    if (obmmMemDesc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Malloc return null.";
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
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Memory copy failed.";
        return nullptr;
    }
    ret = memcpy_s(obmmMemDesc->priv + sizeof(ubMemPrivData), sizeof(customMeta), &customMeta, sizeof(customMeta));
    if (ret != EOK) {
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Memory copy failed.";
        return nullptr;
    }
    for (int i = 0; i < UBSE_EID_LENGTH; i++) {
        obmmMemDesc->deid[i] = 0;
    }
    uint32_t deid;
    ret = UbseNodeController::GetInstance().GetLocalEidBySocket(customMeta.exportSocket, deid);
    if (ret != UBSE_OK) {
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        UBSE_LOG_ERROR << MMI_LOG_INFO << "GetLocalEidBySocket error=" << ret << ", exportSocketId="
                       << customMeta.exportSocket;
        return nullptr;
    }
    UBSE_LOG_INFO << "exportSocketId=" << customMeta.exportSocket << ", deid=" << deid;
    ret = memcpy_s(obmmMemDesc->deid, UBSE_EID_LENGTH, &deid, sizeof(deid));
    if (ret != EOK) {
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        UBSE_LOG_ERROR << MMI_LOG_INFO << "obmmMemDesc->deid copy failed.";
        return nullptr;
    }
    return obmmMemDesc;
}

obmm_mem_desc *ConstructImportMemDesc(
    const ObmmOpParam &opParam, const ubse_mem_obmm_mem_desc &desc)
{
    UbseMemLocalObmmCustomMeta customMeta = opParam.customMeta;
    uint16_t marId = desc.marId;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "The marId=" << marId;
    UbMemPrivData ubMemPrivData = opParam.privData;
    ubMemPrivData.mar_id = marId;
    uint32_t priLen = sizeof(ubMemPrivData) + sizeof(customMeta);
    auto obmmMemDesc = static_cast<obmm_mem_desc *>(malloc(sizeof(obmm_mem_desc) + priLen));
    if (obmmMemDesc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Malloc return null.";
        return obmmMemDesc;
    }
    auto ret = memset_s(obmmMemDesc, sizeof(obmm_mem_desc) + priLen, 0, sizeof(obmm_mem_desc) + priLen);
    if (ret != EOK) {
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Memory set failed.";
        return nullptr;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "desc.seid=" << desc.seid;
    UBSE_LOG_INFO << MMI_LOG_INFO << "desc.deid=" << desc.deid;
    CopyObmmMemDescValue(desc, obmmMemDesc, opParam.customMeta.decoderResult.hpa);
    if (!RmCommonUtils::GetInstance().IsValidUint16(priLen)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "PriLen is invalid, value=" << priLen;
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        return nullptr;
    }
    obmmMemDesc->priv_len = priLen;
    ret = memcpy_s(obmmMemDesc->priv, priLen, &ubMemPrivData, sizeof(ubMemPrivData));
    if (ret != EOK) {
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Memory copy failed.";
        return nullptr;
    }
    ret = memcpy_s(obmmMemDesc->priv + sizeof(ubMemPrivData), sizeof(customMeta), &customMeta, sizeof(customMeta));
    if (ret != EOK) {
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Memory copy failed.";
        return nullptr;
    }
    return obmmMemDesc;
}

UbseResult GetCustomMetaFromNumaExportObj(
    const UbseMemNumaBorrowExportObj &exportObj, UbseMemLocalObmmCustomMeta &customMeta)
{
    customMeta.version = 1;
    std::string requestNodeId = exportObj.req.requestNodeId;
    std::string peerNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    customMeta.type = static_cast<uint8_t>(UbseBorrowType::NUMA_BORROW);
    customMeta.uid = exportObj.req.udsInfo.uid;
    customMeta.gid = exportObj.req.udsInfo.gid;
    customMeta.pid = exportObj.req.udsInfo.pid; // 通过pid来判断是否app借用
    if (memcpy_s(customMeta.usrInfo, UBSE_MAX_USR_INFO_LEN, exportObj.req.usrInfo, UBSE_MAX_USR_INFO_LEN) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "MemCopy fail when copy usrInfo, numa exportObj name is "
                       << exportObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    std::string name = exportObj.req.name;
    if (strcpy_s(customMeta.name, UBSE_MEM_MAX_NAME_LENGTH, name.c_str()) != EOK ||
        strcpy_s(customMeta.requestNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, requestNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.exportNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, peerNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.username, UBSE_MEM_MAX_NAME_LENGTH, exportObj.req.udsInfo.username.c_str()) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO
                       << "StrCopy fail when copy requestNodeId and name to meta, name=" << customMeta.name
                       << ", requestNodeId=" << customMeta.exportNodeId;
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "customMeta.username=" << customMeta.username << ", uid=" << customMeta.uid
                  << ", gid=" << customMeta.gid;

    if (exportObj.algoResult.exportNumaInfos.size() != exportObj.algoResult.importNumaInfos.size()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The sizes of importNumaInfos and exportNumaInfos are different. importSize="
                       << exportObj.algoResult.importNumaInfos.size()
                       << " exportSize=" << exportObj.algoResult.exportNumaInfos.size();
        return UBSE_ERROR_INVAL;
    }
    for (int i = 0; i < exportObj.algoResult.exportNumaInfos.size(); i++) {
        if (!RmCommonUtils::GetInstance().IsValidUint8(exportObj.algoResult.importNumaInfos[i].numaId) ||
            !RmCommonUtils::GetInstance().IsValidUint8(exportObj.algoResult.exportNumaInfos[i].numaId)) {
            UBSE_LOG_ERROR << MMI_LOG_INFO
                           << "NumaId is invalid, import numaId=" << exportObj.algoResult.importNumaInfos[i].numaId
                           << ", export numaId=" << exportObj.algoResult.exportNumaInfos[i].numaId;
            return UBSE_ERROR_INVAL;
        }
        customMeta.importNumaIds[i] = exportObj.algoResult.importNumaInfos[i].numaId;
        customMeta.exportNumaIds[i] = exportObj.algoResult.exportNumaInfos[i].numaId;
        customMeta.numaSizes[i] = exportObj.algoResult.exportNumaInfos[i].size;
    }
    return CopyUbseMemAlgoResult(exportObj.algoResult, exportObj.req.name, customMeta, true);
}
UbseResult GetCustomMetaFromNumaImportObj(
    const UbseMemNumaBorrowImportObj &importObj, UbseMemLocalObmmCustomMeta &customMeta)
{
    customMeta.version = 1;
    std::string requestNodeId = importObj.req.requestNodeId;
    std::string peerNodeId = importObj.algoResult.exportNumaInfos[0].nodeId;
    customMeta.type = static_cast<uint8_t>(UbseBorrowType::NUMA_BORROW);
    customMeta.uid = importObj.req.udsInfo.uid;
    customMeta.gid = importObj.req.udsInfo.gid;
    customMeta.pid = importObj.req.udsInfo.pid;
    if (memcpy_s(customMeta.usrInfo, UBSE_MAX_USR_INFO_LEN, importObj.req.usrInfo, UBSE_MAX_USR_INFO_LEN) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "MemCopy fail when copy usrInfo, numa importObj name is "
                       << importObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    std::string name = importObj.req.name;
    if (strcpy_s(customMeta.name, UBSE_MEM_MAX_NAME_LENGTH, name.c_str()) != EOK ||
        strcpy_s(customMeta.requestNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, requestNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.exportNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, peerNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.username, UBSE_MEM_MAX_NAME_LENGTH, importObj.req.udsInfo.username.c_str()) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO
                       << "StrCopy fail when copy requestNodeId and name to meta, name=" << customMeta.name
                       << ", requestNodeId=" << customMeta.exportNodeId;
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "customMeta.username=" << customMeta.username << ", uid=" << customMeta.uid
                  << ", gid:" << customMeta.gid;
    if (importObj.algoResult.exportNumaInfos.size() != importObj.algoResult.importNumaInfos.size()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The sizes of importNumaInfos and exportNumaInfos are different. importSize="
                       << importObj.algoResult.importNumaInfos.size()
                       << " exportSize=" << importObj.algoResult.exportNumaInfos.size();
        return UBSE_ERROR_INVAL;
    }
    for (int i = 0; i < importObj.algoResult.exportNumaInfos.size(); i++) {
        if (!RmCommonUtils::GetInstance().IsValidUint8(importObj.algoResult.importNumaInfos[i].numaId) ||
            !RmCommonUtils::GetInstance().IsValidUint8(importObj.algoResult.exportNumaInfos[i].numaId)) {
            UBSE_LOG_ERROR << MMI_LOG_INFO
                           << "NumaId is invalid, import numaId=" << importObj.algoResult.importNumaInfos[i].numaId
                           << ", export numaId=" << importObj.algoResult.exportNumaInfos[i].numaId;
            return UBSE_ERROR_INVAL;
        }
        customMeta.importNumaIds[i] = importObj.algoResult.importNumaInfos[i].numaId;
        customMeta.exportNumaIds[i] = importObj.algoResult.exportNumaInfos[i].numaId;
        customMeta.numaSizes[i] = importObj.algoResult.exportNumaInfos[i].size;
    }
    customMeta.attachSocket = importObj.algoResult.attachSocketId;
    return CopyUbseMemAlgoResult(importObj.algoResult, importObj.req.name, customMeta, false);
}

UbseResult GetCustomMetaFromFdExportObj(
    const UbseMemFdBorrowExportObj &exportObj, UbseMemLocalObmmCustomMeta &customMeta)
{
    customMeta.version = 1;
    std::string requestNodeId = exportObj.req.requestNodeId;
    customMeta.type = static_cast<uint8_t>(UbseBorrowType::FD_BORROW);
    customMeta.uid = exportObj.req.udsInfo.uid;
    customMeta.gid = exportObj.req.udsInfo.gid;
    customMeta.pid = exportObj.req.udsInfo.pid; // 通过pid来判断是否app借用
    if (strcpy_s(customMeta.requestNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, requestNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.username, UBSE_MEM_MAX_NAME_LENGTH, exportObj.req.udsInfo.username.c_str()) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO
                       << "StrCopy fail when copy requestNodeId and name to meta, name=" << customMeta.name
                       << ", requestNodeId=" << customMeta.exportNodeId;
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "customMeta.username=" << customMeta.username << ", uid=" << customMeta.uid
                  << ", gid=" << customMeta.gid;
    return CopyUbseMemAlgoResult(exportObj.algoResult, exportObj.req.name, customMeta, true);
}

UbseResult GetCustomMetaFromFdImportObj(
    const UbseMemFdBorrowImportObj &importObj, UbseMemLocalObmmCustomMeta &customMeta)
{
    customMeta.version = 1;
    std::string requestNodeId = importObj.req.requestNodeId;
    customMeta.type = static_cast<uint8_t>(UbseBorrowType::FD_BORROW);
    customMeta.uid = importObj.req.udsInfo.uid;
    customMeta.gid = importObj.req.udsInfo.gid;
    customMeta.pid = importObj.req.udsInfo.pid; // 通过pid来判断是否app借用
    if (strcpy_s(customMeta.requestNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, requestNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.username, UBSE_MEM_MAX_NAME_LENGTH, importObj.req.udsInfo.username.c_str()) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO
                       << "StrCopy fail when copy requestNodeId and name to meta, name=" << customMeta.name
                       << ", username=" << customMeta.username << " requestNodeId=" << customMeta.exportNodeId;
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "customMeta.username=" << customMeta.username << ", uid=" << customMeta.uid
                  << ", gid=" << customMeta.gid;
    return CopyUbseMemAlgoResult(importObj.algoResult, importObj.req.name, customMeta, false);
}

UbseResult GetCustomMetaFromShmExportObj(
    const UbseMemShareBorrowExportObj &exportObj, UbseMemLocalObmmCustomMeta &customMeta)
{
    customMeta.version = 1u;
    std::string requestNodeId = exportObj.req.requestNodeId;
    customMeta.type = static_cast<uint8_t>(UbseBorrowType::SHARE_BORROW);
    customMeta.uid = exportObj.req.udsInfo.uid;
    customMeta.gid = exportObj.req.udsInfo.gid;
    customMeta.pid = exportObj.req.udsInfo.pid;
    customMeta.requestSize = exportObj.req.size;
    std::vector<uint32_t> regionNodeIndex{};
    if (exportObj.req.shmAnonymous) {
        UBSE_LOG_DEBUG << "Share memory " << exportObj.req.name << " is anonymous.";
        customMeta.anonymous = 1; // 1 mean anonymous
    }
    for (auto i = 0; i < exportObj.req.shmRegion.nodeNum; i++) {
        regionNodeIndex.push_back(exportObj.req.shmRegion.nodelist[i].index);
    }
    uint32_t regionMask = 0;
    auto ret = SetMaskFromRegionIndex(regionNodeIndex, regionMask);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Construct shmExportObj failed, shm exportObj name is " << exportObj.req.name;
        return ret;
    }
    customMeta.regionMask = regionMask;
    if (strcpy_s(customMeta.requestNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, requestNodeId.c_str()) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO
                       << "StrCopy fail when copy requestNodeId and name to meta, name=" << customMeta.name
                       << ", requestNodeId=" << customMeta.exportNodeId;
        return UBSE_ERROR_INVAL;
    }
    if (strcpy_s(customMeta.username, UBSE_MEM_MAX_NAME_LENGTH, exportObj.req.udsInfo.username.c_str()) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "StrCopy fail when copy username to meta, name=" << customMeta.name
                       << ", username=" << customMeta.username << " requestNodeId=" << customMeta.exportNodeId;
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "customMeta.username=" << customMeta.username << ", uid=" << customMeta.uid
                  << ", gid:" << customMeta.gid;
    if (memcpy_s(customMeta.usrInfo, UBSE_MAX_USR_INFO_LEN, exportObj.req.usrInfo, UBSE_MAX_USR_INFO_LEN) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "MemCopy fail when copy usrInfo, shm exportObj name is "
                       << exportObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    return CopyUbseMemAlgoResult(exportObj.algoResult, exportObj.req.name, customMeta, false);
}

UbseResult GetCustomMetaFromShmImportObj(
    const UbseMemShareBorrowImportObj &importObj, UbseMemLocalObmmCustomMeta &customMeta)
{
    customMeta.version = 1u;
    std::string requestNodeId = importObj.req.requestNodeId;
    customMeta.type = static_cast<uint8_t>(UbseBorrowType::SHARE_BORROW);
    customMeta.uid = importObj.req.udsInfo.uid;
    customMeta.gid = importObj.req.udsInfo.gid;
    customMeta.pid = importObj.req.udsInfo.pid; // 通过pid来判断是否app借用
    customMeta.requestSize = importObj.req.size;
    std::vector<uint32_t> regionNodeIndex{};
    for (size_t i = 0; i < importObj.req.shmRegion.nodelist.size(); i++) {
        regionNodeIndex.push_back(importObj.req.shmRegion.nodelist[i].index);
    }
    uint32_t regionMask = 0;
    auto ret = SetMaskFromRegionIndex(regionNodeIndex, regionMask);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Construct shmExportObj failed, shmObj name is " << importObj.req.name;
        return ret;
    }
    customMeta.regionMask = regionMask;
    if (strcpy_s(customMeta.requestNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, requestNodeId.c_str()) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO
                       << "StrCopy fail when copy requestNodeId and name to meta, name=" << customMeta.name
                       << ", requestNodeId=" << customMeta.exportNodeId;
        return UBSE_ERROR_INVAL;
    }
    if (strcpy_s(customMeta.username, UBSE_MEM_MAX_NAME_LENGTH, importObj.req.udsInfo.username.c_str()) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "StrCopy fail when copy username to meta, name=" << customMeta.name
                       << ", username=" << customMeta.username << ", requestNodeId=" << customMeta.exportNodeId;
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "customMeta.username=" << customMeta.username << ", uid=" << customMeta.uid
                  << ", gid=" << customMeta.gid;
    if (memcpy_s(customMeta.usrInfo, UBSE_MAX_USR_INFO_LEN, importObj.req.usrInfo, UBSE_MAX_USR_INFO_LEN) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "MemCopy fail when copy usrInfo, shm importObj name is "
                       << importObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    return CopyUbseMemAlgoResult(importObj.algoResult, importObj.req.name, customMeta, false);
}

UbseResult GetCustomMetaFromAddrExportObj(
    const UbseMemAddrBorrowExportObj &exportObj, UbseMemLocalObmmCustomMeta &customMeta)
{
    customMeta.version = 1u;
    std::string requestNodeId = exportObj.req.requestNodeId;
    std::string exportNodeId = exportObj.req.exportNodeId;
    std::string importNodeId = exportObj.req.importNodeId;
    customMeta.type = static_cast<uint8_t>(UbseBorrowType::ADDR_BORROW);
    customMeta.srcPid = exportObj.req.importPid;
    customMeta.dstPid = exportObj.req.exportPid;
    customMeta.importSocket = exportObj.req.srcSocket;
    customMeta.exportSocket = exportObj.req.dstSocket;
    const std::string name = GenerateExportKey(exportObj.req.name, importNodeId);
    if (strcpy_s(customMeta.name, UBSE_MEM_MAX_NAME_LENGTH, name.c_str()) != EOK ||
        strcpy_s(customMeta.requestNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, requestNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.exportNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, exportNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.importNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, importNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.username, UBSE_MEM_MAX_NAME_LENGTH, exportObj.req.udsInfo.username.c_str()) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO
                       << "StrCopy fail when copy requestNodeId and name to meta, name=" << customMeta.name
                       << ", requestNodeId=" << customMeta.exportNodeId;
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "customMeta.username=" << customMeta.username << ", uid=" << customMeta.uid
                  << ", gid:" << customMeta.gid;
    if (exportObj.algoResult.exportNumaInfos.size() != exportObj.algoResult.importNumaInfos.size()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The sizes of importNumaInfos and exportNumaInfos are different. importSize="
                       << exportObj.algoResult.importNumaInfos.size()
                       << ", exportSize=" << exportObj.algoResult.exportNumaInfos.size();
        return UBSE_ERROR_INVAL;
    }
    for (int i = 0; i < exportObj.algoResult.exportNumaInfos.size(); i++) {
        customMeta.importNumaIds[i] = exportObj.algoResult.importNumaInfos[i].numaId;
        customMeta.exportNumaIds[i] = exportObj.algoResult.exportNumaInfos[i].numaId;
        customMeta.numaSizes[i] = exportObj.algoResult.exportNumaInfos[i].size;
    }
    return UBSE_OK;
}
UbseResult GetCustomMetaFromAddrImportObj(
    const UbseMemAddrBorrowImportObj &importObj, UbseMemLocalObmmCustomMeta &customMeta, int index)
{
    customMeta.version = 1u;
    std::string requestNodeId = importObj.req.requestNodeId;
    std::string exportNodeId = importObj.req.exportNodeId;
    std::string importNodeId = importObj.req.importNodeId;
    customMeta.type = static_cast<uint8_t>(UbseBorrowType::ADDR_BORROW);
    customMeta.srcPid = importObj.req.importPid;
    customMeta.dstPid = importObj.req.exportPid;
    customMeta.decoderResult = importObj.status.decoderResult[index];
    std::string name = importObj.req.name;
    if (importObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The exportNumaInfos is empty.";
        return UBSE_ERROR_INVAL;
    }

    if (strcpy_s(customMeta.name, UBSE_MEM_MAX_NAME_LENGTH, name.c_str()) != EOK ||
        strcpy_s(customMeta.requestNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, requestNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.exportNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, exportNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.importNodeId, UBSE_MEM_MAX_NODE_ID_LENGTH, importNodeId.c_str()) != EOK ||
        strcpy_s(customMeta.username, UBSE_MEM_MAX_NAME_LENGTH, importObj.req.udsInfo.username.c_str()) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO
                       << "StrCopy fail when copy requestNodeId and name to meta, name=" << customMeta.name
                       << ", requestNodeId=" << customMeta.exportNodeId;
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "customMeta.username=" << customMeta.username << ", uid=" << customMeta.uid
                  << ", gid=" << customMeta.gid;
    if (importObj.algoResult.exportNumaInfos.size() != importObj.algoResult.importNumaInfos.size()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The sizes of importNumaInfos and exportNumaInfos are different. importSize="
                       << importObj.algoResult.importNumaInfos.size()
                       << " exportSize=" << importObj.algoResult.exportNumaInfos.size();
        return UBSE_ERROR_INVAL;
    }
    customMeta.importSocket = importObj.req.srcSocket;
    customMeta.exportSocket = importObj.algoResult.exportNumaInfos[0].socketId;
    customMeta.attachSocket = importObj.algoResult.attachSocketId;
    for (int i = 0; i < importObj.algoResult.exportNumaInfos.size(); i++) {
        customMeta.importNumaIds[i] = importObj.algoResult.importNumaInfos[i].numaId;
        customMeta.exportNumaIds[i] = importObj.algoResult.exportNumaInfos[i].numaId;
        customMeta.numaSizes[i] = importObj.algoResult.exportNumaInfos[i].size;
    }
    customMeta.attachSocket = importObj.algoResult.attachSocketId;
    return UBSE_OK;
}

obmm_preimport_info *ConstructPreImportInfo(const BasicPreImportInfo &basicPreImportInfo)
{
    size_t preImportInfoSize = sizeof(obmm_preimport_info) + sizeof(UbMemPrivData);
    auto preImportInfoPtr = static_cast<obmm_preimport_info *>(malloc(preImportInfoSize));
    if (preImportInfoPtr == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Malloc mem for preImportInfo failed, preImportInfo is nullptr.";
        return nullptr;
    }
    auto res = memset_s(preImportInfoPtr, preImportInfoSize, 0, preImportInfoSize);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Memory set failed.";
        RmCommonUtils::GetInstance().SafeFree(preImportInfoPtr);
        return nullptr;
    }
    UbMemPrivData ubMemPrivData{};
    ConstructUbMemPrivData(ubMemPrivData, basicPreImportInfo.marId, 0);
    preImportInfoPtr->scna = basicPreImportInfo.scna;
    preImportInfoPtr->dcna = basicPreImportInfo.dcna;
    preImportInfoPtr->base_dist = 0;
    preImportInfoPtr->numa_id = basicPreImportInfo.numa_id;
    preImportInfoPtr->pa = basicPreImportInfo.pa;
    preImportInfoPtr->length = basicPreImportInfo.preOnlineSize;
    preImportInfoPtr->priv_len = sizeof(UbMemPrivData);
    auto ret = memcpy_s(preImportInfoPtr->priv, sizeof(UbMemPrivData), &ubMemPrivData, sizeof(UbMemPrivData));
    if (ret != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO
                       << "Memcpy_s failed when copy ub_mem_priv_data to obmm_preimport_info priv, error is " << ret;
        RmCommonUtils::GetInstance().SafeFree(preImportInfoPtr);
        return nullptr;
    }
    ret = memcpy_s(preImportInfoPtr->seid, UBSE_EID_LENGTH, &basicPreImportInfo.seid, sizeof(uint32_t));
    if (ret != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Memcpy_s failed when copy seid to obmm_preimport_info, error is " << ret;
        RmCommonUtils::GetInstance().SafeFree(preImportInfoPtr);
        return nullptr;
    }
    ret = memcpy_s(preImportInfoPtr->deid, UBSE_EID_LENGTH, &basicPreImportInfo.deid, sizeof(uint32_t));
    if (ret != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Memcpy_s failed when copy deid to obmm_preimport_info, error is " << ret;
        RmCommonUtils::GetInstance().SafeFree(preImportInfoPtr);
        return nullptr;
    }

    return preImportInfoPtr;
}

UbseResult RmObmmUtils::GetPreOnlineInfo(std::vector<BasicPreImportInfo> &basicPreImportInfos)
{
    std::ifstream file("/proc/obmm/preimport_info");
    if (!file.is_open()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Can not open the preimport_info, path is /proc/obmm/preimport_info";
        return UBSE_ERROR_IO;
    }
    std::string line;
    std::getline(file, line);
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }
        if (tokens.size() == LINE_STR_NUM) {
            auto res = GetBasicPreImportInfos(basicPreImportInfos, file, tokens);
            if (res != UBSE_OK) {
                UBSE_LOG_ERROR << MMI_LOG_INFO << "The line is invalid, line= " << line;
                return res;
            }
        } else {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "The line is invalid, line= " << line;
            file.close();
            return UBSE_ERROR_INVAL;
        }
    }
    file.close();
    return UBSE_OK;
}

UbseResult RmObmmUtils::GetBasicPreImportInfos(
    std::vector<BasicPreImportInfo> &basicPreImportInfos,
    std::ifstream &file, const std::vector<std::string> &tokens)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "startAddr= " << tokens[START_ADDR_INDEX]
                  << ", endAddr= " << tokens[END_ADDR_INDEX] << ", dcna= " << tokens[DCNA_INDEX]
                  << ", scna= " << tokens[SCNA_INDEX] << ", deid= " << tokens[DEID_INDEX]
                  << ", seid= " << tokens[SEID_INDEX] << ", numaId= " << tokens[NUMAID_INDEX];
    uint64_t startAddr = 0u;
    uint64_t endAddr = 0u;
    uint64_t dcna = 0u;
    uint64_t scna = 0u;
    uint32_t seid = 0u;
    uint32_t deid = 0u;
    long numaId = 0;
    bool tranRet = true;
    tranRet &= RmCommonUtils::GetInstance().StrToULL(tokens[START_ADDR_INDEX], startAddr, BASE_16);
    tranRet &= RmCommonUtils::GetInstance().StrToULL(tokens[END_ADDR_INDEX], endAddr, BASE_16);
    tranRet &= RmCommonUtils::GetInstance().StrToULL(tokens[DCNA_INDEX], dcna, BASE_16);
    tranRet &= RmCommonUtils::GetInstance().StrToULL(tokens[SCNA_INDEX], scna, BASE_16);
    tranRet &= ParsePreOnlineEidStr(tokens[SEID_INDEX], seid);
    tranRet &= ParsePreOnlineEidStr(tokens[DEID_INDEX], deid);
    tranRet &= RmCommonUtils::GetInstance().StrToLong(tokens[NUMAID_INDEX], numaId);
    UBSE_LOG_INFO << MMI_LOG_INFO << "seid=" << seid << ", deid=" << deid;
    if (!tranRet) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Tran str to value failed.";
        file.close();
        return UBSE_ERROR_INVAL;
    }
    if (endAddr < startAddr || (endAddr == UINT64_MAX && startAddr == 0)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Invalid address range.";
        return UBSE_ERROR_INVAL;
    }
    uint64_t preOnlineSize = endAddr - startAddr + 1;
    BasicPreImportInfo basicPreImportInfo{startAddr,
                                          static_cast<uint32_t>(scna),
                                          static_cast<uint32_t>(dcna),
                                          0u,
                                          static_cast<int>(numaId),
                                          preOnlineSize,
                                          seid,
                                          deid};
    basicPreImportInfos.push_back(basicPreImportInfo);
    return UBSE_OK;
}

uint32_t RmObmmUtils::GetPreOnlineSwitch(bool &preOnlineSwitch)
{
    auto module = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to get config module";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    bool tempSwitch = false;
    auto ret = module->GetConf<bool>("ubse.memory", "ubse.preonline.enable", tempSwitch);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << MMI_LOG_INFO << "ubse.preonline.enable, will reset to default: false";
        preOnlineSwitch = false;
        return UBSE_OK;
    }
    preOnlineSwitch = tempSwitch;
    return UBSE_OK;
}
} // namespace ubse::mmi