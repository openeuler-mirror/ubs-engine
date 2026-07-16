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

#include "mem_fragmentation_msg.h"

#include <securec.h>
#include "msg_utils.h"
#include "vm_serial_util.h"

namespace vm {

VmResult MemBorrowExecuteResultMsg::Serialize()
{
    if (memBorrowResultC_.borrow_ids_size > MAX_BORROW_ID_COUNT) {
        return VM_ERROR_INVAL;
    }

    if (memBorrowResultC_.present_numa_ids_size > MAX_BORROW_ID_COUNT) {
        return VM_ERROR_INVAL;
    }

    for (uint32_t i = 0; i < memBorrowResultC_.borrow_ids_size; ++i) {
        if (strlen(memBorrowResultC_.borrow_ids_ptr[i]) >= MAX_BORROW_ID_LENGTH) {
            return VM_ERROR_INVAL;
        }
    }

    if (strlen(memBorrowResultC_.task_id) >= MEM_TASK_ID_MAX) {
        return VM_ERROR_INVAL;
    }

    VmSerialization out;
    out << memBorrowResultC_.borrow_ids_size;
    for (uint32_t i = 0; i < memBorrowResultC_.borrow_ids_size; ++i) {
        out << std::string(memBorrowResultC_.borrow_ids_ptr[i]);
    }

    out << memBorrowResultC_.present_numa_ids_size;
    for (uint32_t i = 0; i < memBorrowResultC_.present_numa_ids_size; ++i) {
        out << memBorrowResultC_.present_numa_ids_ptr[i];
    }

    out << std::string(memBorrowResultC_.task_id);
    if (!out.Check()) {
        return VM_ERROR;
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = false;
    return VM_OK;
}

VmResult MemBorrowExecuteResultMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);

    uint32_t borrow_ids_size = 0;
    in >> borrow_ids_size;
    memBorrowResultC_.borrow_ids_size = borrow_ids_size;

    for (uint32_t i = 0; i < borrow_ids_size; ++i) {
        std::string borrow_id_str;
        in >> borrow_id_str;
        if (!in.Check()) {
            return VM_ERROR;
        }
        auto ret = StringToC(memBorrowResultC_.borrow_ids_ptr[i], borrow_id_str, MAX_BORROW_ID_LENGTH);
        if (ret != VM_OK) {
            return ret;
        }
    }

    uint32_t present_numa_ids_size = 0;
    in >> present_numa_ids_size;
    memBorrowResultC_.present_numa_ids_size = present_numa_ids_size;
    for (uint32_t i = 0; i < present_numa_ids_size; ++i) {
        in >> memBorrowResultC_.present_numa_ids_ptr[i];
        if (!in.Check()) {
            return VM_ERROR;
        }
    }

    std::string task_id_str;
    in >> task_id_str;
    auto ret = StringToC(memBorrowResultC_.task_id, task_id_str, MEM_TASK_ID_MAX);
    if (ret != VM_OK) {
        return ret;
    }

    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}

mem_borrow_result_c MemBorrowExecuteResultMsg::GetBorrowResult()
{
    return memBorrowResultC_;
}

VmResult MemFragmentationMsg::Serialize()
{
    VmSerialization out;
    auto size = numaInfos_.size();
    if (size > MAX_NUMA_NUM) {
        return VM_ERROR_INVAL;
    }
    out << size;
    for (auto& info : numaInfos_) {
        out << info.timestamp;
        out << info.metaData.nodeId;
        out << info.metaData.hostName;
        out << info.metaData.numaId;
        out << info.metaData.socketId;
        out << info.metaData.isLocal;
        out << info.metaData.memTotal;
        out << info.metaData.memFree;

        auto pageInfoSize = info.metaData.numaPageInfo.size();
        out << pageInfoSize;
        for (const auto& [pageType, pageData] : info.metaData.numaPageInfo) {
            out << pageType;
            out << pageData.pageSize;
            out << pageData.hugePageTotal;
            out << pageData.hugePageFree;
        }
    }

    if (!out.Check()) {
        return VM_ERROR;
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = true;
    return VM_OK;
}

VmResult MemFragmentationMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    size_t vecSize = 0;
    in >> vecSize;
    if (vecSize > MAX_NUMA_NUM) {
        return VM_ERROR_INVAL;
    }

    if (!in.Check()) {
        return VM_ERROR;
    }

    numaInfos_.clear();
    for (size_t i = 0; i < vecSize; ++i) {
        NumaInfo info;
        in >> info.timestamp;
        in >> info.metaData.nodeId;
        in >> info.metaData.hostName;
        in >> info.metaData.numaId;
        in >> info.metaData.socketId;
        in >> info.metaData.isLocal;
        in >> info.metaData.memTotal;
        in >> info.metaData.memFree;

        size_t pageInfoSize = 0;
        in >> pageInfoSize;
        for (size_t j = 0; j < pageInfoSize; j++) {
            uint64_t pageType = 0;
            NumaPageData pageData{};
            in >> pageType;
            in >> pageData.pageSize;
            in >> pageData.hugePageTotal;
            in >> pageData.hugePageFree;
            info.metaData.numaPageInfo[pageType] = pageData;
        }

        numaInfos_.push_back(info);
    }

    if (!in.Check()) {
        return VM_ERROR;
    }

    return VM_OK;
}

VmResult MemFragmentationMsg::GetNumaInfo(std::vector<numa_info_t>& numaInfo)
{
    numaInfo.clear();
    for (auto& info : numaInfos_) {
        numa_info_t data;
        data.timestamp = info.timestamp;
        auto ret = StringToC(data.node_id, info.metaData.nodeId, VIRT_MEM_MAX_NODE_ID_LENGTH);
        ret |= StringToC(data.host_name, info.metaData.hostName, UBS_VA_HOST_NAME_MAX);
        if (ret != VM_OK) {
            SafeDeleteArray(data.huge_page_data);
            return ret;
        }
        data.numa_id = info.metaData.numaId;
        data.socket_id = info.metaData.socketId;
        data.is_local = info.metaData.isLocal;
        data.mem_total = info.metaData.memTotal;
        data.mem_free = info.metaData.memFree;
        data.numaPageInfoCount = info.metaData.numaPageInfo.size();
        if (data.numaPageInfoCount != 0) {
            data.huge_page_data = new (std::nothrow) numa_page_data[data.numaPageInfoCount];
            if (data.huge_page_data == nullptr) {
                return VM_ERROR_NOMEM;
            }
            uint64_t i = 0;
            for (auto& [pageType, numaPageData] : info.metaData.numaPageInfo) {
                data.huge_page_data[i].pageSize = numaPageData.pageSize;
                data.huge_page_data[i].hugePageTotal = numaPageData.hugePageTotal;
                data.huge_page_data[i].hugePageFree = numaPageData.hugePageFree;
                i++;
            }
        }
        numaInfo.push_back(data);
    }
    return VM_OK;
}

VmResult MemFragmentationVmInfoMsg::Serialize()
{
    VmSerialization out;
    auto size = vmInfoList_.size();
    if (size > MAX_VM_NUM) {
        return VM_ERROR_INVAL;
    }
    out << size;
    for (auto& info : vmInfoList_) {
        out << info.metaData.nodeId;
        out << info.metaData.hostName;
        out << info.metaData.uuid;
        out << info.metaData.name;
        out << info.metaData.state;
        out << info.metaData.vmCreateTime;
        out << info.metaData.maxMem;
        out << info.metaData.pid;
        auto numaInfoSize = info.numaInfo.size();
        out << numaInfoSize;
        for (auto& [numaId, vmDomainNumaInfo] : info.numaInfo) {
            out << numaId;
            out << vmDomainNumaInfo.numaId;
            out << vmDomainNumaInfo.pageSize;
            out << vmDomainNumaInfo.usedMem;
            out << vmDomainNumaInfo.socketId;
            out << vmDomainNumaInfo.isLocal;
        }
        out << info.timestamp;
    }

    if (!out.Check()) {
        return VM_ERROR;
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = true;
    return VM_OK;
}

VmResult MemFragmentationVmInfoMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    size_t vecSize = 0;
    in >> vecSize;
    if (vecSize > MAX_VM_NUM) {
        return VM_ERROR_INVAL;
    }

    if (!in.Check()) {
        return VM_ERROR;
    }

    vmInfoList_.clear();
    for (size_t i = 0; i < vecSize; ++i) {
        mempooling::VmDomainInfo info;
        in >> info.metaData.nodeId;
        in >> info.metaData.hostName;
        in >> info.metaData.uuid;
        in >> info.metaData.name;
        in >> info.metaData.state;
        in >> info.metaData.vmCreateTime;
        in >> info.metaData.maxMem;
        in >> info.metaData.pid;
        size_t numaInfoSize = 0;
        in >> numaInfoSize;
        for (size_t j = 0; j < numaInfoSize; j++) {
            int16_t numaId = 0;
            VmDomainNumaInfo vmDomainNumaInfo{};
            in >> numaId;
            in >> vmDomainNumaInfo.numaId;
            in >> vmDomainNumaInfo.pageSize;
            in >> vmDomainNumaInfo.usedMem;
            in >> vmDomainNumaInfo.socketId;
            in >> vmDomainNumaInfo.isLocal;
            info.numaInfo.emplace(numaId, vmDomainNumaInfo);
        }
        in >> info.timestamp;
        vmInfoList_.push_back(info);
    }

    if (!in.Check()) {
        return VM_ERROR;
    }

    return VM_OK;
}

std::vector<vm_domain_info_for_c> MemFragmentationVmInfoMsg::GetVmInfo()
{
    std::vector<vm_domain_info_for_c> vmInfo{};
    for (auto& info : vmInfoList_) {
        vm_domain_info_for_c data;
        data.timestamp = info.timestamp;
        auto ret = StringToC(data.metadata.nodeId, info.metaData.nodeId, VIRT_MEM_MAX_NODE_ID_LENGTH);
        ret |= StringToC(data.metadata.hostName, info.metaData.hostName, UBS_VA_HOST_NAME_MAX);
        ret |= StringToC(data.metadata.uuid, info.metaData.uuid, VIRT_MAX_UUID_LENGTH);
        ret |= StringToC(data.metadata.name, info.metaData.name, VIRT_MAX_NAME_LENGTH);
        ret |= StringToC(data.metadata.state, info.metaData.state, VIRT_MAX_STATE_LENGTH);
        if (ret != VM_OK) {
            return {};
        }
        data.metadata.vmCreateTime = info.metaData.vmCreateTime;
        data.metadata.maxMem = info.metaData.maxMem;
        data.metadata.pid = info.metaData.pid;

        data.numaInfoCount = info.numaInfo.size();
        data.numaInfo = new (std::nothrow) vm_numa_info_for_c[data.numaInfoCount];
        if (data.numaInfo == nullptr) {
            return {};
        }
        uint64_t i = 0;
        for (auto& [numaId, vmDomainNumaInfo] : info.numaInfo) {
            data.numaInfo[i].numaId = vmDomainNumaInfo.numaId;
            data.numaInfo[i].socketId = vmDomainNumaInfo.socketId;
            data.numaInfo[i].isLocal = vmDomainNumaInfo.isLocal;
            data.numaInfo[i].pageSize = vmDomainNumaInfo.pageSize;
            data.numaInfo[i].usedMem = vmDomainNumaInfo.usedMem;
            i++;
        }
        vmInfo.push_back(data);
    }
    return vmInfo;
}

VmResult MemFragmentationMemBorrowStrategyInputMsg::Serialize()
{
    VmSerialization out;
    out << srcMemoryBorrowParam_.src_nid;
    out << srcMemoryBorrowParam_.src_numa_id;
    out << srcMemoryBorrowParam_.src_socket_id;
    out << srcMemoryBorrowParam_.borrow_size;
    if (!out.Check()) {
        return VM_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = false;
    return VM_OK;
}

VmResult MemFragmentationMemBorrowStrategyInputMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    if (!in.Check()) {
        return VM_ERROR;
    }

    std::string tmpSrcNid;
    in >> tmpSrcNid;
    auto ret = StringToC(srcMemoryBorrowParam_.src_nid, tmpSrcNid, VIRT_MEM_MAX_NODE_ID_LENGTH);
    if (ret != VM_OK) {
        return ret;
    }
    in >> srcMemoryBorrowParam_.src_numa_id;
    in >> srcMemoryBorrowParam_.src_socket_id;
    in >> srcMemoryBorrowParam_.borrow_size;
    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult MemRollbackMsg::Serialize()
{
    VmSerialization out;

    out << rollbackParams_.node_id;
    if (!out.Check()) {
        return VM_ERROR;
    }

    out << rollbackParams_.borrow_id_size;
    if (!out.Check()) {
        return VM_ERROR;
    }

    for (const auto& borrow_id : rollbackParams_.borrow_id_list) {
        out << borrow_id;
        if (!out.Check()) {
            return VM_ERROR;
        }
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = false;

    return VM_OK;
}

VmResult MemRollbackMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }

    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    if (!in.Check()) {
        return VM_ERROR;
    }

    std::string tmpNodeID;
    in >> tmpNodeID;
    if (!in.Check()) {
        return VM_ERROR;
    }
    rollbackParams_.node_id = tmpNodeID;

    uint32_t tmpBorrowIDSize;
    in >> tmpBorrowIDSize;
    if (!in.Check()) {
        return VM_ERROR;
    }
    rollbackParams_.borrow_id_size = tmpBorrowIDSize;

    rollbackParams_.borrow_id_list.clear();
    for (uint32_t i = 0; i < tmpBorrowIDSize; ++i) {
        std::string tmpBorrowID;
        in >> tmpBorrowID;
        if (!in.Check()) {
            return VM_ERROR;
        }
        rollbackParams_.borrow_id_list.push_back(tmpBorrowID);
    }

    return VM_OK;
}

src_memory_borrow_param MemFragmentationMemBorrowStrategyInputMsg::GetInputMsg() const
{
    return srcMemoryBorrowParam_;
}

VmResult MemBorrowSettingMsg::Serialize()
{
    VmSerialization out;
    borrow_strategy_c& strategy = borrowSettingC_.borrow_strategy;

    out << strategy.src_host_id;
    out << strategy.src_socket_id;
    out << strategy.src_numa_id;
    out << strategy.borrow_size;

    if (strategy.dest_numa_infos_size > MAX_DEST_PARAM_SIZE) {
        return VM_ERROR_INVAL;
    }

    out << strategy.dest_numa_infos_size;
    if (!out.Check()) {
        return VM_ERROR;
    }

    for (uint32_t i = 0; i < strategy.dest_numa_infos_size; i++) {
        out << strategy.dest_numa_infos[i].host_id;
        out << strategy.dest_numa_infos[i].socket_id;

        if (strategy.dest_numa_infos[i].numa_nums > MAX_DEST_NUMA_NUM) {
            return VM_ERROR_INVAL;
        }

        out << strategy.dest_numa_infos[i].numa_nums;
        if (!out.Check()) {
            return VM_ERROR;
        }

        for (uint16_t j = 0; j < strategy.dest_numa_infos[i].numa_nums; j++) {
            out << strategy.dest_numa_infos[i].numa_ids[j];
            out << strategy.dest_numa_infos[i].mem_sizes[j];
        }
    }

    out << borrowSettingC_.isAsync;
    if (!out.Check()) {
        return VM_ERROR;
    }

    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = false;
    return VM_OK;
}

VmResult MemBorrowSettingMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }

    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    borrow_strategy_c& strategy = borrowSettingC_.borrow_strategy;
    std::string tmpMsg;
    in >> tmpMsg;
    VmResult ret = StringToC(strategy.src_host_id, tmpMsg, VIRT_MEM_MAX_NODE_ID_LENGTH);
    if (ret != VM_OK) {
        return ret;
    }

    in >> strategy.src_socket_id;
    in >> strategy.src_numa_id;
    in >> strategy.borrow_size;
    in >> strategy.dest_numa_infos_size;

    if (!in.Check()) {
        return VM_ERROR;
    }

    if (strategy.dest_numa_infos_size > MAX_DEST_PARAM_SIZE) {
        return VM_ERROR_INVAL;
    }

    for (uint32_t i = 0; i < strategy.dest_numa_infos_size; i++) {
        in >> tmpMsg;
        ret = StringToC(strategy.dest_numa_infos[i].host_id, tmpMsg, VIRT_MEM_MAX_NODE_ID_LENGTH);
        if (ret != VM_OK) {
            return ret;
        }

        in >> strategy.dest_numa_infos[i].socket_id;
        in >> strategy.dest_numa_infos[i].numa_nums;
        if (!in.Check()) {
            return VM_ERROR;
        }

        if (strategy.dest_numa_infos[i].numa_nums > MAX_DEST_NUMA_NUM) {
            return VM_ERROR_INVAL;
        }

        for (uint16_t j = 0; j < strategy.dest_numa_infos[i].numa_nums; j++) {
            in >> strategy.dest_numa_infos[i].numa_ids[j];
            in >> strategy.dest_numa_infos[i].mem_sizes[j];
        }
    }

    in >> borrowSettingC_.isAsync;
    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}

borrow_setting_c MemBorrowSettingMsg::GetMemBorrowSettingMsg()
{
    return borrowSettingC_;
}

VmResult MemTaskResultQueryMsg::Serialize()
{
    VmSerialization out;
    out << asyncTaskInfoC_.task_id;
    uint32_t status_value = static_cast<uint32_t>(asyncTaskInfoC_.status);
    out << status_value;
    out << asyncTaskInfoC_.resultCode;
    const auto& result = asyncTaskInfoC_.memBorrowResult;

    if (result.borrow_ids_size > MAX_BORROW_ID_COUNT)
        return VM_ERROR_INVAL;
    out << result.borrow_ids_size;
    if (!out.Check())
        return VM_ERROR;
    for (uint32_t i = 0; i < result.borrow_ids_size; ++i) {
        std::string id_str(result.borrow_ids_ptr[i]);
        out << id_str;
        if (!out.Check())
            return VM_ERROR;
    }
    if (result.present_numa_ids_size > MAX_BORROW_ID_COUNT)
        return VM_ERROR_INVAL;
    out << result.present_numa_ids_size;
    if (!out.Check())
        return VM_ERROR;

    for (uint32_t i = 0; i < result.present_numa_ids_size; i++) {
        out << result.present_numa_ids_ptr[i];
        if (!out.Check())
            return VM_ERROR;
    }

    if (!out.Check())
        return VM_ERROR;
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = false;

    return VM_OK;
}

VmResult MemTaskResultQueryMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    std::string tmpMsg;
    std::string task_id_str;
    in >> task_id_str;
    VmResult ret = StringToC(asyncTaskInfoC_.task_id, task_id_str, MEM_TASK_ID_MAX);
    if (ret != VM_OK) {
        return ret;
    }
    uint32_t status_value = 0;
    in >> status_value;
    asyncTaskInfoC_.status = static_cast<async_task_status_c>(status_value);

    in >> asyncTaskInfoC_.resultCode;
    if (!in.Check()) {
        return VM_ERROR;
    }

    mem_borrow_result_c& result = asyncTaskInfoC_.memBorrowResult;
    in >> result.borrow_ids_size;
    if (!in.Check()) {
        return VM_ERROR;
    }

    for (uint32_t i = 0; i < result.borrow_ids_size; ++i) {
        std::string id_str;
        in >> id_str;
        if (!in.Check()) {
            return VM_ERROR;
        }

        VmResult ret = StringToC(result.borrow_ids_ptr[i], id_str, MAX_BORROW_ID_LENGTH);
        if (ret != VM_OK) {
            return ret;
        }
    }

    in >> result.present_numa_ids_size;
    for (uint32_t i = 0; i < result.present_numa_ids_size; i++) {
        in >> result.present_numa_ids_ptr[i];
    }

    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}

async_task_info_c MemTaskResultQueryMsg::GetTaskResult()
{
    return asyncTaskInfoC_;
}

borrow_strategy_c MemFragmentationMemBorrowStrategyOutputMsg::GetMemBorrowStrategyOutputMsg()
{
    return outputMsg_;
}

VmResult MemFragmentationMemBorrowStrategyOutputMsg::SetOutputMsg(const MemBorrowStrategyResult& result)
{
    auto ret = StringToC(outputMsg_.src_host_id, result.srcParam.srcNid, VIRT_MEM_MAX_NODE_ID_LENGTH);
    if (ret != VM_OK) {
        return ret;
    }
    outputMsg_.src_socket_id = result.srcParam.srcSocketId;
    outputMsg_.src_numa_id = result.srcParam.srcNumaId;
    outputMsg_.borrow_size = result.borrowSize;
    outputMsg_.dest_numa_infos_size = result.destParam.size();
    for (uint32_t i = 0; i < outputMsg_.dest_numa_infos_size; i++) {
        ret =
            StringToC(outputMsg_.dest_numa_infos[i].host_id, result.destParam[i].destNid, VIRT_MEM_MAX_NODE_ID_LENGTH);
        if (ret != VM_OK) {
            return ret;
        }
        outputMsg_.dest_numa_infos[i].socket_id = result.destParam[i].destSocketId;
        outputMsg_.dest_numa_infos[i].numa_nums = result.destParam[i].destNumaNum;
        for (uint16_t j = 0; j < outputMsg_.dest_numa_infos[i].numa_nums; j++) {
            outputMsg_.dest_numa_infos[i].numa_ids[j] = result.destParam[i].destNumaId[j];
            outputMsg_.dest_numa_infos[i].mem_sizes[j] = result.destParam[i].memSize[j];
        }
    }
    return VM_OK;
}

VmResult MemFragmentationMemBorrowStrategyOutputMsg::Serialize()
{
    VmSerialization out;
    out << outputMsg_.src_host_id;
    out << outputMsg_.src_socket_id;
    out << outputMsg_.src_numa_id;
    out << outputMsg_.borrow_size;
    if (outputMsg_.dest_numa_infos_size > MAX_DEST_PARAM_SIZE) {
        return VM_ERROR_INVAL;
    }
    out << outputMsg_.dest_numa_infos_size;
    if (!out.Check()) {
        return VM_ERROR;
    }
    for (auto i = 0; i < outputMsg_.dest_numa_infos_size; i++) {
        out << outputMsg_.dest_numa_infos[i].host_id;
        out << outputMsg_.dest_numa_infos[i].socket_id;
        if (outputMsg_.dest_numa_infos[i].numa_nums > MAX_DEST_NUMA_NUM) {
            return VM_ERROR_INVAL;
        }
        out << outputMsg_.dest_numa_infos[i].numa_nums;
        for (auto j = 0; j < outputMsg_.dest_numa_infos[i].numa_nums; j++) {
            out << outputMsg_.dest_numa_infos[i].numa_ids[j];
            out << outputMsg_.dest_numa_infos[i].mem_sizes[j];
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

VmResult MemFragmentationMemBorrowStrategyOutputMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    std::string tmpMsg;
    in >> tmpMsg;
    auto ret = StringToC(outputMsg_.src_host_id, tmpMsg, VIRT_MEM_MAX_NODE_ID_LENGTH);
    if (ret != VM_OK) {
        return ret;
    }
    in >> outputMsg_.src_socket_id;
    in >> outputMsg_.src_numa_id;
    in >> outputMsg_.borrow_size;
    in >> outputMsg_.dest_numa_infos_size;
    if (!in.Check()) {
        return VM_ERROR;
    }
    if (outputMsg_.dest_numa_infos_size > MAX_DEST_PARAM_SIZE) {
        return VM_ERROR_INVAL;
    }
    for (uint32_t i = 0; i < outputMsg_.dest_numa_infos_size; i++) {
        in >> tmpMsg;
        ret = StringToC(outputMsg_.dest_numa_infos[i].host_id, tmpMsg, VIRT_MEM_MAX_NODE_ID_LENGTH);
        if (ret != VM_OK) {
            return ret;
        }
        in >> outputMsg_.dest_numa_infos[i].socket_id;
        in >> outputMsg_.dest_numa_infos[i].numa_nums;
        if (!in.Check()) {
            return VM_ERROR;
        }
        if (outputMsg_.dest_numa_infos[i].numa_nums > MAX_DEST_NUMA_NUM) {
            return VM_ERROR_INVAL;
        }
        for (uint16_t j = 0; j < outputMsg_.dest_numa_infos[i].numa_nums; j++) {
            in >> outputMsg_.dest_numa_infos[i].numa_ids[j];
            in >> outputMsg_.dest_numa_infos[i].mem_sizes[j];
        }
    }
    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult MemFragmentationMemBorrowExecuteOutputMsg::Serialize()
{
    VmSerialization out;
    size_t size = outputMsg_.borrowIds.size();
    out << size;
    if (!out.Check()) {
        return VM_ERROR;
    }
    for (auto borrowId : outputMsg_.borrowIds) {
        out << borrowId;
    }
    size = outputMsg_.presentNumaIds.size();
    out << size;
    if (!out.Check()) {
        return VM_ERROR;
    }
    for (auto numaId : outputMsg_.presentNumaIds) {
        out << numaId;
    }
    if (!out.Check()) {
        return VM_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = false;
    return VM_OK;
}

VmResult MemFragmentationMemBorrowExecuteOutputMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    size_t size;
    in >> size;
    if (!in.Check()) {
        return VM_ERROR;
    }
    for (size_t i = 0; i < size; i++) {
        std::string tmp;
        in >> tmp;
        outputMsg_.borrowIds.push_back(tmp);
    }
    in >> size;
    if (!in.Check()) {
        return VM_ERROR;
    }
    for (size_t i = 0; i < size; i++) {
        uint16_t tmp = 0;
        in >> tmp;
        outputMsg_.presentNumaIds.push_back(tmp);
    }
    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}

MemBorrowExecuteResult MemFragmentationMemBorrowExecuteOutputMsg::GetMemBorrowExecuteOutputMsg()
{
    return outputMsg_;
}

VmResult MemFragmentationMemMigrateStrategyInputMsg::SetInputMsg(MemMigrateStrategySrcParam memMigrateSrcParam)
{
    inputMsg.borrowSize = memMigrateSrcParam.borrowSize;
    auto ret = memcpy_s(inputMsg.borrowInNode, VIRT_MEM_MAX_NODE_ID_LENGTH, memMigrateSrcParam.borrowInNode,
                        VIRT_MEM_MAX_NODE_ID_LENGTH);
    if (ret != 0) {
        return VM_ERROR;
    }
    if (memMigrateSrcParam.vmInfoListSize > MAX_VM_NUM) {
        return VM_INVALID_PARAM_ERROR;
    }
    inputMsg.vmInfoListSize = memMigrateSrcParam.vmInfoListSize;
    for (uint32_t i = 0; i < inputMsg.vmInfoListSize; i++) {
        inputMsg.vmInfoList[i].pid = memMigrateSrcParam.vmInfoList[i].pid;
        inputMsg.vmInfoList[i].ratio = memMigrateSrcParam.vmInfoList[i].ratio;
    }
    return VM_OK;
}

MemMigrateStrategySrcParam MemFragmentationMemMigrateStrategyInputMsg::GetInputMsg() const
{
    return inputMsg;
}

VmResult MemFragmentationMemMigrateStrategyInputMsg::Serialize()
{
    VmSerialization out;
    out << inputMsg.borrowSize;
    out << inputMsg.borrowInNode;
    out << inputMsg.vmInfoListSize;
    if (!out.Check()) {
        return VM_ERROR;
    }
    if (inputMsg.vmInfoListSize > MAX_VM_NUM) {
        return VM_ERROR_INVAL;
    }
    for (uint32_t i = 0; i < inputMsg.vmInfoListSize; i++) {
        out << inputMsg.vmInfoList[i].pid;
        out << inputMsg.vmInfoList[i].ratio;
    }
    if (!out.Check()) {
        return VM_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = false;
    return VM_OK;
}

VmResult MemFragmentationMemMigrateStrategyInputMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    if (!in.Check()) {
        return VM_ERROR;
    }
    in >> inputMsg.borrowSize;
    std::string tmp;
    in >> tmp;
    auto ret = StringToC(inputMsg.borrowInNode, tmp, VIRT_MEM_MAX_NODE_ID_LENGTH);
    if (ret != VM_OK) {
        return ret;
    }
    in >> inputMsg.vmInfoListSize;
    if (!in.Check()) {
        return VM_ERROR;
    }
    if (inputMsg.vmInfoListSize > MAX_VM_NUM) {
        return VM_ERROR_INVAL;
    }
    for (uint32_t i = 0; i < inputMsg.vmInfoListSize; i++) {
        in >> inputMsg.vmInfoList[i].pid;
        in >> inputMsg.vmInfoList[i].ratio;
    }
    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}

MemFragmentationMemMigrateStrategyOutputMsg::~MemFragmentationMemMigrateStrategyOutputMsg()
{
    SafeDeleteArray(outputMsg.vmInfoList);
}

VmResult MemFragmentationMemMigrateStrategyOutputMsg::SetOutputMsg(MigrateStrategyResult migrateStrategyResult)
{
    if (migrateStrategyResult.vmInfoList.size() > MAX_VM_NUM) {
        return VM_ERROR_INVAL;
    }
    outputMsg.vmInfoListSize = migrateStrategyResult.vmInfoList.size();
    outputMsg.waitingTime = migrateStrategyResult.waitingTime;
    outputMsg.vmInfoList = new (std::nothrow) VmMigrateStrategy[outputMsg.vmInfoListSize];
    if (outputMsg.vmInfoList == nullptr) {
        return VM_ERROR_NOMEM;
    }
    for (uint32_t i = 0; i < outputMsg.vmInfoListSize; i++) {
        outputMsg.vmInfoList[i].pid = migrateStrategyResult.vmInfoList[i].pid;
        outputMsg.vmInfoList[i].memSize = migrateStrategyResult.vmInfoList[i].memSize;
        outputMsg.vmInfoList[i].destNumaId = migrateStrategyResult.vmInfoList[i].desNumaId;
    }
    return VM_OK;
}

MemMigrateStrategy MemFragmentationMemMigrateStrategyOutputMsg::GetOutputMsg() const
{
    return outputMsg;
}

VmResult MemFragmentationMemMigrateStrategyOutputMsg::Serialize()
{
    if (outputMsg.vmInfoListSize > MAX_VM_NUM) {
        return VM_ERROR_INVAL;
    }
    VmSerialization out;
    out << outputMsg.vmInfoListSize;
    if (!out.Check()) {
        return VM_ERROR;
    }
    for (uint32_t i = 0; i < outputMsg.vmInfoListSize; i++) {
        out << outputMsg.vmInfoList[i].destNumaId;
        out << outputMsg.vmInfoList[i].memSize;
        out << outputMsg.vmInfoList[i].pid;
    }
    out << outputMsg.waitingTime;
    if (!out.Check()) {
        return VM_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = false;
    return VM_OK;
}

VmResult MemFragmentationMemMigrateStrategyOutputMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    if (!in.Check()) {
        return VM_ERROR;
    }
    in >> outputMsg.vmInfoListSize;
    if (!in.Check()) {
        return VM_ERROR;
    }
    if (outputMsg.vmInfoListSize > MAX_VM_NUM) {
        return VM_ERROR_INVAL;
    }
    outputMsg.vmInfoList = new (std::nothrow) VmMigrateStrategy[outputMsg.vmInfoListSize];
    if (outputMsg.vmInfoList == nullptr) {
        return VM_ERROR_NOMEM;
    }
    for (uint32_t i = 0; i < outputMsg.vmInfoListSize; i++) {
        in >> outputMsg.vmInfoList[i].destNumaId;
        in >> outputMsg.vmInfoList[i].memSize;
        in >> outputMsg.vmInfoList[i].pid;
    }
    in >> outputMsg.waitingTime;
    return VM_OK;
}

VmResult MemFragmentationMemMigrateExecuteInputMsg::SetInputMsg(MemMigrateExecuteSrcParam srcParam)
{
    if (srcParam.borrowIdsCount > MAX_BORROW_ID_COUNT || srcParam.vmInfoListSize > MAX_VM_NUM) {
        return VM_ERROR_INVAL;
    }
    auto ret = memcpy_s(inputMsg.borrowInNode, VIRT_MEM_MAX_NODE_ID_LENGTH, srcParam.borrowInNode,
                        VIRT_MEM_MAX_NODE_ID_LENGTH);
    if (ret != 0) {
        return VM_ERROR;
    }
    inputMsg.borrowIdsCount = srcParam.borrowIdsCount;
    for (uint32_t i = 0; i < inputMsg.borrowIdsCount; i++) {
        ret = memcpy_s(inputMsg.borrowIds[i], MAX_BORROW_ID_LENGTH, srcParam.borrowIds[i], MAX_BORROW_ID_LENGTH);
        if (ret != 0) {
            return VM_ERROR;
        }
    }
    inputMsg.vmInfoListSize = srcParam.vmInfoListSize;
    for (uint32_t i = 0; i < inputMsg.vmInfoListSize; i++) {
        inputMsg.vmInfoList[i].destNumaId = srcParam.vmInfoList[i].destNumaId;
        inputMsg.vmInfoList[i].memSize = srcParam.vmInfoList[i].memSize;
        inputMsg.vmInfoList[i].pid = srcParam.vmInfoList[i].pid;
    }
    inputMsg.waitingTime = srcParam.waitingTime;
    return VM_OK;
}

MemMigrateExecuteSrcParam MemFragmentationMemMigrateExecuteInputMsg::GetInputMsg() const
{
    return inputMsg;
}

VmResult MemFragmentationMemMigrateExecuteInputMsg::Serialize()
{
    VmSerialization out;
    out << inputMsg.borrowInNode;
    if (inputMsg.borrowIdsCount > MAX_BORROW_ID_COUNT) {
        return VM_ERROR_INVAL;
    }
    out << inputMsg.borrowIdsCount;
    if (!out.Check()) {
        return VM_ERROR;
    }
    for (uint32_t i = 0; i < inputMsg.borrowIdsCount; i++) {
        out << inputMsg.borrowIds[i];
    }
    if (inputMsg.vmInfoListSize > MAX_VM_NUM) {
        return VM_ERROR_INVAL;
    }
    out << inputMsg.vmInfoListSize;
    if (!out.Check()) {
        return VM_ERROR;
    }
    for (uint32_t i = 0; i < inputMsg.vmInfoListSize; i++) {
        out << inputMsg.vmInfoList[i].destNumaId;
        out << inputMsg.vmInfoList[i].memSize;
        out << inputMsg.vmInfoList[i].pid;
    }
    out << inputMsg.waitingTime;
    if (!out.Check()) {
        return VM_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = false;
    return VM_OK;
}

VmResult MemFragmentationMemMigrateExecuteInputMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    if (!in.Check()) {
        return VM_ERROR;
    }
    std::string tmp;
    in >> tmp;
    auto ret = StringToC(inputMsg.borrowInNode, tmp, sizeof(inputMsg.borrowInNode));
    if (ret != VM_OK) {
        return ret;
    }
    in >> inputMsg.borrowIdsCount;
    if (!in.Check()) {
        return VM_ERROR;
    }
    if (inputMsg.borrowIdsCount > MAX_BORROW_ID_COUNT) {
        return VM_ERROR_INVAL;
    }

    for (uint32_t i = 0; i < inputMsg.borrowIdsCount; i++) {
        in >> tmp;
        ret = StringToC(inputMsg.borrowIds[i], tmp, sizeof(inputMsg.borrowIds[i]));
        if (ret != VM_OK) {
            return ret;
        }
    }
    in >> inputMsg.vmInfoListSize;
    if (!in.Check()) {
        return VM_ERROR;
    }
    if (inputMsg.vmInfoListSize > MAX_VM_NUM) {
        return VM_ERROR_INVAL;
    }
    for (uint32_t i = 0; i < inputMsg.vmInfoListSize; i++) {
        in >> inputMsg.vmInfoList[i].destNumaId;
        in >> inputMsg.vmInfoList[i].memSize;
        in >> inputMsg.vmInfoList[i].pid;
    }
    in >> inputMsg.waitingTime;
    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult mem_fragmentation::MemFragmentationNodeInfoListMsg::Serialize()
{
    VmSerialization out;
    auto nodeInfoListCount = nodeInfoList.size();
    out << nodeInfoListCount;
    for (auto& [nodeId, numaInfos, isCurrent] : nodeInfoList) {
        out << nodeId;
        auto numaInfosCount = numaInfos.size();
        out << numaInfosCount;
        for (auto& [timestamp, metaData] : numaInfos) {
            out << timestamp;
            out << metaData.nodeId;
            out << metaData.hostName;
            out << metaData.numaId;
            out << metaData.socketId;
            out << metaData.isLocal;
            out << metaData.memTotal;
            out << metaData.memFree;
            auto numaPageSize = metaData.numaPageInfo.size();
            out << numaPageSize;
            for (auto& [pageSize, numaPageData] : metaData.numaPageInfo) {
                out << numaPageData.pageSize;
                out << numaPageData.hugePageTotal;
                out << numaPageData.hugePageFree;
            }
        }
        out << isCurrent;
    }
    if (!out.Check()) {
        return VM_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = false;
    return VM_OK;
}

VmResult mem_fragmentation::MemFragmentationNodeInfoListMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    if (!in.Check()) {
        return VM_ERROR;
    }

    size_t nodeInfoListCount{};
    in >> nodeInfoListCount;
    nodeInfoList.reserve(nodeInfoListCount);
    for (size_t i = 0; i < nodeInfoListCount; i++) {
        NodeInfo nodeInfo{};
        in >> nodeInfo.nodeId;
        size_t numaInfosCount{};
        in >> numaInfosCount;
        nodeInfo.numaInfos.reserve(numaInfosCount);
        for (size_t j = 0; j < numaInfosCount; j++) {
            NumaInfo numaInfo{};
            in >> numaInfo.timestamp;
            in >> numaInfo.metaData.nodeId;
            in >> numaInfo.metaData.hostName;
            in >> numaInfo.metaData.numaId;
            in >> numaInfo.metaData.socketId;
            in >> numaInfo.metaData.isLocal;
            in >> numaInfo.metaData.memTotal;
            in >> numaInfo.metaData.memFree;
            size_t numaPageInfoSize{};
            in >> numaPageInfoSize;
            for (size_t k = 0; k < numaPageInfoSize; k++) {
                NumaPageData numaPageData{};
                in >> numaPageData.pageSize;
                in >> numaPageData.hugePageTotal;
                in >> numaPageData.hugePageFree;
                numaInfo.metaData.numaPageInfo[numaPageData.pageSize] = numaPageData;
            }
            nodeInfo.numaInfos.emplace_back(std::move(numaInfo));
        }
        in >> nodeInfo.isCurrent;
        nodeInfoList.emplace_back(std::move(nodeInfo));
    }

    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult mem_fragmentation::MemFragmentationMemBorrowParamMsg::Serialize()
{
    VmSerialization out;
    out << borrowPram.nodeId;
    uint32_t numaMetaInfosCount = borrowPram.numaMetaInfos.size();
    out << numaMetaInfosCount;
    for (auto& [numaId] : borrowPram.numaMetaInfos) {
        out << numaId;
    }
    out << borrowPram.borrowSize;
    out << isAsync;
    if (!out.Check()) {
        return VM_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = false;
    return VM_OK;
}

VmResult mem_fragmentation::MemFragmentationMemBorrowParamMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    if (!in.Check()) {
        return VM_ERROR;
    }

    in >> borrowPram.nodeId;
    uint32_t numaMetaInfosCount{};
    in >> numaMetaInfosCount;
    borrowPram.numaMetaInfos.reserve(numaMetaInfosCount);
    for (size_t j = 0; j < numaMetaInfosCount; j++) {
        NumaMetaInfo numaMetaInfo{};
        in >> numaMetaInfo.numaId;
        borrowPram.numaMetaInfos.emplace_back(std::move(numaMetaInfo));
    }
    in >> borrowPram.borrowSize;
    in >> isAsync;

    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult mem_fragmentation::MemFragmentationMemBorrowResultMsg::Serialize()
{
    VmSerialization out;

    auto memBorrowRstCsSize = memBorrowRstCs.size();
    out << memBorrowRstCsSize;
    for (size_t i = 0; i < memBorrowRstCsSize; ++i) {
        auto [borrow_ids_ptr, present_numa_ids_ptr, borrow_ids_size, present_numa_ids_size, task_id] =
            memBorrowRstCs[i];
        out << borrow_ids_size;
        for (size_t j = 0; j < borrow_ids_size; ++j) {
            out << borrow_ids_ptr[j];
        }
        out << present_numa_ids_size;
        for (size_t j = 0; j < present_numa_ids_size; ++j) {
            out << present_numa_ids_ptr[j];
        }
        out << task_id;
    }

    if (!out.Check()) {
        return VM_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = out.GetBuffer(true);
    mOutputRawDataOwned = false;
    return VM_OK;
}

VmResult mem_fragmentation::MemFragmentationMemBorrowResultMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    if (!in.Check()) {
        return VM_ERROR;
    }

    size_t memBorrowRstCsSize{};
    in >> memBorrowRstCsSize;
    memBorrowRstCs.reserve(memBorrowRstCsSize);
    std::string tmpMsg;
    VmResult ret{};
    for (size_t i = 0; i < memBorrowRstCsSize; ++i) {
        mem_borrow_result_c memBorrowRstC{};
        in >> memBorrowRstC.borrow_ids_size;
        for (size_t j = 0; j < memBorrowRstC.borrow_ids_size; ++j) {
            in >> tmpMsg;
            ret = StringToC(memBorrowRstC.borrow_ids_ptr[j], tmpMsg, MAX_BORROW_ID_LENGTH);
            if (ret != VM_OK) {
                return VM_ERROR;
            }
        }
        in >> memBorrowRstC.present_numa_ids_size;
        for (size_t j = 0; j < memBorrowRstC.present_numa_ids_size; ++j) {
            in >> memBorrowRstC.present_numa_ids_ptr[j];
        }
        in >> tmpMsg;
        ret = StringToC(memBorrowRstC.task_id, tmpMsg, MEM_TASK_ID_MAX);
        if (ret != VM_OK) {
            return VM_ERROR;
        }
        memBorrowRstCs.emplace_back(memBorrowRstC);
    }

    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult mem_fragmentation::MemFragmentationPageSwapEnableMsg::Serialize()
{
    VmSerialization out;

    out << pid;
    uint32_t pageSwapPairsCount = pageSwapPairs.size();
    out << pageSwapPairsCount;
    for (auto& [localNumaQuota, remoteNumaQuota] : pageSwapPairs) {
        uint32_t localNumaQuotaCount = localNumaQuota.size();
        out << localNumaQuotaCount;
        for (auto& [numaId, quota] : localNumaQuota) {
            out << numaId;
            out << quota;
        }
        uint32_t remoteNumaQuotaCount = remoteNumaQuota.size();
        out << remoteNumaQuotaCount;
        for (auto& [numaId, quota] : remoteNumaQuota) {
            out << numaId;
            out << quota;
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

VmResult mem_fragmentation::MemFragmentationPageSwapEnableMsg::Deserialize()
{
    if (mInputRawData == nullptr) {
        return VM_ERROR_NULLPTR;
    }
    VmDeSerialization in(mInputRawData, mInputRawDataSize);
    if (!in.Check()) {
        return VM_ERROR;
    }

    in >> pid;
    uint32_t pageSwapPairsCount = 0;
    in >> pageSwapPairsCount;
    pageSwapPairs.reserve(pageSwapPairsCount);
    for (size_t j = 0; j < pageSwapPairsCount; j++) {
        PageSwapPair pageSwapPair{};
        uint32_t localNumaQuotaCount = 0;
        in >> localNumaQuotaCount;
        pageSwapPair.localNumaQuotas.reserve(localNumaQuotaCount);
        for (size_t k = 0; k < localNumaQuotaCount; k++) {
            NumaQuota numaQuota{};
            in >> numaQuota.numaId;
            in >> numaQuota.quota;
            pageSwapPair.localNumaQuotas.emplace_back(std::move(numaQuota));
        }
        uint32_t remoteNumaQuotaCount = 0;
        in >> remoteNumaQuotaCount;
        pageSwapPair.remoteNumaQuotas.reserve(remoteNumaQuotaCount);
        for (size_t k = 0; k < remoteNumaQuotaCount; k++) {
            NumaQuota numaQuota{};
            in >> numaQuota.numaId;
            in >> numaQuota.quota;
            pageSwapPair.remoteNumaQuotas.emplace_back(std::move(numaQuota));
        }
        pageSwapPairs.emplace_back(std::move(pageSwapPair));
    }

    if (!in.Check()) {
        return VM_ERROR;
    }
    return VM_OK;
}
} // namespace vm
