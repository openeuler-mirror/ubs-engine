/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "mem_container_msg.h"

#include "msg_utils.h"
#include "vm_serial_util.h"

namespace vm {
VmResult MemContainerPidMemInfoInputMsg::Serialize()
{
    VmSerialization out;
    out << pidParamForC_.srcNid;
    if (pidParamForC_.pid_size > NO_2048) {
        return VM_ERROR_INVAL;
    }
    out << pidParamForC_.pid_size;
    for (size_t i = 0; i < pidParamForC_.pid_size; i++) {
        out << pidParamForC_.pids[i];
    }

    if (!out.Check()) {
        return VM_ERROR;
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    // Setting it to True will automatically release memory, leaving it to be manually released externally.
    mOutputRawDataOwned = false;
    return VM_OK;
}

VmResult MemContainerPidMemInfoInputMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);

    if (!in.Check()) {
        return VM_ERROR;
    }

    std::string tempSrcNid;
    in >> tempSrcNid;
    auto ret = StringToC(pidParamForC_.srcNid, tempSrcNid, sizeof(pidParamForC_.srcNid));
    if (ret != VM_OK) {
        return ret;
    }
    in >> pidParamForC_.pid_size;
    if (pidParamForC_.pid_size > NO_2048) {
        return VM_ERROR;
    }
    for (size_t i = 0; i < pidParamForC_.pid_size; i++) {
        in >> pidParamForC_.pids[i];
    }

    if (!in.Check()) {
        return VM_ERROR;
    }

    return VM_OK;
}

SrcMemoryBorrowParam MemContainerPidMemInfoInputMsg::GetBorrowParam()
{
    SrcMemoryBorrowParam borrowParam{};
    borrowParam.srcNid = pidParamForC_.srcNid;
    return borrowParam;
}

std::vector<pid_t> MemContainerPidMemInfoInputMsg::GetPids()
{
    std::vector<pid_t> pids;
    size_t size = pidParamForC_.pid_size;
    for (size_t i = 0; i < size; i++) {
        pids.push_back(pidParamForC_.pids[i]);
    }
    return pids;
}

VmResult MemContainerPidMemInfoOutputMsg::Serialize()
{
    VmSerialization out;
    auto size = pidInfos_.size();
    out << size;
    for (auto &info : pidInfos_) {
        out << info.pid;
        out << info.localUsedMem;
        auto numaIdsCount = info.localNumaIds.size();
        out << numaIdsCount;
        for (auto &localNumaId : info.localNumaIds) {
            out << localNumaId;
        }
        out << info.remoteUsedMem;
    }

    if (!out.Check()) {
        return VM_ERROR;
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    // Setting it to True will automatically release memory, leaving it to be manually released externally.
    mOutputRawDataOwned = false;
    return VM_OK;
}

VmResult MemContainerPidMemInfoOutputMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    size_t vecSize = 0;
    in >> vecSize;
    if (!in.Check()) {
        return VM_ERROR;
    }

    pidInfos_.clear();
    for (size_t i = 0; i < vecSize; ++i) {
        PidInfo info{};
        in >> info.pid;
        in >> info.localUsedMem;
        size_t numaSize = 0;
        in >> numaSize;
        for (size_t j = 0; j < numaSize; ++j) {
            uint16_t localNumaId;
            in >> localNumaId;
            info.localNumaIds.push_back(localNumaId);
        }
        in >> info.remoteUsedMem;
        pidInfos_.push_back(info);
    }

    if (!in.Check()) {
        return VM_ERROR;
    }

    return VM_OK;
}

std::vector<pid_mem_info_for_c> MemContainerPidMemInfoOutputMsg::GetPidInfos()
{
    std::vector<pid_mem_info_for_c> pidInfo{};
    for (auto &info : pidInfos_) {
        pid_mem_info_for_c data{};
        data.pid = info.pid;
        data.localUsedMem = info.localUsedMem;
        const size_t size = NO_64;
        data.localNumaCount = info.localNumaIds.size();
        if (info.localNumaIds.size() <= size) {
            std::copy(info.localNumaIds.begin(), info.localNumaIds.end(), data.localNumaIds);
        }

        data.remoteUsedMem = info.remoteUsedMem;
        pidInfo.push_back(data);
    }
    return pidInfo;
}

WaterMark UpdateWaterLineForCInputMsg::GetWaterMark()
{
    WaterMark waterMark{};
    waterMark.highWaterMark = waterMark_.highWaterMark;
    waterMark.lowWaterMark = waterMark_.lowWaterMark;
    return waterMark;
}

VmResult UpdateWaterLineForCInputMsg::Serialize()
{
    VmSerialization out;
    out << waterMark_.highWaterMark;
    out << waterMark_.lowWaterMark;
    if (!out.Check()) {
        return VM_ERROR;
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    // Setting it to True will automatically release memory, leaving it to be manually released externally.
    mOutputRawDataOwned = false;
    return VM_OK;
}

VmResult UpdateWaterLineForCInputMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    if (!in.Check()) {
        return VM_ERROR;
    }

    in >> waterMark_.highWaterMark;
    in >> waterMark_.lowWaterMark;
    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}

container_id_list_for_c ContainerIdListForCInputMsg::GetContainerPidInfo()
{
    container_id_list_for_c containerIdListForC{};
    auto maxSize = sizeof(containerIdListForC_) / sizeof(containerIdListForC_.containerId[0]);
    size_t listSize = std::min(containerIdListForC_.containerIdSize, maxSize);
    for (size_t i = 0; i < listSize; ++i) {
        auto ret = StringToC(containerIdListForC.containerId[i], std::string(containerIdListForC_.containerId[i]),
                             sizeof(containerIdListForC.containerId[i]));
        if (ret != VM_OK) {
            return {};
        }
    }
    containerIdListForC.containerIdSize = containerIdListForC_.containerIdSize;
    return containerIdListForC;
}

VmResult ContainerIdListForCInputMsg::Serialize()
{
    VmSerialization out;
    auto maxSize = sizeof(containerIdListForC_) / sizeof(containerIdListForC_.containerId[0]);
    size_t listSize = std::min(containerIdListForC_.containerIdSize, maxSize);
    out << listSize;

    for (size_t i = 0; i < listSize; ++i) {
        out << std::string(containerIdListForC_.containerId[i]);
    }

    if (!out.Check()) {
        return VM_ERROR;
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = false;

    return VM_OK;
}

VmResult ContainerIdListForCInputMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }

    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    in >> containerIdListForC_.containerIdSize;
    if (!in.Check()) {
        return VM_ERROR;
    }
    auto maxSize = sizeof(containerIdListForC_) / sizeof(containerIdListForC_.containerId[0]);
    containerIdListForC_.containerIdSize = std::min(containerIdListForC_.containerIdSize, maxSize);

    for (size_t i = 0; i < containerIdListForC_.containerIdSize; ++i) {
        std::string tempContainerId;
        in >> tempContainerId;
        auto ret = StringToC(containerIdListForC_.containerId[i], tempContainerId,
                             sizeof(containerIdListForC_.containerId[i]));
        if (ret != VM_OK) {
            return ret;
        }
    }
    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}

std::vector<container_pid_info_for_c> ContainerPidsForCInputMsg::GetContainerPidInfos()
{
    std::vector<container_pid_info_for_c> containerPidInfos{};
    for (auto &info : containerPidInfos_) {
        container_pid_info_for_c data{};
        data.containerId = info.containerId;
        const size_t maxSize = sizeof(info.pids) / sizeof(info.pids[0]);
        data.pidsCount = std::min(info.pidsCount, maxSize);
        for (size_t i = 0; i < data.pidsCount; ++i) {
            data.pids[i] = info.pids[i];
        }

        containerPidInfos.push_back(data);
    }
    return containerPidInfos;
}

VmResult ContainerPidsForCInputMsg::Serialize()
{
    VmSerialization out;
    auto size = containerPidInfos_.size();
    out << size;
    for (auto &info : containerPidInfos_) {
        out << info.containerId;
        size = std::min(info.pidsCount, sizeof(info.pids) / sizeof(info.pids[0]));
        out << size;

        for (size_t j = 0; j < size; ++j) {
            out << info.pids[j];
        }
    }
    if (!out.Check()) {
        return VM_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = false;
    return VM_OK;
}

VmResult ContainerPidsForCInputMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    size_t vecSize = 0;
    in >> vecSize;
    if (!in.Check()) {
        return VM_ERROR;
    }

    containerPidInfos_.clear();
    for (size_t i = 0; i < vecSize; ++i) {
        container_pid_info_for_c info{};

        // Read ContainerId
        in >> info.containerId;

        // Read PIDs based on PidsCount
        size_t pidsCount = 0;
        in >> pidsCount;

        // Ensure we don't exceed the array size
        pidsCount = std::min(pidsCount, static_cast<size_t>(NO_2048));

        for (size_t j = 0; j < pidsCount; ++j) {
            in >> info.pids[j];
        }

        info.pidsCount = pidsCount;
        containerPidInfos_.push_back(info);
    }

    if (!in.Check()) {
        for (size_t i = 0; i < containerPidInfos_.size(); i++) {
            SafeDeleteArray(containerPidInfos_[i].containerId);
        }
        containerPidInfos_.clear();
        return VM_ERROR;
    }

    return VM_OK;
}

VmResult MemContainerWaterLineMemBorrowInputMsg::Serialize()
{
    VmSerialization out;
    out << memBorrowRequest_.borrowParam.srcNid;
    size_t size = memBorrowRequest_.borrowParam.srcLocationsSize;
    if (size > NO_16) {
        return VM_ERROR;
    }
    out << size;

    for (size_t i = 0; i < size; i++) {
        out << memBorrowRequest_.borrowParam.srcLocations[i].socketId;
        out << memBorrowRequest_.borrowParam.srcLocations[i].numaId;
    }
    size = memBorrowRequest_.borrowSizesSize;
    if (size > NO_64) {
        return VM_ERROR;
    }
    out << size;
    for (size_t i = 0; i < size; i++) {
        out << memBorrowRequest_.borrowSizes[i];
    }
    out << memBorrowRequest_.waterMark.highWaterMark;
    out << memBorrowRequest_.waterMark.lowWaterMark;

    if (!out.Check()) {
        return VM_ERROR;
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    // Setting it to True will automatically release memory, leaving it to be manually released externally.
    mOutputRawDataOwned = false;
    return VM_OK;
}

VmResult MemContainerWaterLineMemBorrowInputMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);

    if (!in.Check()) {
        return VM_ERROR;
    }
    std::string tempSrcNid;
    in >> tempSrcNid;
    auto ret =
        StringToC(memBorrowRequest_.borrowParam.srcNid, tempSrcNid, sizeof(memBorrowRequest_.borrowParam.srcNid));
    if (ret != VM_OK) {
        return ret;
    }
    size_t size = 0;
    in >> size;
    if (size > NO_16) {
        return VM_ERROR;
    }
    memBorrowRequest_.borrowParam.srcLocationsSize = size;
    for (size_t i = 0; i < size; i++) {
        in >> memBorrowRequest_.borrowParam.srcLocations[i].socketId;
        in >> memBorrowRequest_.borrowParam.srcLocations[i].numaId;
    }
    in >> size;
    if (size > NO_64) {
        return VM_ERROR;
    }
    memBorrowRequest_.borrowSizesSize = size;
    for (size_t i = 0; i < size; i++) {
        in >> memBorrowRequest_.borrowSizes[i];
    }
    in >> memBorrowRequest_.waterMark.highWaterMark;
    in >> memBorrowRequest_.waterMark.lowWaterMark;

    if (!in.Check()) {
        return VM_ERROR;
    }

    return VM_OK;
}

VmResult MemContainerWaterLineMemBorrowInputMsg::GetParams(NodeLocInfo &nodeLocInfo, std::vector<uint64_t> &borrowSizes,
                                                           WaterMark &waterMark)
{
    // Get nodeLocInfo
    std::string hostId = memBorrowRequest_.borrowParam.srcNid;
    if (hostId == "") {
        return VM_ERROR;
    }
    nodeLocInfo.hostId = hostId;
    nodeLocInfo.socketId = -1;
    nodeLocInfo.numaId = -1;
    if (memBorrowRequest_.borrowParam.srcLocationsSize == 1) {
        nodeLocInfo.socketId = memBorrowRequest_.borrowParam.srcLocations[0].socketId;
        nodeLocInfo.numaId = memBorrowRequest_.borrowParam.srcLocations[0].numaId;
    }
    // Get borrowSizes
    size_t size = memBorrowRequest_.borrowSizesSize;
    if (size > NO_64) {
        return VM_ERROR;
    }
    borrowSizes.clear();
    for (auto i = 0; i < size; i++) {
        borrowSizes.emplace_back(memBorrowRequest_.borrowSizes[i]);
    }
    // Get WaterMark
    waterMark.highWaterMark = memBorrowRequest_.waterMark.highWaterMark;
    waterMark.lowWaterMark = memBorrowRequest_.waterMark.lowWaterMark;
    return VM_OK;
}

VmResult MemContainerWaterLineMemBorrowOutputMsg::Serialize()
{
    VmSerialization out;
    size_t size = borrowIds_.size();
    out << size;
    for (auto &borrowId : borrowIds_) {
        out << borrowId;
    }

    if (!out.Check()) {
        return VM_ERROR;
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    // Setting it to True will automatically release memory, leaving it to be manually released externally.
    mOutputRawDataOwned = false;
    return VM_OK;
}
VmResult MemContainerWaterLineMemBorrowOutputMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);

    if (!in.Check()) {
        return VM_ERROR;
    }

    size_t size = 0;
    in >> size;
    borrowIds_.clear();
    for (size_t i = 0; i < size; i++) {
        std::string borrowId;
        in >> borrowId;
        borrowIds_.emplace_back(borrowId);
    }

    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult MemContainerWaterLineMemMigrateInputMsg::Serialize()
{
    VmSerialization out;
    out << memMigrateRequest_.borrowParam.srcNid;
    size_t size = memMigrateRequest_.borrowParam.srcLocationsSize;
    if (size >
        sizeof(memMigrateRequest_.borrowParam.srcLocations) / sizeof(memMigrateRequest_.borrowParam.srcLocations[0])) {
        return VM_ERROR;
    }
    out << size;

    for (size_t i = 0; i < size; i++) {
        out << memMigrateRequest_.borrowParam.srcLocations[i].socketId;
        out << memMigrateRequest_.borrowParam.srcLocations[i].numaId;
    }
    size = memMigrateRequest_.borrowIdsSize;
    if (size > sizeof(memMigrateRequest_.borrowIds) / sizeof(memMigrateRequest_.borrowIds[0])) {
        return VM_ERROR;
    }
    out << size;
    for (size_t i = 0; i < size; i++) {
        out << memMigrateRequest_.borrowIds[i];
    }
    size = memMigrateRequest_.containerParamSize;
    if (size > sizeof(memMigrateRequest_.containerParam) / sizeof(memMigrateRequest_.containerParam[0])) {
        return VM_ERROR;
    }
    out << size;
    for (size_t i = 0; i < size; i++) {
        out << memMigrateRequest_.containerParam[i].pid;
        out << memMigrateRequest_.containerParam[i].ratio;
    }

    if (!out.Check()) {
        return VM_ERROR;
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    // Setting it to True will automatically release memory, leaving it to be manually released externally.
    mOutputRawDataOwned = false;
    return VM_OK;
}
VmResult MemContainerWaterLineMemMigrateInputMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);

    if (!in.Check()) {
        return VM_ERROR;
    }
    std::string tmpNid;
    in >> tmpNid;
    auto ret = StringToC(memMigrateRequest_.borrowParam.srcNid, tmpNid, sizeof(memMigrateRequest_.borrowParam.srcNid));
    if (ret != VM_OK) {
        return ret;
    }
    size_t size = 0;
    in >> size;
    if (size >
        sizeof(memMigrateRequest_.borrowParam.srcLocations) / sizeof(memMigrateRequest_.borrowParam.srcLocations[0])) {
        return VM_ERROR;
    }
    memMigrateRequest_.borrowParam.srcLocationsSize = size;
    for (size_t i = 0; i < size; i++) {
        in >> memMigrateRequest_.borrowParam.srcLocations[i].socketId;
        in >> memMigrateRequest_.borrowParam.srcLocations[i].numaId;
    }
    in >> size;
    if (size > sizeof(memMigrateRequest_.borrowIds) / sizeof(memMigrateRequest_.borrowIds[0])) {
        return VM_ERROR;
    }
    memMigrateRequest_.borrowIdsSize = size;
    for (size_t i = 0; i < size; i++) {
        in >> tmpNid;
        if (StringToC(memMigrateRequest_.borrowIds[i], tmpNid, sizeof(memMigrateRequest_.borrowIds[i])) != VM_OK) {
            return VM_ERROR;
        }
    }
    in >> size;
    if (size > sizeof(memMigrateRequest_.containerParam) / sizeof(memMigrateRequest_.containerParam[0])) {
        return VM_ERROR;
    }
    memMigrateRequest_.containerParamSize = size;
    for (size_t i = 0; i < size; i++) {
        in >> memMigrateRequest_.containerParam[i].pid;
        in >> memMigrateRequest_.containerParam[i].ratio;
    }

    if (!in.Check()) {
        return VM_ERROR;
    }

    return VM_OK;
}

VmResult MemContainerWaterLineMemMigrateInputMsg::GetParams(NodeLocInfo &nodeLocInfo,
                                                            std::unordered_set<std::string> &borrowIdSet,
                                                            std::vector<VMPresetParam> &vmPresetParamList)
{
    // Get nodeLocInfo
    std::string hostId = memMigrateRequest_.borrowParam.srcNid;
    if (hostId == "") {
        return VM_ERROR;
    }
    nodeLocInfo.hostId = hostId;
    nodeLocInfo.socketId = -1;
    nodeLocInfo.numaId = -1;
    if (memMigrateRequest_.borrowParam.srcLocationsSize == 1) {
        nodeLocInfo.socketId = memMigrateRequest_.borrowParam.srcLocations[0].socketId;
        nodeLocInfo.numaId = memMigrateRequest_.borrowParam.srcLocations[0].numaId;
    }
    // Get borrowIdSet
    if (memMigrateRequest_.borrowIdsSize >
        (sizeof(memMigrateRequest_.borrowIds) / sizeof(memMigrateRequest_.borrowIds[0]))) {
        return VM_ERROR;
    }
    borrowIdSet.clear();
    for (size_t i = 0; i < memMigrateRequest_.borrowIdsSize; i++) {
        borrowIdSet.emplace(memMigrateRequest_.borrowIds[i]);
    }
    // Get vmPresetParamList
    auto size = memMigrateRequest_.containerParamSize;
    if (size > (sizeof(memMigrateRequest_.containerParam) / sizeof(memMigrateRequest_.containerParam[0]))) {
        return VM_ERROR;
    }
    vmPresetParamList.clear();
    for (size_t i = 0; i < size; i++) {
        VMPresetParam vmPresetParam{};
        vmPresetParam.pid = memMigrateRequest_.containerParam[i].pid;
        vmPresetParam.ratio = memMigrateRequest_.containerParam[i].ratio;
        vmPresetParamList.emplace_back(vmPresetParam);
    }
    return VM_OK;
}

VmResult MemContainerWaterLineMemReturnInputMsg::Serialize()
{
    VmSerialization out;
    out << memReturnRequest_.borrowParam.srcNid;
    size_t size = memReturnRequest_.borrowParam.srcLocationsSize;
    if (size >
        sizeof(memReturnRequest_.borrowParam.srcLocations) / sizeof(memReturnRequest_.borrowParam.srcLocations[0])) {
        return VM_ERROR;
    }
    out << size;

    for (size_t i = 0; i < size; i++) {
        out << memReturnRequest_.borrowParam.srcLocations[i].socketId;
        out << memReturnRequest_.borrowParam.srcLocations[i].numaId;
    }
    size = memReturnRequest_.borrowIdsSize;
    if (size > sizeof(memReturnRequest_.borrowIds) / sizeof(memReturnRequest_.borrowIds[0])) {
        return VM_ERROR;
    }
    out << size;
    for (size_t i = 0; i < size; i++) {
        out << memReturnRequest_.borrowIds[i];
    }
    size = memReturnRequest_.pidsSize;
    if (size > sizeof(memReturnRequest_.pids) / sizeof(memReturnRequest_.pids[0])) {
        return VM_ERROR;
    }
    out << size;
    for (size_t i = 0; i < size; i++) {
        out << memReturnRequest_.pids[i];
    }

    if (!out.Check()) {
        return VM_ERROR;
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    // Setting it to True will automatically release memory, leaving it to be manually released externally.
    mOutputRawDataOwned = false;
    return VM_OK;
}
VmResult MemContainerWaterLineMemReturnInputMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);

    if (!in.Check()) {
        return VM_ERROR;
    }
    std::string tmpNid;
    in >> tmpNid;
    auto ret = StringToC(memReturnRequest_.borrowParam.srcNid, tmpNid, sizeof(memReturnRequest_.borrowParam.srcNid));
    if (ret != VM_OK) {
        return ret;
    }
    size_t size = 0;
    in >> size;
    if (size >
        sizeof(memReturnRequest_.borrowParam.srcLocations) / sizeof(memReturnRequest_.borrowParam.srcLocations[0])) {
        return VM_ERROR;
    }
    memReturnRequest_.borrowParam.srcLocationsSize = size;
    for (size_t i = 0; i < size; i++) {
        in >> memReturnRequest_.borrowParam.srcLocations[i].socketId;
        in >> memReturnRequest_.borrowParam.srcLocations[i].numaId;
    }
    in >> size;
    if (size > sizeof(memReturnRequest_.borrowIds) / sizeof(memReturnRequest_.borrowIds[0])) {
        return VM_ERROR;
    }
    memReturnRequest_.borrowIdsSize = size;
    for (size_t i = 0; i < size; i++) {
        in >> tmpNid;
        ret = StringToC(memReturnRequest_.borrowIds[i], tmpNid, sizeof(memReturnRequest_.borrowIds[i]));
        if (ret != VM_OK) {
            return ret;
        }
    }
    in >> size;
    if (size > sizeof(memReturnRequest_.pids) / sizeof(memReturnRequest_.pids[0])) {
        return VM_ERROR;
    }
    memReturnRequest_.pidsSize = size;
    for (size_t i = 0; i < size; i++) {
        in >> memReturnRequest_.pids[i];
    }

    if (!in.Check()) {
        return VM_ERROR;
    }

    return VM_OK;
}

VmResult MemContainerWaterLineMemReturnInputMsg::GetParams(NodeLocInfo &nodeLocInfo,
                                                           std::vector<std::string> &borrowIds,
                                                           std::vector<pid_t> &pids)
{
    // Get nodeLocInfo
    std::string hostId = memReturnRequest_.borrowParam.srcNid;
    if (hostId == "") {
        return VM_ERROR;
    }
    nodeLocInfo.hostId = hostId;
    nodeLocInfo.socketId = -1;
    nodeLocInfo.numaId = -1;
    if (memReturnRequest_.borrowParam.srcLocationsSize == 1) {
        nodeLocInfo.socketId = memReturnRequest_.borrowParam.srcLocations[0].socketId;
        nodeLocInfo.numaId = memReturnRequest_.borrowParam.srcLocations[0].numaId;
    }
    // Get borrowIds
    if (memReturnRequest_.borrowIdsSize >
        (sizeof(memReturnRequest_.borrowIds) / sizeof(memReturnRequest_.borrowIds[0]))) {
        return VM_ERROR;
    }
    borrowIds.clear();
    for (size_t i = 0; i < memReturnRequest_.borrowIdsSize; i++) {
        borrowIds.emplace_back(memReturnRequest_.borrowIds[i]);
    }
    // Get pids
    if (memReturnRequest_.pidsSize > (sizeof(memReturnRequest_.pids) / sizeof(memReturnRequest_.pids[0]))) {
        return VM_ERROR;
    }
    pids.clear();
    for (size_t i = 0; i < memReturnRequest_.pidsSize; i++) {
        pids.emplace_back(memReturnRequest_.pids[i]);
    }
    return VM_OK;
}
} // namespace vm
