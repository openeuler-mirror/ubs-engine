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

#include "ubse_mem_obj_restore.h"

#include "ubse_mem_def.h"
#include "ubse_mem_instance_inner.h"
#include "ubse_obmm_meta_restore.h"
#include "ubse_obmm_utils.h"
#include "securec.h"

namespace ubse::mmi::restore {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::common::def;
void ConstructSingleFdImportObj(const std::vector<UbseMemLocalObmmMetaData>& fdImportLocalObmmMetaDatas,
                                UbseMemFdBorrowImportObj& ubseMemFdBorrowImportObj, bool& isNormal)
{
    if (fdImportLocalObmmMetaDatas.empty()) {
        isNormal = false;
        return;
    }
    UbseMemLocalObmmMetaData obmmMetaData = fdImportLocalObmmMetaDatas[0];
    UbseMemFdBorrowReq req{};
    req.name = std::string(obmmMetaData.customMeta.name);
    req.requestNodeId = std::string(obmmMetaData.customMeta.requestNodeId); // 需要额外加
    req.importNodeId = obmmMetaData.localNodeId;
    req.distance = MEM_DISTANCE_L0; // 使用默认值，不额外加，因为决策完成之后，这个就没有作用了
    req.udsInfo = {obmmMetaData.customMeta.uid, obmmMetaData.customMeta.gid,
                   static_cast<int>(obmmMetaData.customMeta.pid), obmmMetaData.customMeta.username};
    UBSE_LOG_INFO << MMI_LOG_INFO << "req.udsInfo.username=" << req.udsInfo.username << ", uid=" << req.udsInfo.uid
                  << ", gid=" << req.udsInfo.gid;
    std::string lendNode = std::string(obmmMetaData.customMeta.exportNodeId);
    int numaCount = 0;
    uint64_t resourceMemSize = 0;
    for (int i = 0; i < TOPOLOGY_MAX_NUMA_PER_SOCKET; i++) {
        if (obmmMetaData.customMeta.numaSizes[i] != NO_0) {
            numaCount++;
            req.lenderLocs.push_back({lendNode, obmmMetaData.customMeta.exportNumaIds[i]});
            req.lenderSizes.push_back(obmmMetaData.customMeta.numaSizes[i]);
            bool isOverflow = false;
            resourceMemSize =
                RmCommonUtils::GetInstance().SafeAdd(resourceMemSize, obmmMetaData.customMeta.numaSizes[i], isOverflow);
            if (isOverflow) {
                UBSE_LOG_ERROR << MMI_LOG_INFO
                               << "Overflow occurred during addition. resourceMemSize=" << resourceMemSize
                               << ", numaSize=" << obmmMetaData.customMeta.numaSizes[i];
                return;
            }
        }
    }
    req.size = resourceMemSize;
    // 构造其他的东西
    UbseMemAlgoResult algoResult{};
    algoResult.attachSocketId = obmmMetaData.customMeta.attachSocket;
    for (int i = 0; i < numaCount; i++) {
        algoResult.exportNumaInfos.push_back({lendNode, obmmMetaData.customMeta.exportSocket,
                                              obmmMetaData.customMeta.exportNumaIds[i],
                                              obmmMetaData.customMeta.numaSizes[i]});
        algoResult.importNumaInfos.push_back(
            {req.importNodeId, obmmMetaData.customMeta.importSocket, obmmMetaData.customMeta.importNumaIds[i],
             obmmMetaData.customMeta.numaSizes[i], obmmMetaData.customMeta.portId, obmmMetaData.customMeta.chipId});
    }
    algoResult.blockSize = RmCommonUtils::GetInstance().SizeByte2Mb(obmmMetaData.totalSize);
    std::vector<UbseMemObmmInfo> exportObmmInfo{};
    std::vector<UbseMemImportResult> importResults{};
    UbseMemImportStatus status{};
    for (size_t i = 0; i < fdImportLocalObmmMetaDatas.size(); i++) {
        exportObmmInfo.push_back(
            {fdImportLocalObmmMetaDatas[i].customMeta.exportMemid, fdImportLocalObmmMetaDatas[i].obmmMemExportInfo});
        importResults.push_back({fdImportLocalObmmMetaDatas[i].localMemId, -1});
        status.decoderResult.push_back(fdImportLocalObmmMetaDatas[i].customMeta.decoderResult);
    }

    status.errCode = UBSE_OK;
    status.importResults = importResults;

    ubseMemFdBorrowImportObj.req = req;
    ubseMemFdBorrowImportObj.status = status;
    ubseMemFdBorrowImportObj.algoResult = algoResult;
    ubseMemFdBorrowImportObj.exportObmmInfo = exportObmmInfo;
    isNormal = status.importResults.size() == obmmMetaData.customMeta.memidCount;
}

UbseResult ConstructFdImportObj(const std::vector<UbseMemLocalObmmMetaData>& fdImportLocalObmmMetaDatas,
                                UbseMemFdImportObjMap& normalFdImportObjMap)
{
    UbseMemFdImportObjMap abnormalFdImportObjMap{};
    UbseResult ret = UBSE_OK;
    std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData>> fdBorrowImportObjMap{};
    for (auto& localObmmMetaDatasItem : fdImportLocalObmmMetaDatas) {
        std::string name = localObmmMetaDatasItem.customMeta.name;
        if (fdBorrowImportObjMap.find(name) != fdBorrowImportObjMap.end()) {
            fdBorrowImportObjMap[name].push_back(localObmmMetaDatasItem);
        } else {
            fdBorrowImportObjMap[name] = {localObmmMetaDatasItem};
        }
    }
    for (auto& fdBorrowImportItem : fdBorrowImportObjMap) {
        UbseMemFdBorrowImportObj ubseMemFdBorrowImportObj{};
        bool isNormal = true;
        ConstructSingleFdImportObj(fdBorrowImportItem.second, ubseMemFdBorrowImportObj, isNormal);
        if (isNormal) {
            normalFdImportObjMap.emplace(fdBorrowImportItem.first, ubseMemFdBorrowImportObj);
        } else {
            abnormalFdImportObjMap.emplace(fdBorrowImportItem.first, ubseMemFdBorrowImportObj);
        }
    }
    normalFdImportObjMap.merge(ProcessAbnormalImportObjMap(abnormalFdImportObjMap));
    return ret;
}

void ConstructSingleFdExportObj(const std::vector<UbseMemLocalObmmMetaData>& exportLocalObmmMetaDatas,
                                UbseMemFdBorrowExportObj& ubseMemFdBorrowExportObj, bool& isNormal)
{
    if (exportLocalObmmMetaDatas.empty()) {
        isNormal = false;
        return;
    }
    UbseMemLocalObmmMetaData obmmMetaData = exportLocalObmmMetaDatas[0];
    UbseMemFdBorrowReq req{};
    req.name = std::string(obmmMetaData.customMeta.name);
    if (size_t pos = req.name.find_last_of('_'); pos != std::string::npos) {
        req.name = req.name.substr(0, pos);
    }
    req.requestNodeId = std::string(obmmMetaData.customMeta.requestNodeId);
    req.importNodeId = std::string(obmmMetaData.customMeta.importNodeId);
    req.distance = MEM_DISTANCE_L0; // 使用默认值，不额外加，因为决策完成之后，这个就没有作用了
    req.udsInfo = {obmmMetaData.customMeta.uid, obmmMetaData.customMeta.gid,
                   static_cast<int>(obmmMetaData.customMeta.pid), obmmMetaData.customMeta.username};
    UBSE_LOG_INFO << MMI_LOG_INFO << "req.udsInfo.username=" << req.udsInfo.username << ", uid=" << req.udsInfo.uid
                  << ", gid=" << req.udsInfo.gid;
    std::string lendNode = std::string(obmmMetaData.customMeta.exportNodeId);
    int numaCount = 0;
    uint64_t resourceMemSize = 0;
    for (int i = 0; i < TOPOLOGY_MAX_NUMA_PER_SOCKET; i++) {
        if (obmmMetaData.customMeta.numaSizes[i] != NO_0) {
            numaCount++;
            req.lenderLocs.push_back({lendNode, obmmMetaData.customMeta.exportNumaIds[i]});
            req.lenderSizes.push_back(obmmMetaData.customMeta.numaSizes[i]);
            bool isOverflow = false;
            resourceMemSize =
                RmCommonUtils::GetInstance().SafeAdd(resourceMemSize, obmmMetaData.customMeta.numaSizes[i], isOverflow);
            if (isOverflow) {
                UBSE_LOG_ERROR << MMI_LOG_INFO
                               << "Overflow occurred during addition. resourceMemSize=" << resourceMemSize
                               << ", numaSize=" << obmmMetaData.customMeta.numaSizes[i];
                return;
            }
        }
    }
    req.size = resourceMemSize;
    // 构造其他的东西
    UbseMemAlgoResult algoResult{};
    // 导出对象这里的attachSocket不赋值，统一上面的流程
    for (int i = 0; i < numaCount; i++) {
        algoResult.exportNumaInfos.push_back({lendNode, obmmMetaData.customMeta.exportSocket,
                                              obmmMetaData.customMeta.exportNumaIds[i],
                                              obmmMetaData.customMeta.numaSizes[i]});
        algoResult.importNumaInfos.push_back({req.importNodeId, obmmMetaData.customMeta.importSocket,
                                              obmmMetaData.customMeta.importNumaIds[i],
                                              obmmMetaData.customMeta.numaSizes[i]});
    }

    std::vector<UbseMemObmmInfo> exportObmmInfo{};
    for (size_t i = 0; i < exportLocalObmmMetaDatas.size(); i++) {
        exportObmmInfo.push_back(
            {exportLocalObmmMetaDatas[i].localMemId, exportLocalObmmMetaDatas[i].obmmMemExportInfo});
    }
    algoResult.blockSize = RmCommonUtils::GetInstance().SizeByte2Mb(obmmMetaData.totalSize);
    UbseMemExportStatus status{};
    status.errCode = UBSE_OK;
    status.exportObmmInfo = exportObmmInfo;

    ubseMemFdBorrowExportObj.req = req;
    ubseMemFdBorrowExportObj.status = status;
    ubseMemFdBorrowExportObj.algoResult = algoResult;
    isNormal = status.exportObmmInfo.size() == obmmMetaData.customMeta.memidCount;
}

UbseResult ConstructFdExportObj(const std::vector<UbseMemLocalObmmMetaData>& exportLocalObmmMetaDatas,
                                UbseMemFdExportObjMap& normalFdExportObjMap)
{
    UbseResult ret = UBSE_OK;
    UbseMemFdExportObjMap abnormalFdExportObjMap{};
    std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData>> exportObjMap{};
    for (auto& localObmmMetaDatasItem : exportLocalObmmMetaDatas) {
        std::string name = localObmmMetaDatasItem.customMeta.name;
        if (exportObjMap.find(name) != exportObjMap.end()) {
            exportObjMap[name].push_back(localObmmMetaDatasItem);
        } else {
            exportObjMap[name] = {localObmmMetaDatasItem};
        }
    }
    // 芯片表项碎片，导入一半的时候，进程挂掉了，从obmm获取数据前等3s,保证单次最大1G执行完成
    for (auto& exportItem : exportObjMap) {
        UbseMemFdBorrowExportObj ubseMemFdBorrowExportObj{};
        bool isNormal = true;
        ConstructSingleFdExportObj(exportItem.second, ubseMemFdBorrowExportObj, isNormal);
        if (isNormal) {
            normalFdExportObjMap.emplace(exportItem.first, ubseMemFdBorrowExportObj);
        } else {
            abnormalFdExportObjMap.emplace(exportItem.first, ubseMemFdBorrowExportObj);
        }
    }
    normalFdExportObjMap.merge(ProcessAbnormalExportObjMap(abnormalFdExportObjMap));
    return ret;
}

void ConstructSingleNumaImportObj(const std::vector<UbseMemLocalObmmMetaData>& importLocalObmmMetaDatas,
                                  UbseMemNumaBorrowImportObj& ubseMemNumaBorrowImportObj, bool& isNormal)
{
    if (importLocalObmmMetaDatas.empty()) {
        isNormal = false;
        return;
    }
    UbseMemLocalObmmMetaData obmmMetaData = importLocalObmmMetaDatas[0];
    UbseMemNumaBorrowReq req{};
    auto& meta = obmmMetaData.customMeta;
    std::string lendNode = std::string(meta.exportNodeId);
    BuildSingleNumaImportReq(meta, req, lendNode);
    if (memcpy_s(req.usrInfo, UBSE_MAX_USR_INFO_LEN, obmmMetaData.customMeta.usrInfo, UBSE_MAX_USR_INFO_LEN) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "MemCopy fail when copy usrInfo, name=" << req.name << ", usrInfo="
                       << reinterpret_cast<char*>(
                              obmmMetaData.customMeta.usrInfo); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        isNormal = false;
        return;
    }
    // 构造其他的东西
    UbseMemAlgoResult algoResult{};
    algoResult.attachSocketId = meta.attachSocket;
    for (int i = 0; i < TOPOLOGY_MAX_NUMA_PER_SOCKET; i++) {
        if (meta.numaSizes[i] != NO_0) {
            algoResult.exportNumaInfos.push_back(
                {lendNode, meta.exportSocket, meta.exportNumaIds[i], meta.numaSizes[i]});
            algoResult.importNumaInfos.push_back({req.importNodeId, meta.importSocket, meta.importNumaIds[i],
                                                  meta.numaSizes[i], meta.portId, meta.chipId});
        }
    }
    std::vector<UbseMemObmmInfo> exportObmmInfo{};
    std::vector<UbseMemImportResult> importResults{};
    UbseMemImportStatus status{};
    for (size_t i = 0; i < importLocalObmmMetaDatas.size(); i++) {
        exportObmmInfo.push_back(
            {importLocalObmmMetaDatas[i].customMeta.exportMemid, importLocalObmmMetaDatas[i].obmmMemExportInfo});
        importResults.push_back({importLocalObmmMetaDatas[i].localMemId, importLocalObmmMetaDatas[i].remoteNumaId});
        status.decoderResult.push_back(importLocalObmmMetaDatas[i].customMeta.decoderResult);
    }
    algoResult.blockSize = RmCommonUtils::GetInstance().SizeByte2Mb(obmmMetaData.totalSize);
    status.errCode = UBSE_OK;
    status.importResults = importResults;

    ubseMemNumaBorrowImportObj.req = req;
    ubseMemNumaBorrowImportObj.status = status;
    ubseMemNumaBorrowImportObj.algoResult = algoResult;
    ubseMemNumaBorrowImportObj.exportObmmInfo = exportObmmInfo;
    isNormal = status.importResults.size() == meta.memidCount;
}
void BuildSingleNumaImportReq(const UbseMemLocalObmmCustomMeta& meta, UbseMemNumaBorrowReq& req, std::string& lendNode)
{
    req.name = std::string(meta.name);
    req.requestNodeId = std::string(meta.requestNodeId); // 需要额外加
    req.importNodeId = std::string(meta.importNodeId);
    req.distance = MEM_DISTANCE_L0; // 使用默认值，不额外加，因为决策完成之后，这个就没有作用了
    req.udsInfo = {meta.uid, meta.gid, static_cast<int>(meta.pid), meta.username};
    UBSE_LOG_INFO << MMI_LOG_INFO << "req.udsInfo.username=" << req.udsInfo.username << ", uid=" << req.udsInfo.uid
                  << ", gid=" << req.udsInfo.gid;
    uint64_t resourceMemSize = 0;
    for (int i = 0; i < TOPOLOGY_MAX_NUMA_PER_SOCKET; i++) {
        if (meta.numaSizes[i] != NO_0) {
            req.lenderLocs.push_back({lendNode, meta.exportNumaIds[i]});
            req.lenderSizes.push_back(meta.numaSizes[i]);
            bool isOverflow = false;
            resourceMemSize = RmCommonUtils::GetInstance().SafeAdd(resourceMemSize, meta.numaSizes[i], isOverflow);
            if (isOverflow) {
                UBSE_LOG_ERROR << MMI_LOG_INFO
                               << "Overflow occurred during addition. resourceMemSize=" << resourceMemSize
                               << ", numaSize=" << meta.numaSizes[i];
                return;
            }
        }
    }
    req.size = resourceMemSize;
    // 大数据的numa借用，这个值是没有的，直接用默认值，虚拟化的numa借用，这里才会有用
    if (req.udsInfo.pid == NO_0) {
        req.srcSocket = meta.importSocket;
        req.srcNuma = meta.importNumaIds[0];
    }
}

UbseResult ConstructNumaImportObj(const std::vector<UbseMemLocalObmmMetaData>& importLocalObmmMetaDatas,
                                  UbseMemNumaImportObjMap& normalImportObjMap)
{
    UbseResult ret = UBSE_OK;
    UbseMemNumaImportObjMap abnormalImportObjMap{};
    std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData>> importObjMap{};
    GetBorrowObjMap(importLocalObmmMetaDatas, importObjMap);
    // 芯片表项碎片，导入一半的时候，进程挂掉了，从obmm获取数据前等3s,保证单次最大1G执行完成
    for (auto& importObjItem : importObjMap) {
        UbseMemNumaBorrowImportObj ubseMemImportObj{};
        bool isNormal = true;
        ConstructSingleNumaImportObj(importObjItem.second, ubseMemImportObj, isNormal);
        if (isNormal) {
            normalImportObjMap.emplace(importObjItem.first, ubseMemImportObj);
        } else {
            abnormalImportObjMap.emplace(importObjItem.first, ubseMemImportObj);
        }
    }
    normalImportObjMap.merge(ProcessAbnormalImportObjMap(abnormalImportObjMap));
    return ret;
}

UbseResult ConstructNumaExportObj(const std::vector<UbseMemLocalObmmMetaData>& exportLocalObmmMetaDatas,
                                  UbseMemNumaExportObjMap& normalExportObjMap)
{
    UbseResult ret = UBSE_OK;
    UbseMemNumaExportObjMap abnormalExportObjMap{};
    std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData>> exportObjMap{};
    GetBorrowObjMap(exportLocalObmmMetaDatas, exportObjMap);
    for (auto& exportItem : exportObjMap) {
        UbseMemNumaBorrowExportObj ubseMemNumaBorrowExportObj{};
        bool isNormal = true;
        ConstructSingleNumaExportObj(exportItem.second, ubseMemNumaBorrowExportObj, isNormal);
        if (isNormal) {
            normalExportObjMap.emplace(exportItem.first, ubseMemNumaBorrowExportObj);
        } else {
            abnormalExportObjMap.emplace(exportItem.first, ubseMemNumaBorrowExportObj);
        }
    }
    normalExportObjMap.merge(ProcessAbnormalExportObjMap(abnormalExportObjMap));
    return ret;
}

static bool AfterConstructSingleNumaExportObj(const std::vector<UbseMemLocalObmmMetaData>& exportLocalObmmMetaDatas,
                                              UbseMemNumaBorrowExportObj& mxeMemNumaBorrowExportObj,
                                              const UbseMemLocalObmmMetaData& obmmMetaData,
                                              const UbseMemNumaBorrowReq& req, const UbseMemAlgoResult& algoResult)
{
    std::vector<UbseMemObmmInfo> exportObmmInfo{};
    for (size_t i = 0; i < exportLocalObmmMetaDatas.size(); i++) {
        exportObmmInfo.push_back(
            {exportLocalObmmMetaDatas[i].localMemId, exportLocalObmmMetaDatas[i].obmmMemExportInfo});
    }
    UbseMemExportStatus status{};
    status.errCode = UBSE_OK;
    status.exportObmmInfo = exportObmmInfo;

    mxeMemNumaBorrowExportObj.req = req;
    mxeMemNumaBorrowExportObj.status = status;
    mxeMemNumaBorrowExportObj.algoResult = algoResult;

    return status.exportObmmInfo.size() == obmmMetaData.customMeta.memidCount;
}

void ConstructSingleNumaExportObj(const std::vector<UbseMemLocalObmmMetaData>& exportLocalObmmMetaDatas,
                                  UbseMemNumaBorrowExportObj& ubseMemNumaBorrowExportObj, bool& isNormal)
{
    if (exportLocalObmmMetaDatas.empty()) {
        isNormal = false;
        return;
    }
    UbseMemLocalObmmMetaData obmmMetaData = exportLocalObmmMetaDatas[0];
    std::string lendNode = std::string(obmmMetaData.customMeta.exportNodeId);
    UbseMemNumaBorrowReq req{};
    BuildSingleNumaExportReq(obmmMetaData, lendNode, req);
    if (memcpy_s(req.usrInfo, UBSE_MAX_USR_INFO_LEN, obmmMetaData.customMeta.usrInfo, UBSE_MAX_USR_INFO_LEN) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "MemCopy fail when copy usrInfo, name is " << req.name << ", usrInfo="
                       << reinterpret_cast<char*>(
                              obmmMetaData.customMeta.usrInfo); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        isNormal = false;
        return;
    }
    // 构造其他的东西
    UbseMemAlgoResult algoResult{};
    for (int i = 0; i < TOPOLOGY_MAX_NUMA_PER_SOCKET; i++) {
        if (obmmMetaData.customMeta.numaSizes[i] != NO_0) {
            algoResult.exportNumaInfos.push_back({lendNode, obmmMetaData.customMeta.exportSocket,
                                                  obmmMetaData.customMeta.exportNumaIds[i],
                                                  obmmMetaData.customMeta.numaSizes[i]});
            algoResult.importNumaInfos.push_back({req.importNodeId, obmmMetaData.customMeta.importSocket,
                                                  obmmMetaData.customMeta.importNumaIds[i],
                                                  obmmMetaData.customMeta.numaSizes[i]});
        }
    }
    algoResult.blockSize = RmCommonUtils::GetInstance().SizeByte2Mb(obmmMetaData.totalSize);
    isNormal = AfterConstructSingleNumaExportObj(exportLocalObmmMetaDatas, ubseMemNumaBorrowExportObj, obmmMetaData,
                                                 req, algoResult);
}
void BuildSingleNumaExportReq(UbseMemLocalObmmMetaData& obmmMetaData, std::string& lendNode, UbseMemNumaBorrowReq& req)
{
    req.name = std::string(obmmMetaData.customMeta.name);
    size_t pos = req.name.find_last_of('_');
    if (pos != std::string::npos) {
        req.name = req.name.substr(0, pos);
    }
    req.requestNodeId = std::string(obmmMetaData.customMeta.requestNodeId);
    req.importNodeId = std::string(obmmMetaData.customMeta.importNodeId);
    req.distance = MEM_DISTANCE_L0;
    req.udsInfo = {obmmMetaData.customMeta.uid, obmmMetaData.customMeta.gid,
                   static_cast<int>(obmmMetaData.customMeta.pid), obmmMetaData.customMeta.username};
    UBSE_LOG_INFO << MMI_LOG_INFO << "req.udsInfo.username=" << req.udsInfo.username << ", uid=" << req.udsInfo.uid
                  << ", gid=" << req.udsInfo.gid;
    uint64_t resourceMemSize = 0;
    for (int i = 0; i < TOPOLOGY_MAX_NUMA_PER_SOCKET; i++) {
        if (obmmMetaData.customMeta.numaSizes[i] != NO_0) {
            req.lenderLocs.push_back({lendNode, obmmMetaData.customMeta.exportNumaIds[i]});
            req.lenderSizes.push_back(obmmMetaData.customMeta.numaSizes[i]);
            bool isOverflow = false;
            resourceMemSize =
                RmCommonUtils::GetInstance().SafeAdd(resourceMemSize, obmmMetaData.customMeta.numaSizes[i], isOverflow);
            if (isOverflow) {
                UBSE_LOG_ERROR << MMI_LOG_INFO
                               << "Overflow occurred during addition. resourceMemSize=" << resourceMemSize
                               << ", numaSize=" << obmmMetaData.customMeta.numaSizes[i];
                return;
            }
        }
    }
    req.size = resourceMemSize;
    // 大数据的numa借用，这个值是没有的，直接用默认值，虚拟化的numa借用，这里才会有用
    if (req.udsInfo.pid == NO_0) {
        req.srcSocket = obmmMetaData.customMeta.importSocket;
        req.srcNuma = obmmMetaData.customMeta.importNumaIds[0];
    }
}

UbseResult ConstructShareImportObj(const std::vector<UbseMemLocalObmmMetaData>& importLocalObmmMetaDatas,
                                   UbseMemShareImportObjMap& normalImportObjMap)
{
    UbseResult ret = UBSE_OK;
    UbseMemShareImportObjMap abnormalImportObjMap{};
    std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData>> importObjMap{};
    GetBorrowObjMap(importLocalObmmMetaDatas, importObjMap);
    for (auto& importObjItem : importObjMap) {
        UbseMemShareBorrowImportObj mxeMemImportObj{};
        bool isNormal = true;
        ConstructSingleShareImportObj(importObjItem.second, mxeMemImportObj, isNormal);
        if (isNormal) {
            normalImportObjMap.emplace(importObjItem.first, mxeMemImportObj);
        } else {
            abnormalImportObjMap.emplace(importObjItem.first, mxeMemImportObj);
        }
    }
    normalImportObjMap.merge(ProcessAbnormalImportObjMap(abnormalImportObjMap));
    return ret;
}

static bool AfterConstructSingleShareImportObj(const std::vector<UbseMemLocalObmmMetaData>& importLocalObmmMetaDatas,
                                               UbseMemShareBorrowImportObj& mxeMemShareBorrowImportObj,
                                               const UbseMemLocalObmmMetaData& obmmMetaData,
                                               const UbseMemShareBorrowReq& req, const UbseMemAlgoResult& algoResult)
{
    std::vector<UbseMemObmmInfo> exportObmmInfo{};
    std::vector<UbseMemImportResult> importResults{};
    UbseMemImportStatus status{};
    for (size_t i = 0; i < importLocalObmmMetaDatas.size(); i++) {
        exportObmmInfo.push_back(
            {importLocalObmmMetaDatas[i].customMeta.exportMemid, importLocalObmmMetaDatas[i].obmmMemExportInfo});
        importResults.push_back({importLocalObmmMetaDatas[i].localMemId, -1});
        status.decoderResult.push_back(importLocalObmmMetaDatas[i].customMeta.decoderResult);
    }
    status.errCode = UBSE_OK;
    status.importResults = importResults;
    mxeMemShareBorrowImportObj.req = req;
    mxeMemShareBorrowImportObj.status = status;
    mxeMemShareBorrowImportObj.algoResult = algoResult;
    mxeMemShareBorrowImportObj.exportObmmInfo = exportObmmInfo;
    mxeMemShareBorrowImportObj.realExe = true; // 恢复的时候，只有为true的情况?
    mxeMemShareBorrowImportObj.importNodeId = obmmMetaData.localNodeId;
    mxeMemShareBorrowImportObj.shareAttr = {obmmMetaData.customMeta.uid, obmmMetaData.customMeta.gid,
                                            obmmMetaData.totalSize};
    return status.importResults.size() == obmmMetaData.customMeta.memidCount;
}

static bool AfterConstructSingleShareImportObjFromExportMetaData(
    const std::vector<UbseMemLocalObmmMetaData>& exportLocalObmmMetaDatas,
    UbseMemShareBorrowImportObj& mxeMemShareBorrowImportObj, const UbseMemLocalObmmMetaData& obmmMetaData,
    const UbseMemShareBorrowReq& req, const UbseMemAlgoResult& algoResult)
{
    std::vector<UbseMemObmmInfo> exportObmmInfo{};
    std::vector<UbseMemImportResult> importResults{};
    for (size_t i = 0; i < exportLocalObmmMetaDatas.size(); i++) {
        exportObmmInfo.push_back(
            {exportLocalObmmMetaDatas[i].customMeta.exportMemid, exportLocalObmmMetaDatas[i].obmmMemExportInfo});
        importResults.push_back({exportLocalObmmMetaDatas[i].localMemId, -1});
    }
    UbseMemImportStatus status{};
    status.errCode = UBSE_OK;
    status.importResults = importResults;
    mxeMemShareBorrowImportObj.req = req;
    mxeMemShareBorrowImportObj.status = status;
    mxeMemShareBorrowImportObj.algoResult = algoResult;
    mxeMemShareBorrowImportObj.exportObmmInfo = exportObmmInfo;
    mxeMemShareBorrowImportObj.realExe = false;
    mxeMemShareBorrowImportObj.importNodeId = obmmMetaData.localNodeId;
    mxeMemShareBorrowImportObj.shareAttr = {obmmMetaData.customMeta.uid, obmmMetaData.customMeta.gid,
                                            obmmMetaData.totalSize};
    return status.importResults.size() == obmmMetaData.customMeta.memidCount;
}

void ConstructSingleShareImportObj(const std::vector<UbseMemLocalObmmMetaData>& importLocalObmmMetaDatas,
                                   UbseMemShareBorrowImportObj& mxeMemShareBorrowImportObj, bool& isNormal)
{
    if (importLocalObmmMetaDatas.empty()) {
        isNormal = false;
        return;
    }
    UbseMemLocalObmmMetaData obmmMetaData = importLocalObmmMetaDatas[0];
    UbseMemShareBorrowReq req{};
    std::string exportNode = std::string(obmmMetaData.customMeta.exportNodeId);
    std::string importNode = std::string(obmmMetaData.customMeta.importNodeId);
    req.name = std::string(obmmMetaData.customMeta.name);
    req.requestNodeId = std::string(obmmMetaData.customMeta.requestNodeId); // 需要额外加
    req.baseNodeId = obmmMetaData.customMeta.requestNodeId;
    req.distance = MEM_DISTANCE_L0; // 使用默认值，不额外加，因为决策完成之后，这个就没有作用了
    req.udsInfo = {obmmMetaData.customMeta.uid, obmmMetaData.customMeta.gid,
                   static_cast<int>(obmmMetaData.customMeta.pid), obmmMetaData.customMeta.username};
    UBSE_LOG_INFO << MMI_LOG_INFO << "req.udsInfo.username=" << req.udsInfo.username << ", uid=" << req.udsInfo.uid
                  << ", gid=" << req.udsInfo.gid;
    UbseShmRegionDesc shmRegions{0};
    for (uint32_t i = 0; i < MAX_NODE_NUM; i++) {
        if (IsBitSet(obmmMetaData.customMeta.regionMask, i)) {
            UBSE_LOG_INFO << MMI_LOG_INFO << "region mask is " << obmmMetaData.customMeta.regionMask << ", index=" << i;
            shmRegions.nodeNum++;
            shmRegions.nodelist.push_back({i});
        }
    }
    int numaCount = 0;
    uint64_t resourceMemSize = 0;
    for (int i = 0; i < TOPOLOGY_MAX_NUMA_PER_SOCKET; i++) {
        if (obmmMetaData.customMeta.numaSizes[i] != NO_0) {
            numaCount++;
            UBSE_LOG_DEBUG << MMI_LOG_INFO << "Numa sizes is " << obmmMetaData.customMeta.numaSizes[i];
            bool isOverflow = false;
            resourceMemSize =
                RmCommonUtils::GetInstance().SafeAdd(resourceMemSize, obmmMetaData.customMeta.numaSizes[i], isOverflow);
            if (isOverflow) {
                UBSE_LOG_ERROR << MMI_LOG_INFO
                               << "Overflow occurred during addition. resourceMemSize=" << resourceMemSize
                               << ", numaSize=" << obmmMetaData.customMeta.numaSizes[i];
                return;
            }
        }
    }
    req.size = resourceMemSize;
    req.shmRegion = shmRegions;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "req size is " << resourceMemSize;

    if (memcpy_s(req.usrInfo, UBSE_MAX_USR_INFO_LEN, obmmMetaData.customMeta.usrInfo, UBSE_MAX_USR_INFO_LEN) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "MemCopy fail when copy usrInfo, name is " << req.name << ", usrInfo="
                       << reinterpret_cast<char*>(
                              obmmMetaData.customMeta.usrInfo); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        isNormal = false;
        return;
    }
    CopyUbMemPrivData(req.ubseMemPrivData, obmmMetaData.privData);

    UbseMemAlgoResult algoResult{};
    algoResult.attachSocketId = obmmMetaData.customMeta.attachSocket;
    for (int i = 0; i < numaCount; i++) {
        algoResult.exportNumaInfos.push_back({exportNode, obmmMetaData.customMeta.exportSocket,
                                              obmmMetaData.customMeta.exportNumaIds[i],
                                              obmmMetaData.customMeta.numaSizes[i]});
        algoResult.importNumaInfos.push_back(
            {importNode, obmmMetaData.customMeta.importSocket, obmmMetaData.customMeta.importNumaIds[i],
             obmmMetaData.customMeta.numaSizes[i], obmmMetaData.customMeta.portId, obmmMetaData.customMeta.chipId});
    }
    isNormal = AfterConstructSingleShareImportObj(importLocalObmmMetaDatas, mxeMemShareBorrowImportObj, obmmMetaData,
                                                  req, algoResult);
}

UbseResult ConstructShareImportObjFromExportMetaData(
    const std::vector<UbseMemLocalObmmMetaData>& exportLocalObmmMetaDatas, UbseMemShareImportObjMap& importObjMap)
{
    UbseResult ret = UBSE_OK;
    std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData>> exportObjMap{};
    GetBorrowObjMap(exportLocalObmmMetaDatas, exportObjMap);
    for (auto& exportObjItem : exportObjMap) {
        UbseMemShareBorrowImportObj mxeMemImportObj{};
        bool isNormal = true;
        ConstructSingleShareImportObjFromExportMetaData(exportObjItem.second, mxeMemImportObj, isNormal);
        if (isNormal) {
            importObjMap.emplace(exportObjItem.first, mxeMemImportObj);
        }
    }
    return ret;
}

void ConstructSingleShareImportObjFromExportMetaData(
    const std::vector<UbseMemLocalObmmMetaData>& exportLocalObmmMetaDatas,
    UbseMemShareBorrowImportObj& mxeMemShareBorrowImportObj, bool& isNormal)
{
    if (exportLocalObmmMetaDatas.empty()) {
        isNormal = false;
        return;
    }
    UbseMemLocalObmmMetaData obmmMetaData = exportLocalObmmMetaDatas[0];
    if (obmmMetaData.usedPidCount == 0) {
        isNormal = false;
        return;
    }
    UbseMemShareBorrowReq req{};
    std::string exportNode = std::string(obmmMetaData.customMeta.exportNodeId);
    int numaCount;
    AssignReqValue(obmmMetaData, req, numaCount);

    if (memcpy_s(req.usrInfo, UBSE_MAX_USR_INFO_LEN, obmmMetaData.customMeta.usrInfo, UBSE_MAX_USR_INFO_LEN) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "MemCopy fail when copy usrInfo, name is " << req.name << ", usrInfo="
                       << reinterpret_cast<char*>(
                              obmmMetaData.customMeta.usrInfo); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        isNormal = false;
        return;
    }
    CopyUbMemPrivData(req.ubseMemPrivData, obmmMetaData.privData);

    UbseMemAlgoResult algoResult{};
    algoResult.attachSocketId = obmmMetaData.customMeta.attachSocket;
    for (int i = 0; i < numaCount; i++) {
        algoResult.exportNumaInfos.push_back({exportNode, obmmMetaData.customMeta.exportSocket,
                                              obmmMetaData.customMeta.exportNumaIds[i],
                                              obmmMetaData.customMeta.numaSizes[i]});
        algoResult.importNumaInfos.push_back(
            {exportNode, obmmMetaData.customMeta.exportSocket, obmmMetaData.customMeta.exportNumaIds[i],
             obmmMetaData.customMeta.numaSizes[i], obmmMetaData.customMeta.portId, obmmMetaData.customMeta.chipId});
    }
    req.requestNodeId = obmmMetaData.localNodeId;
    algoResult.blockSize = RmCommonUtils::GetInstance().SizeByte2Mb(obmmMetaData.totalSize);
    isNormal = AfterConstructSingleShareImportObjFromExportMetaData(
        exportLocalObmmMetaDatas, mxeMemShareBorrowImportObj, obmmMetaData, req, algoResult);
}

UbseResult ConstructShareExportObj(const std::vector<UbseMemLocalObmmMetaData>& exportLocalObmmMetaDatas,
                                   UbseMemShareExportObjMap& normalExportObjMap)
{
    UbseResult ret = UBSE_OK;
    UbseMemShareExportObjMap abnormalExportObjMap{};
    std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData>> exportObjMap{};
    GetBorrowObjMap(exportLocalObmmMetaDatas, exportObjMap);
    for (auto& exportItem : exportObjMap) {
        UbseMemShareBorrowExportObj mxeMemNumaBorrowExportObj{};
        bool isNormal = true;
        ConstructSingleShareExportObj(exportItem.second, mxeMemNumaBorrowExportObj, isNormal);
        if (isNormal) {
            normalExportObjMap.emplace(exportItem.first, mxeMemNumaBorrowExportObj);
        } else {
            abnormalExportObjMap.emplace(exportItem.first, mxeMemNumaBorrowExportObj);
        }
    }
    normalExportObjMap.merge(ProcessAbnormalExportObjMap(abnormalExportObjMap));
    return ret;
}

void AssignReqValue(UbseMemLocalObmmMetaData obmmMetaData, UbseMemShareBorrowReq& req, int& numaCount)
{
    req.name = std::string(obmmMetaData.customMeta.name);
    req.requestNodeId = std::string(obmmMetaData.customMeta.requestNodeId); // 需要额外加
    req.baseNodeId = obmmMetaData.localNodeId;
    req.distance = MEM_DISTANCE_L0; // 使用默认值，不额外加，因为决策完成之后，这个就没有作用了
    req.udsInfo = {obmmMetaData.customMeta.uid, obmmMetaData.customMeta.gid,
                   static_cast<int>(obmmMetaData.customMeta.pid), obmmMetaData.customMeta.username};
    UBSE_LOG_INFO << MMI_LOG_INFO << "req.udsInfo.username=" << req.udsInfo.username << ", uid=" << req.udsInfo.uid
                  << ", gid=" << req.udsInfo.gid;
    UbseShmRegionDesc shmRegions{};
    shmRegions.nodeNum = 0;
    for (int i = 0; i < MAX_NODE_NUM; i++) {
        if (IsBitSet(obmmMetaData.customMeta.regionMask, i)) {
            UBSE_LOG_INFO << MMI_LOG_INFO << "Region mask is " << obmmMetaData.customMeta.regionMask << ", index=" << i;
            shmRegions.nodeNum++;
            UbseNodeInfo mxeNodeInfo{};
            mxeNodeInfo.index = i;
            shmRegions.nodelist.push_back(mxeNodeInfo);
        }
    }
    numaCount = 0;
    for (int i = 0; i < TOPOLOGY_MAX_NUMA_PER_SOCKET; i++) {
        if (obmmMetaData.customMeta.numaSizes[i] != NO_0) {
            numaCount++;
        }
    }
    req.size = obmmMetaData.customMeta.requestSize;
    req.shmRegion = shmRegions;
}
void ConstructSingleShareExportObj(const std::vector<UbseMemLocalObmmMetaData>& exportLocalObmmMetaDatas,
                                   UbseMemShareBorrowExportObj& mxeMemShareBorrowExportObj, bool& isNormal)
{
    if (exportLocalObmmMetaDatas.empty()) {
        isNormal = false;
        return;
    }
    UbseMemLocalObmmMetaData obmmMetaData = exportLocalObmmMetaDatas[0];
    UbseMemShareBorrowReq req{};
    req.shmAnonymous = obmmMetaData.customMeta.anonymous == 1; // 1 mean anonymous
    int numaCount;
    AssignReqValue(obmmMetaData, req, numaCount);

    if (memcpy_s(req.usrInfo, UBSE_MAX_USR_INFO_LEN, obmmMetaData.customMeta.usrInfo, UBSE_MAX_USR_INFO_LEN) != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "MemCopy fail when copy usrInfo, name is " << req.name << ", usrInfo="
                       << reinterpret_cast<char*>(
                              obmmMetaData.customMeta.usrInfo); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        isNormal = false;
        return;
    }
    CopyUbMemPrivData(req.ubseMemPrivData, obmmMetaData.privData);

    std::string lendNode = std::string(obmmMetaData.customMeta.exportNodeId);
    // 构造算法结果
    UbseMemAlgoResult algoResult{};
    for (int i = 0; i < numaCount; i++) {
        algoResult.exportNumaInfos.push_back({lendNode, obmmMetaData.customMeta.exportSocket,
                                              obmmMetaData.customMeta.exportNumaIds[i],
                                              obmmMetaData.customMeta.numaSizes[i]});
    }
    algoResult.blockSize = RmCommonUtils::GetInstance().SizeByte2Mb(obmmMetaData.totalSize);
    std::vector<UbseMemObmmInfo> exportObmmInfo{};
    for (size_t i = 0; i < exportLocalObmmMetaDatas.size(); i++) {
        exportObmmInfo.push_back(
            {exportLocalObmmMetaDatas[i].localMemId, exportLocalObmmMetaDatas[i].obmmMemExportInfo});
    }
    UbseMemExportStatus status{};
    status.errCode = UBSE_OK;
    status.exportObmmInfo = exportObmmInfo;

    mxeMemShareBorrowExportObj.req = req;
    mxeMemShareBorrowExportObj.status = status;
    mxeMemShareBorrowExportObj.algoResult = algoResult;
    isNormal = status.exportObmmInfo.size() == obmmMetaData.customMeta.memidCount;
}

UbseResult ConstructAddrImportObj(const std::vector<UbseMemLocalObmmMetaData>& importLocalObmmMetaDatas,
                                  UbseMemAddrImportObjMap& normalImportObjMap)
{
    UbseResult ret = UBSE_OK;
    UbseMemAddrImportObjMap abnormalImportObjMap{};
    std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData>> importObjMap{};
    GetBorrowObjMap(importLocalObmmMetaDatas, importObjMap);
    // 芯片表项碎片，导入一半的时候，进程挂掉了，从obmm获取数据前等3s,保证单次最大1G执行完成
    for (auto& importObjItem : importObjMap) {
        UbseMemAddrBorrowImportObj mxeMemImportObj{};
        bool isNormal = true;
        ConstructSingleAddrImportObj(importObjItem.second, mxeMemImportObj, isNormal);
        if (isNormal) {
            normalImportObjMap.emplace(importObjItem.first, mxeMemImportObj);
        } else {
            abnormalImportObjMap.emplace(importObjItem.first, mxeMemImportObj);
        }
    }
    normalImportObjMap.merge(ProcessAbnormalAddrImportObjMap(abnormalImportObjMap));
    return ret;
}

void SetAddrReqInfo(UbseMemAddrBorrowReq& req, const UbseMemLocalObmmMetaData obmmMetaData)
{
    req.name = std::string(obmmMetaData.customMeta.name);
    req.requestNodeId = std::string(obmmMetaData.customMeta.requestNodeId); // 需要额外加
    req.importNodeId = std::string(obmmMetaData.customMeta.importNodeId);
    req.srcSocket = obmmMetaData.customMeta.importSocket;
    req.srcNuma = obmmMetaData.customMeta.importNumaIds[0];
    req.importPid = obmmMetaData.customMeta.srcPid;
    req.exportNodeId = std::string(obmmMetaData.customMeta.exportNodeId);
    req.exportPid = obmmMetaData.customMeta.dstPid;
    req.dstNuma = obmmMetaData.customMeta.dstNuma;
    req.dstSocket = obmmMetaData.customMeta.dstSocket;
    req.udsInfo = {obmmMetaData.customMeta.uid, obmmMetaData.customMeta.gid,
                   static_cast<int>(obmmMetaData.customMeta.pid), obmmMetaData.customMeta.username};
}

void ConstructSingleAddrImportObj(const std::vector<UbseMemLocalObmmMetaData>& importLocalObmmMetaDatas,
                                  UbseMemAddrBorrowImportObj& mxeMemAddrBorrowImportObj, bool& isNormal)
{
    if (importLocalObmmMetaDatas.empty()) {
        isNormal = false;
        return;
    }
    UbseMemLocalObmmMetaData obmmMetaData = importLocalObmmMetaDatas[0];
    UbseMemAddrBorrowReq req{};
    SetAddrReqInfo(req, obmmMetaData);
    UbseMemAlgoResult algoResult{};
    algoResult.attachSocketId = obmmMetaData.customMeta.attachSocket;
    for (int i = 0; i < TOPOLOGY_MAX_NUMA_PER_SOCKET; i++) {
        if (obmmMetaData.customMeta.numaSizes[i] != NO_0) {
            algoResult.exportNumaInfos.push_back({req.exportNodeId, obmmMetaData.customMeta.exportSocket,
                                                  obmmMetaData.customMeta.exportNumaIds[i],
                                                  obmmMetaData.customMeta.numaSizes[i]});
            algoResult.importNumaInfos.push_back(
                {req.importNodeId, obmmMetaData.customMeta.importSocket, obmmMetaData.customMeta.importNumaIds[i],
                 obmmMetaData.customMeta.numaSizes[i], obmmMetaData.customMeta.portId, obmmMetaData.customMeta.chipId});
        }
    }
    std::vector<UbseMemAddrInfo> addrList{};
    std::vector<UbseMemImportResult> importResults{};
    UbseMemImportStatus status{};
    std::vector<UbseMemObmmInfo> exportObmmInfo{};
    for (const auto& item : importLocalObmmMetaDatas) {
        addrList.push_back({item.customMeta.virAddr, item.totalSize});
        exportObmmInfo.push_back({item.customMeta.exportMemid, item.obmmMemExportInfo});
        importResults.push_back({item.localMemId, item.remoteNumaId});
        MemInstanceInnerAddrBorrow::GetInstance().AddAddrRemoteNuma(item.remoteNumaId);
        status.decoderResult.push_back(item.customMeta.decoderResult);
    }
    status.errCode = UBSE_OK;
    status.importResults = importResults;
    req.exportAddrList = addrList;
    mxeMemAddrBorrowImportObj.req = req;
    mxeMemAddrBorrowImportObj.algoResult = algoResult;
    mxeMemAddrBorrowImportObj.status = status;
    mxeMemAddrBorrowImportObj.exportObmmInfo = exportObmmInfo;
    isNormal = (addrList.size() == obmmMetaData.customMeta.memidCount);
}

UbseResult ConstructAddrExportObj(const std::vector<UbseMemLocalObmmMetaData>& exportLocalObmmMetaDatas,
                                  UbseMemAddrExportObjMap& normalExportObjMap)
{
    UbseResult ret = UBSE_OK;
    UbseMemAddrExportObjMap abnormalExportObjMap{};
    std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData>> exportObjMap{};
    GetBorrowObjMap(exportLocalObmmMetaDatas, exportObjMap);
    // 芯片表项碎片，导入一半的时候，进程挂掉了，从obmm获取数据前等3s,保证单次最大1G执行完成
    for (auto& exportItem : exportObjMap) {
        UbseMemAddrBorrowExportObj mxeMemAddrBorrowExportObj{};
        bool isNormal = true;
        ConstructSingleAddrExportObj(exportItem.second, mxeMemAddrBorrowExportObj, isNormal);
        if (isNormal) {
            normalExportObjMap.emplace(exportItem.first, mxeMemAddrBorrowExportObj);
        } else {
            abnormalExportObjMap.emplace(exportItem.first, mxeMemAddrBorrowExportObj);
        }
    }
    normalExportObjMap.merge(ProcessAbnormalExportObjMap(abnormalExportObjMap));
    return ret;
}

void SetAddrReqByMetaData(UbseMemAddrBorrowReq& req, const UbseMemLocalObmmMetaData& obmmMetaData)
{
    req.name = std::string(obmmMetaData.customMeta.name);
    size_t pos = req.name.find_last_of('_');
    if (pos != std::string::npos) {
        req.name = req.name.substr(0, pos);
    }
    req.requestNodeId = std::string(obmmMetaData.customMeta.requestNodeId); // 需要额外加
    req.importNodeId = std::string(obmmMetaData.customMeta.importNodeId);
    req.srcSocket = obmmMetaData.customMeta.importSocket;
    req.srcNuma = obmmMetaData.customMeta.importNumaIds[0];
    req.importPid = obmmMetaData.customMeta.srcPid;
    req.exportNodeId = std::string(obmmMetaData.customMeta.exportNodeId);
    req.exportPid = obmmMetaData.customMeta.dstPid;
    req.dstNuma = obmmMetaData.customMeta.dstNuma;
    req.dstSocket = obmmMetaData.customMeta.dstSocket;
    req.udsInfo = {obmmMetaData.customMeta.uid, obmmMetaData.customMeta.gid,
                   static_cast<int>(obmmMetaData.customMeta.pid), obmmMetaData.customMeta.username};
}

void ConstructSingleAddrExportObj(const std::vector<UbseMemLocalObmmMetaData>& exportLocalObmmMetaDatas,
                                  UbseMemAddrBorrowExportObj& mxeMemAddrBorrowExportObj, bool& isNormal)
{
    if (exportLocalObmmMetaDatas.empty()) {
        isNormal = false;
        return;
    }
    UbseMemLocalObmmMetaData obmmMetaData = exportLocalObmmMetaDatas[0];
    UbseMemAddrBorrowReq req{};
    req.name = std::string(obmmMetaData.customMeta.name);
    size_t pos = req.name.find_last_of('_');
    if (pos != std::string::npos) {
        req.name = req.name.substr(0, pos);
    }
    SetAddrReqByMetaData(req, obmmMetaData);
    std::vector<UbseMemAddrInfo> addrList{};
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos{};
    UbseMemExportStatus status{};
    std::vector<UbseMemObmmInfo> exportObmmInfo{};
    for (const auto& item : exportLocalObmmMetaDatas) {
        addrList.push_back({item.customMeta.virAddr, item.totalSize});
        exportObmmInfo.push_back({item.localMemId, item.obmmMemExportInfo});
    }
    status.exportObmmInfo = exportObmmInfo;
    status.errCode = UBSE_OK;
    req.exportAddrList = addrList;
    mxeMemAddrBorrowExportObj.req = req;
    mxeMemAddrBorrowExportObj.status = status;

    UbseMemAlgoResult algoResult{};
    algoResult.attachSocketId = obmmMetaData.customMeta.attachSocket;
    for (int i = 0; i < TOPOLOGY_MAX_NUMA_PER_SOCKET; i++) {
        if (obmmMetaData.customMeta.numaSizes[i] != NO_0) {
            algoResult.exportNumaInfos.push_back({req.exportNodeId, obmmMetaData.customMeta.exportSocket,
                                                  obmmMetaData.customMeta.exportNumaIds[i],
                                                  obmmMetaData.customMeta.numaSizes[i]});
            algoResult.importNumaInfos.push_back({req.importNodeId, obmmMetaData.customMeta.importSocket,
                                                  obmmMetaData.customMeta.importNumaIds[i],
                                                  obmmMetaData.customMeta.numaSizes[i]});
        }
    }
    mxeMemAddrBorrowExportObj.algoResult = algoResult;
    // 算法决策结果，在导出端不进行恢复，只在导入端进行恢复
    isNormal = (addrList.size() == obmmMetaData.customMeta.memidCount);
}

void GetBorrowObjMap(const std::vector<UbseMemLocalObmmMetaData>& localObmmMetaDatas,
                     std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData>>& borrowObjMap)
{
    for (auto& localObmmMetaDatasItem : localObmmMetaDatas) {
        std::string name = localObmmMetaDatasItem.customMeta.name;
        if (borrowObjMap.find(name) != borrowObjMap.end()) {
            borrowObjMap[name].push_back(localObmmMetaDatasItem);
        } else {
            borrowObjMap[name] = {localObmmMetaDatasItem};
        }
    }
}

static void ClassifyLocalObmmMetaData(LocalObmmMetaData& localObmmMetaData,
                                      const std::vector<UbseMemLocalObmmMetaData>& ubseMemLocalObmmMetaDatas)
{
    for (size_t i = 0; i < ubseMemLocalObmmMetaDatas.size(); i++) {
        if (ubseMemLocalObmmMetaDatas[i].memIdType == static_cast<uint8_t>(UbseObmmType::IMPORT) &&
            ubseMemLocalObmmMetaDatas[i].customMeta.type == static_cast<int8_t>(UbseBorrowType::FD_BORROW)) {
            localObmmMetaData.FdImportMetaData.push_back(ubseMemLocalObmmMetaDatas[i]);
        }
        if (ubseMemLocalObmmMetaDatas[i].memIdType == static_cast<uint8_t>(UbseObmmType::IMPORT) &&
            ubseMemLocalObmmMetaDatas[i].customMeta.type == static_cast<int8_t>(UbseBorrowType::NUMA_BORROW)) {
            localObmmMetaData.NumaImportMetaData.push_back(ubseMemLocalObmmMetaDatas[i]);
        }
        if (ubseMemLocalObmmMetaDatas[i].memIdType == static_cast<uint8_t>(UbseObmmType::IMPORT) &&
            ubseMemLocalObmmMetaDatas[i].customMeta.type == static_cast<int8_t>(UbseBorrowType::SHARE_BORROW)) {
            localObmmMetaData.ShmImportMetaData.push_back(ubseMemLocalObmmMetaDatas[i]);
        }
        if (ubseMemLocalObmmMetaDatas[i].memIdType == static_cast<uint8_t>(UbseObmmType::IMPORT) &&
            ubseMemLocalObmmMetaDatas[i].customMeta.type == static_cast<int8_t>(UbseBorrowType::ADDR_BORROW)) {
            localObmmMetaData.AddrImportMetaData.push_back(ubseMemLocalObmmMetaDatas[i]);
        }

        if (ubseMemLocalObmmMetaDatas[i].memIdType == static_cast<uint8_t>(UbseObmmType::EXPORT) &&
            ubseMemLocalObmmMetaDatas[i].customMeta.type == static_cast<int8_t>(UbseBorrowType::FD_BORROW)) {
            localObmmMetaData.FdExportMetaData.push_back(ubseMemLocalObmmMetaDatas[i]);
        }
        if (ubseMemLocalObmmMetaDatas[i].memIdType == static_cast<uint8_t>(UbseObmmType::EXPORT) &&
            ubseMemLocalObmmMetaDatas[i].customMeta.type == static_cast<int8_t>(UbseBorrowType::NUMA_BORROW)) {
            localObmmMetaData.NumaExportMetaData.push_back(ubseMemLocalObmmMetaDatas[i]);
        }
        if (ubseMemLocalObmmMetaDatas[i].memIdType == static_cast<uint8_t>(UbseObmmType::EXPORT) &&
            ubseMemLocalObmmMetaDatas[i].customMeta.type == static_cast<int8_t>(UbseBorrowType::SHARE_BORROW)) {
            localObmmMetaData.ShmExportMetaData.push_back(ubseMemLocalObmmMetaDatas[i]);
        }
        if (ubseMemLocalObmmMetaDatas[i].memIdType == static_cast<uint8_t>(UbseObmmType::EXPORT) &&
            ubseMemLocalObmmMetaDatas[i].customMeta.type == static_cast<int8_t>(UbseBorrowType::ADDR_BORROW)) {
            localObmmMetaData.AddrExportMetaData.push_back(ubseMemLocalObmmMetaDatas[i]);
        }
    }
}

UbseResult GetLocalObmmMeta(std::vector<UbseMemLocalObmmMetaData>& allObmmDatas, LocalObmmMetaData& localObmmMetaData)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "GetLocalObmmMeta start.";
    allObmmDatas.clear();
    bool lastPage = true;
    auto ret = RmObmmMetaRestore::ReadAgentLocalObmmMetaData(0, allObmmDatas, lastPage);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ReadAgentLocalObmmMetaData failed.";
        return ret;
    }
    ClassifyLocalObmmMetaData(localObmmMetaData, allObmmDatas);
    UBSE_LOG_INFO << MMI_LOG_INFO << "GetLocalObmmMeta end, size=" << allObmmDatas.size() << ", "
                  << RmCommonUtils::GetInstance().TranStructToStr(localObmmMetaData);
    return UBSE_OK;
}

UbseMemAddrImportObjMap ProcessAbnormalAddrImportObjMap(UbseMemAddrImportObjMap& importObjMap)
{
    UbseMemAddrImportObjMap faultObjMap{};
    if (importObjMap.empty()) {
        UBSE_LOG_DEBUG << MMI_LOG_INFO << "ImportObjMap is empty, not need process";
        return faultObjMap;
    }
    for (auto& item : importObjMap) {
        auto& importObj = item.second;
        auto timeoutMs = RmObmmExecutor::CalculateUnImportTimeout(importObj.algoResult.blockSize);
        auto& importResults = importObj.status.importResults;
        bool hasFault = false;
        for (size_t i = 0; i < importResults.size(); i++) {
            auto ret = RmObmmExecutor::GetInstance().ObmmUnImport(importResults[i].memId, timeoutMs);
            if (UBSE_RESULT_FAIL(ret)) {
                UBSE_LOG_ERROR << MMI_LOG_INFO
                               << "Obmm unimport failed, mark as faulty, memid=" << importResults[i].memId
                               << ", errCode=" << ret;
                importObj.errorCode = ret;
                importObj.status.errCode = ret;
                hasFault = true;
                break;
            }
            UBSE_LOG_DEBUG << MMI_LOG_INFO << "Obmm unImport memid success, memid=" << importResults[i].memId;
            MemInstanceInnerAddrBorrow::GetInstance().DeleteAddrRemoteNuma(importResults[i].numaId);
        }
        if (hasFault) {
            UBSE_LOG_WARN << MMI_LOG_INFO << "Move faulty obj to fault map, name=" << item.first;
            faultObjMap.emplace(item.first, importObj);
        }
    }
    return faultObjMap;
}
} // namespace ubse::mmi::restore