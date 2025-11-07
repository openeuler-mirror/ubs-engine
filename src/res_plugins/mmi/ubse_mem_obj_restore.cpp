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
#include "ubse_obmm_meta_restore.h"

namespace ubse::mmi {
void RmMemObjRestore::ConstructSingleFdImportObj(
    const std::vector<UbseMemLocalObmmMetaData> &fdImportLocalObmmMetaDatas,
    UbseMemFdBorrowImportObj &ubseMemFdBorrowImportObj, bool &isNormal)
{
    UbseMemLocalObmmMetaData obmmMetaData = fdImportLocalObmmMetaDatas[0];
    UbseMemFdBorrowReq req{};
    req.name = std::string(obmmMetaData.customMeta.name);
    req.timestamp = obmmMetaData.customMeta.timeStamp;
    req.requestNodeId = std::string(obmmMetaData.customMeta.requestNodeId); // 需要额外加
    req.importNodeId = obmmMetaData.localNodeId;
    req.distance = MEM_DISTANCE_L0; // 使用默认值，不额外加，因为决策完成之后，这个就没有作用了
    req.udsInfo = {obmmMetaData.customMeta.uid, obmmMetaData.customMeta.gid,
                   static_cast<int>(obmmMetaData.customMeta.pid)};
    std::string lendNode = std::string(obmmMetaData.customMeta.exportNodeId);
    int numaCount = 0;
    uint64_t resourceMemSize = 0;
    for (int i = 0; i < UBSE_MEM_MAX_EXPORT_NUMA_SIZE; i++) {
        if (obmmMetaData.customMeta.numaSizes[i] != NO_0) {
            numaCount++;
            req.lenderLocs.push_back({lendNode, obmmMetaData.customMeta.exportNumaIds[i]});
            req.lenderSizes.push_back(obmmMetaData.customMeta.numaSizes[i]);
            resourceMemSize = resourceMemSize + obmmMetaData.customMeta.numaSizes[i];
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
        algoResult.importNumaInfos.push_back({req.importNodeId, obmmMetaData.customMeta.importSocket,
                                              obmmMetaData.customMeta.importNumaIds[i],
                                              obmmMetaData.customMeta.numaSizes[i]});
    }
    std::vector<UbseMemObmmInfo> exportObmmInfo{};
    std::vector<UbseMemImportResult> importResults{};
    for (int i = 0; i < fdImportLocalObmmMetaDatas.size(); i++) {
        exportObmmInfo.push_back(
            {fdImportLocalObmmMetaDatas[i].customMeta.exportMemid, fdImportLocalObmmMetaDatas[i].obmmMemExportInfo});
        importResults.push_back({fdImportLocalObmmMetaDatas[i].localMemId, -1});
    }
    UbseMemImportStatus status{};
    status.errCode = UBSE_OK;
    status.importResults = importResults;

    ubseMemFdBorrowImportObj.req = req;
    ubseMemFdBorrowImportObj.status = status;
    ubseMemFdBorrowImportObj.algoResult = algoResult;
    ubseMemFdBorrowImportObj.exportObmmInfo = exportObmmInfo;
    isNormal = status.importResults.size() == obmmMetaData.customMeta.memidCount;
}

UbseResult RmMemObjRestore::ConstructFdImportObj(
    const std::vector<UbseMemLocalObmmMetaData> &fdImportLocalObmmMetaDatas,
    UbseMemFdImportObjMap &normalFdImportObjMap)
{
    UbseMemFdImportObjMap abnormalFdImportObjMap{};
    UbseResult ret = UBSE_OK;
    std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData> > fdBorrowImportObjMap{};
    for (auto &localObmmMetaDatasItem : fdImportLocalObmmMetaDatas) {
        std::string name = localObmmMetaDatasItem.customMeta.name;
        if (fdBorrowImportObjMap.find(name) != fdBorrowImportObjMap.end()) {
            fdBorrowImportObjMap[name].push_back(localObmmMetaDatasItem);
        } else {
            fdBorrowImportObjMap[name] = {localObmmMetaDatasItem};
        }
    }
    for (auto &fdBorrowImportItem : fdBorrowImportObjMap) {
        UbseMemFdBorrowImportObj ubseMemFdBorrowImportObj{};
        bool isNormal = true;
        ConstructSingleFdImportObj(fdBorrowImportItem.second, ubseMemFdBorrowImportObj, isNormal);
        if (isNormal) {
            normalFdImportObjMap.emplace(fdBorrowImportItem.first, ubseMemFdBorrowImportObj);
        } else {
            abnormalFdImportObjMap.emplace(fdBorrowImportItem.first, ubseMemFdBorrowImportObj);
        }
    }
    ret = ProcessAbnormalImportObjMap(abnormalFdImportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "ProcessAbnormalImportObjMap failed, name and memId info = "
            << GetNameAndMemIdFromImportObjMap(abnormalFdImportObjMap);
        return ret;
    }
    return ret;
}

void RmMemObjRestore::ConstructSingleFdExportObj(const std::vector<UbseMemLocalObmmMetaData> &exportLocalObmmMetaDatas,
                                                 UbseMemFdBorrowExportObj &ubseMemFdBorrowExportObj, bool &isNormal)
{
    UbseMemLocalObmmMetaData obmmMetaData = exportLocalObmmMetaDatas[0];
    UbseMemFdBorrowReq req{};
    req.name = std::string(obmmMetaData.customMeta.name);
    req.timestamp = obmmMetaData.customMeta.timeStamp;
    req.requestNodeId = obmmMetaData.localNodeId; // 需要额外加
    req.importNodeId = obmmMetaData.localNodeId;
    req.requestNodeId = std::string(obmmMetaData.customMeta.requestNodeId);
    req.importNodeId = std::string(obmmMetaData.customMeta.importNodeId);
    req.distance = MEM_DISTANCE_L0; // 使用默认值，不额外加，因为决策完成之后，这个就没有作用了
    req.udsInfo = {obmmMetaData.customMeta.uid, obmmMetaData.customMeta.gid,
                   static_cast<int>(obmmMetaData.customMeta.pid)};
    std::string lendNode = std::string(obmmMetaData.customMeta.exportNodeId);
    int numaCount = 0;
    uint64_t resourceMemSize = 0;
    for (int i = 0; i < UBSE_MEM_MAX_EXPORT_NUMA_SIZE; i++) {
        if (obmmMetaData.customMeta.numaSizes[i] != NO_0) {
            numaCount++;
            req.lenderLocs.push_back({lendNode, obmmMetaData.customMeta.exportNumaIds[i]});
            req.lenderSizes.push_back(obmmMetaData.customMeta.numaSizes[i]);
            resourceMemSize = resourceMemSize + obmmMetaData.customMeta.numaSizes[i];
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
    for (int i = 0; i < exportLocalObmmMetaDatas.size(); i++) {
        exportObmmInfo.push_back(
            {exportLocalObmmMetaDatas[i].localMemId, exportLocalObmmMetaDatas[i].obmmMemExportInfo});
    }
    UbseMemExportStatus status{};
    status.errCode = UBSE_OK;
    status.exportObmmInfo = exportObmmInfo;

    ubseMemFdBorrowExportObj.req = req;
    ubseMemFdBorrowExportObj.status = status;
    ubseMemFdBorrowExportObj.algoResult = algoResult;
    isNormal = status.exportObmmInfo.size() == obmmMetaData.customMeta.memidCount;
}

UbseResult RmMemObjRestore::ConstructFdExportObj(const std::vector<UbseMemLocalObmmMetaData> &exportLocalObmmMetaDatas,
                                                 UbseMemFdExportObjMap &normalFdExportObjMap)
{
    UbseResult ret = UBSE_OK;
    UbseMemFdExportObjMap abnormalFdExportObjMap{};
    std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData> > exportObjMap{};
    for (auto &localObmmMetaDatasItem : exportLocalObmmMetaDatas) {
        std::string name = localObmmMetaDatasItem.customMeta.name;
        if (exportObjMap.find(name) != exportObjMap.end()) {
            exportObjMap[name].push_back(localObmmMetaDatasItem);
        } else {
            exportObjMap[name] = {localObmmMetaDatasItem};
        }
    }
    // 芯片表项碎片，导入一半的时候，进程挂掉了，从obmm获取数据前等3s,保证单次最大1G执行完成
    for (auto &exportItem : exportObjMap) {
        UbseMemFdBorrowExportObj ubseMemFdBorrowExportObj{};
        bool isNormal = true;
        ConstructSingleFdExportObj(exportItem.second, ubseMemFdBorrowExportObj, isNormal);
        if (isNormal) {
            normalFdExportObjMap.emplace(exportItem.first, ubseMemFdBorrowExportObj);
        } else {
            abnormalFdExportObjMap.emplace(exportItem.first, ubseMemFdBorrowExportObj);
        }
    }
    ret = ProcessAbnormalExportObjMap(abnormalFdExportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "ProcessAbnormalExportObjMap failed, name and memId info ="
            << GetNameAndMemIdFromExportObjMap(abnormalFdExportObjMap);
        return ret;
    }
    return ret;
}

void RmMemObjRestore::ConstructSingleNumaImportObj(
    const std::vector<UbseMemLocalObmmMetaData> &importLocalObmmMetaDatas,
    UbseMemNumaBorrowImportObj &ubseMemNumaBorrowImportObj, bool &isNormal)
{
    UbseMemLocalObmmMetaData obmmMetaData = importLocalObmmMetaDatas[0];
    UbseMemNumaBorrowReq req{};
    req.name = std::string(obmmMetaData.customMeta.name);
    req.timestamp = obmmMetaData.customMeta.timeStamp;
    req.requestNodeId = std::string(obmmMetaData.customMeta.requestNodeId); // 需要额外加
    req.importNodeId = std::string(obmmMetaData.customMeta.importNodeId);
    req.distance = MEM_DISTANCE_L0; // 使用默认值，不额外加，因为决策完成之后，这个就没有作用了
    req.udsInfo = {obmmMetaData.customMeta.uid, obmmMetaData.customMeta.gid,
                   static_cast<int>(obmmMetaData.customMeta.pid)};
    std::string lendNode = std::string(obmmMetaData.customMeta.exportNodeId);
    int numaCount = 0;
    size_t resourceMemSize = 0;
    for (int i = 0; i < UBSE_MEM_MAX_EXPORT_NUMA_SIZE; i++) {
        if (obmmMetaData.customMeta.numaSizes[i] != NO_0) {
            numaCount++;
            req.lenderLocs.push_back({lendNode, obmmMetaData.customMeta.exportNumaIds[i]});
            req.lenderSizes.push_back(obmmMetaData.customMeta.numaSizes[i]);
            resourceMemSize = resourceMemSize + obmmMetaData.customMeta.numaSizes[i];
        }
    }
    req.size = resourceMemSize;
    // 大数据的numa借用，这个值是没有的，直接用默认值，虚拟化的numa借用，这里才会有用
    if (req.udsInfo.pid == NO_0) {
        req.srcSocket = obmmMetaData.customMeta.importSocket;
        req.srcNuma = obmmMetaData.customMeta.importNumaIds[0];
    }
    // 构造其他的东西
    UbseMemAlgoResult algoResult{};
    algoResult.attachSocketId = obmmMetaData.customMeta.attachSocket;
    for (int i = 0; i < numaCount; i++) {
        algoResult.exportNumaInfos.push_back({lendNode, obmmMetaData.customMeta.exportSocket,
                                              obmmMetaData.customMeta.exportNumaIds[i],
                                              obmmMetaData.customMeta.numaSizes[i]});
        algoResult.importNumaInfos.push_back({req.importNodeId, obmmMetaData.customMeta.importSocket,
                                              obmmMetaData.customMeta.importNumaIds[i],
                                              obmmMetaData.customMeta.numaSizes[i]});
    }
    std::vector<UbseMemObmmInfo> exportObmmInfo{};
    std::vector<UbseMemImportResult> importResults{};
    for (int i = 0; i < importLocalObmmMetaDatas.size(); i++) {
        exportObmmInfo.push_back(
            {importLocalObmmMetaDatas[i].customMeta.exportMemid, importLocalObmmMetaDatas[i].obmmMemExportInfo});
        importResults.push_back({importLocalObmmMetaDatas[i].localMemId, -1});
    }
    UbseMemImportStatus status{};
    status.errCode = UBSE_OK;
    status.importResults = importResults;

    ubseMemNumaBorrowImportObj.req = req;
    ubseMemNumaBorrowImportObj.status = status;
    ubseMemNumaBorrowImportObj.algoResult = algoResult;
    ubseMemNumaBorrowImportObj.exportObmmInfo = exportObmmInfo;
    isNormal = status.importResults.size() == obmmMetaData.customMeta.memidCount;
}

UbseResult RmMemObjRestore::ConstructNumaImportObj(
    const std::vector<UbseMemLocalObmmMetaData> &importLocalObmmMetaDatas, UbseMemNumaImportObjMap &normalImportObjMap)
{
    UbseResult ret = UBSE_OK;
    UbseMemNumaImportObjMap abnormalImportObjMap{};
    std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData> > importObjMap{};
    GetBorrowObjMap(importLocalObmmMetaDatas, importObjMap);
    // 芯片表项碎片，导入一半的时候，进程挂掉了，从obmm获取数据前等3s,保证单次最大1G执行完成
    for (auto &importObjItem : importObjMap) {
        UbseMemNumaBorrowImportObj ubseMemImportObj{};
        bool isNormal = true;
        ConstructSingleNumaImportObj(importObjItem.second, ubseMemImportObj, isNormal);
        if (isNormal) {
            normalImportObjMap.emplace(importObjItem.first, ubseMemImportObj);
        } else {
            abnormalImportObjMap.emplace(importObjItem.first, ubseMemImportObj);
        }
    }
    ret = ProcessAbnormalImportObjMap(abnormalImportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "ProcessAbnormalImportObjMap failed, name and memId info ="
            << GetNameAndMemIdFromImportObjMap(abnormalImportObjMap);
        return ret;
    }
    return ret;
}

UbseResult RmMemObjRestore::ConstructNumaExportObj(
    const std::vector<UbseMemLocalObmmMetaData> &exportLocalObmmMetaDatas, UbseMemNumaExportObjMap &normalExportObjMap)
{
    UbseResult ret = UBSE_OK;
    UbseMemNumaExportObjMap abnormalExportObjMap{};
    std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData> > exportObjMap{};
    GetBorrowObjMap(exportLocalObmmMetaDatas, exportObjMap);
    for (auto &exportItem : exportObjMap) {
        UbseMemNumaBorrowExportObj ubseMemNumaBorrowExportObj{};
        bool isNormal = true;
        ConstructSingleNumaExportObj(exportItem.second, ubseMemNumaBorrowExportObj, isNormal);
        if (isNormal) {
            normalExportObjMap.emplace(exportItem.first, ubseMemNumaBorrowExportObj);
        } else {
            abnormalExportObjMap.emplace(exportItem.first, ubseMemNumaBorrowExportObj);
        }
    }
    ret = ProcessAbnormalExportObjMap(abnormalExportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "ProcessAbnormalExportObjMap failed, name and memId info ="
            << GetNameAndMemIdFromExportObjMap(abnormalExportObjMap);
        return ret;
    }
    return ret;
}

void RmMemObjRestore::ConstructSingleNumaExportObj(
    const std::vector<UbseMemLocalObmmMetaData> &exportLocalObmmMetaDatas,
    UbseMemNumaBorrowExportObj &ubseMemNumaBorrowExportObj, bool &isNormal)
{
    UbseMemLocalObmmMetaData obmmMetaData = exportLocalObmmMetaDatas[0];
    UbseMemNumaBorrowReq req{};
    req.name = std::string(obmmMetaData.customMeta.name);
    req.timestamp = obmmMetaData.customMeta.timeStamp;
    req.requestNodeId = std::string(obmmMetaData.customMeta.requestNodeId);
    req.importNodeId = std::string(obmmMetaData.customMeta.importNodeId);
    req.distance = MEM_DISTANCE_L0;
    req.udsInfo = {obmmMetaData.customMeta.uid, obmmMetaData.customMeta.gid,
                   static_cast<int>(obmmMetaData.customMeta.pid)};
    std::string lendNode = std::string(obmmMetaData.customMeta.exportNodeId);
    int numaCount = 0;
    size_t resourceMemSize = 0;
    for (int i = 0; i < UBSE_MEM_MAX_EXPORT_NUMA_SIZE; i++) {
        if (obmmMetaData.customMeta.numaSizes[i] != NO_0) {
            numaCount++;
            req.lenderLocs.push_back({lendNode, obmmMetaData.customMeta.exportNumaIds[i]});
            req.lenderSizes.push_back(obmmMetaData.customMeta.numaSizes[i]);
            resourceMemSize = resourceMemSize + obmmMetaData.customMeta.numaSizes[i];
        }
    }
    req.size = resourceMemSize;
    // 大数据的numa借用，这个值是没有的，直接用默认值，虚拟化的numa借用，这里才会有用
    if (req.udsInfo.pid == NO_0) {
        req.srcSocket = obmmMetaData.customMeta.importSocket;
        req.srcNuma = obmmMetaData.customMeta.importNumaIds[0];
    }
    // 构造其他的东西
    UbseMemAlgoResult algoResult{};
    for (int i = 0; i < numaCount; i++) {
        algoResult.exportNumaInfos.push_back({lendNode, obmmMetaData.customMeta.exportSocket,
                                              obmmMetaData.customMeta.exportNumaIds[i],
                                              obmmMetaData.customMeta.numaSizes[i]});
        algoResult.importNumaInfos.push_back({req.importNodeId, obmmMetaData.customMeta.importSocket,
                                              obmmMetaData.customMeta.importNumaIds[i],
                                              obmmMetaData.customMeta.numaSizes[i]});
    }

    std::vector<UbseMemObmmInfo> exportObmmInfo{};
    for (int i = 0; i < exportLocalObmmMetaDatas.size(); i++) {
        exportObmmInfo.push_back(
            {exportLocalObmmMetaDatas[i].localMemId, exportLocalObmmMetaDatas[i].obmmMemExportInfo});
    }
    UbseMemExportStatus status{};
    status.errCode = UBSE_OK;
    status.exportObmmInfo = exportObmmInfo;

    ubseMemNumaBorrowExportObj.req = req;
    ubseMemNumaBorrowExportObj.status = status;
    ubseMemNumaBorrowExportObj.algoResult = algoResult;
    isNormal = status.exportObmmInfo.size() == obmmMetaData.customMeta.memidCount;
}

void RmMemObjRestore::GetBorrowObjMap(
    const std::vector<UbseMemLocalObmmMetaData> &localObmmMetaDatas,
    std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData> > &borrowObjMap)
{
    for (auto &localObmmMetaDatasItem : localObmmMetaDatas) {
        std::string name = localObmmMetaDatasItem.customMeta.name;
        if (borrowObjMap.find(name) != borrowObjMap.end()) {
            borrowObjMap[name].push_back(localObmmMetaDatasItem);
        } else {
            borrowObjMap[name] = {localObmmMetaDatasItem};
        }
    }
}

static void ClassifyLocalObmmMetaData(LocalObmmMetaData &localObmmMetaData,
                                      const std::vector<UbseMemLocalObmmMetaData> &ubseMemLocalObmmMetaDatas)
{
    for (int i = 0; i < ubseMemLocalObmmMetaDatas.size(); i++) {
        if (ubseMemLocalObmmMetaDatas[i].memIdType == NO_0 &&
            ubseMemLocalObmmMetaDatas[i].customMeta.type == static_cast<int8_t>(UbseBorrowType::FD_BORROW)) {
            localObmmMetaData.FdImportMetaData.push_back(ubseMemLocalObmmMetaDatas[i]);
        }
        if (ubseMemLocalObmmMetaDatas[i].memIdType == NO_0 &&
            ubseMemLocalObmmMetaDatas[i].customMeta.type == static_cast<int8_t>(UbseBorrowType::NUMA_BORROW)) {
            localObmmMetaData.NumaImportMetaData.push_back(ubseMemLocalObmmMetaDatas[i]);
        }
        if (ubseMemLocalObmmMetaDatas[i].memIdType == NO_1 &&
            ubseMemLocalObmmMetaDatas[i].customMeta.type == static_cast<int8_t>(UbseBorrowType::FD_BORROW)) {
            localObmmMetaData.FdExportMetaData.push_back(ubseMemLocalObmmMetaDatas[i]);
        }
        if (ubseMemLocalObmmMetaDatas[i].memIdType == NO_1 &&
            ubseMemLocalObmmMetaDatas[i].customMeta.type == static_cast<int8_t>(UbseBorrowType::NUMA_BORROW)) {
            localObmmMetaData.NumaExportMetaData.push_back(ubseMemLocalObmmMetaDatas[i]);
        }
    }
}

UbseResult RmMemObjRestore::GetLocalObmmMeta(std::vector<UbseMemLocalObmmMetaData> &allObmmDatas,
                                             LocalObmmMetaData &localObmmMetaData)
{
    bool lastPage = true;
    auto ret = RmObmmMetaRestore::GetInstance().ReadAgentLocalObmmMetaData(0, allObmmDatas, lastPage);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "ReadAgentLocalObmmMetaData failed.";
        return ret;
    }
    ClassifyLocalObmmMetaData(localObmmMetaData, allObmmDatas);
    return UBSE_OK;
}
} // namespace ubse::mmi