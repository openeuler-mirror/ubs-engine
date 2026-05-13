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

#include "mem_fragmentation_sdk_server.h"
#include <ubse_api_server.h>
#include <ubse_logger.h>
#include <ubse_node.h>
#include <ubse_security.h>
#include <map>
#include <string>
#include <vector>

#include "hugepage_handler.h"
#include "mem_fragmentation_msg.h"
#include "mem_handler.h"
#include "mem_task_manager.h"
#include "mempooling_def.h"
#include "mempooling_module.h"
#include "msg_utils.h"
#include "ubs_virt_agent_object_def.h"
#include "vm_configuration.h"
#include "vm_def.h"
#include "vm_sdk_def.h"
#include "vm_system_util.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using std::string;
using std::vector;
using namespace ubse::log;
using namespace ubse::nodeController;
using namespace ubse::security;
using namespace api::server;
using namespace vm::mempooling;

constexpr uint32_t NODE_ANTI_MIN_LENGTH = 1;
VmResult VirtMemFragSdk::QueryRegister()
{
    auto ret = RegisterIpcHandler(UBS_VA_QUERY, UBS_VA_NUMA_INFO, GetNodeInfo, UBS_VA_QUERY_PERMISSION);
    ret |= RegisterIpcHandler(UBS_VA_QUERY, UBS_VA_VM_INFO, GetVmInfo, UBS_VA_QUERY_PERMISSION);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Registration of query sdk server failed. " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_DEBUG << "Query sdk server registration successful.";
    return VM_OK;
}

VmResult VirtMemFragSdk::Register()
{
    auto ret = RegisterIpcHandler(UBS_VA_MEM_FRAGMENTATION, UBS_VA_NODE_ANTI_AFFINITY, NodeAntiAffinity,
                                  UBS_VA_FRAGMENTATION_PERMISSION);
    ret |= RegisterIpcHandler(UBS_VA_MEM_FRAGMENTATION, UBS_VA_MEM_BORROW_STRATEGY, MemBorrowStrategy,
                              UBS_VA_FRAGMENTATION_PERMISSION);
    ret |= RegisterIpcHandler(UBS_VA_MEM_FRAGMENTATION, UBS_VA_MEM_BORROW_EXECUTE, MemBorrowExecute,
                              UBS_VA_FRAGMENTATION_PERMISSION);
    ret |= RegisterIpcHandler(UBS_VA_MEM_FRAGMENTATION, UBS_VA_MEM_MIGRATE_STRATEGY, MemMigrateStrategy,
                              UBS_VA_FRAGMENTATION_PERMISSION);
    ret |= RegisterIpcHandler(UBS_VA_MEM_FRAGMENTATION, UBS_VA_MEM_MIGRATE_EXECUTE, MemMigrateExecute,
                              UBS_VA_FRAGMENTATION_PERMISSION);
    ret |= RegisterIpcHandler(UBS_VA_MEM_FRAGMENTATION, UBS_VA_MEM_RETURN, MemReturn, UBS_VA_FRAGMENTATION_PERMISSION);
    ret |=
        RegisterIpcHandler(UBS_VA_MEM_FRAGMENTATION, UBS_VA_MEM_ROLLBACK, MemRollback, UBS_VA_FRAGMENTATION_PERMISSION);
    ret |= RegisterIpcHandler(UBS_VA_MEM_FRAGMENTATION, UBS_VA_SYNC_TASK_QUERY, MemTaskQuery,
                              UBS_VA_FRAGMENTATION_PERMISSION);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Registration of VirtMemFragSdk failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_DEBUG << "VirtMemFragSdk registration succeed.";
    return VM_OK;
}

uint32_t GetNumaInfoList(std::vector<NumaInfo>& numaInfos)
{
    UBSE_LOG_INFO << "GetNumaInfoList start.";
    numaInfos.clear();
    const auto UBSRMRSGetNumaInfoListOnNode = MempoolingModule::UBSRMRSGetNumaInfoListOnNode();
    if (UBSRMRSGetNumaInfoListOnNode == nullptr) {
        return VM_ERROR;
    }
    try {
        auto ret = UBSRMRSGetNumaInfoListOnNode(numaInfos);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Call UBSRMRSGetNumaInfoListOnNode failed. " << FormatRetCode(ret);
            return VM_ERROR;
        }
        if (numaInfos.size() > MAX_NUMA_NUM) {
            UBSE_LOG_ERROR << "The size of numaInfos is " << numaInfos.size() << ", which is more than "
                           << MAX_NUMA_NUM;
            return VM_ERROR;
        }
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Call UBSRMRSGetNumaInfoListOnNode Exception. " << e.what();
        return VM_ERROR;
    }

    UBSE_LOG_INFO << "NumaInfos size=" << numaInfos.size();
    return VM_OK;
}

uint32_t GetVmInfoList(std::vector<mempooling::VmDomainInfo>& vmInfoList)
{
    UBSE_LOG_INFO << "GetVmInfoList start.";
    vmInfoList.clear();
    // invoke
    const auto UBSRMRSGetVmInfoListOnNode = MempoolingModule::UBSRMRSGetVmInfoListOnNode();
    if (UBSRMRSGetVmInfoListOnNode == nullptr) {
        return VM_ERROR;
    }
    try {
        auto ret = UBSRMRSGetVmInfoListOnNode(vmInfoList);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Call UBSRMRSGetVmInfoListOnNode failed. " << FormatRetCode(ret);
            return VM_ERROR;
        }
        if (vmInfoList.size() > MAX_VM_NUM) {
            UBSE_LOG_ERROR << "The size of vmInfoList is " << vmInfoList.size() << ", which is more than "
                           << MAX_VM_NUM;
            return VM_ERROR;
        }
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Call UBSRMRSGetVmInfoListOnNode Exception. " << e.what();
        return VM_ERROR;
    }

    UBSE_LOG_INFO << "vmInfoList size=" << vmInfoList.size();
    return VM_OK;
}

uint32_t PackNumaInfoList(const std::vector<NumaInfo>& numaInfos, UbseIpcMessage& buffer)
{
    MemFragmentationMsg msg{numaInfos};
    UBSE_LOG_DEBUG << "Start MemFragmentationMsg Serialize.";
    auto ret = msg.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemFragmentationMsg Serialize fail. " << FormatRetCode(ret);
        return ret;
    }

    buffer.buffer = new (std::nothrow) uint8_t[msg.SerializedDataSize()];
    if (buffer.buffer == nullptr) {
        UBSE_LOG_ERROR << "PackNumaInfoList new buffer failed.";
        return VM_ERROR;
    }
    ret = memcpy_s(buffer.buffer, msg.SerializedDataSize(), msg.SerializedData(), msg.SerializedDataSize());
    if (ret != EOK) {
        SafeDeleteArray(buffer.buffer);
        UBSE_LOG_ERROR << "PackNumaInfoList memcpy_s failed.";
        return VM_ERROR;
    }
    buffer.length = msg.SerializedDataSize();
    return VM_OK;
}

uint32_t PackVmInfoList(const std::vector<mempooling::VmDomainInfo>& vmInfoList, UbseIpcMessage& buffer)
{
    MemFragmentationVmInfoMsg msg{vmInfoList};

    auto ret = msg.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemFragmentationMsg Serialize fail. " << FormatRetCode(ret);
        return ret;
    }
    buffer.buffer = new (std::nothrow) uint8_t[msg.SerializedDataSize()];
    if (buffer.buffer == nullptr) {
        UBSE_LOG_ERROR << "PackVmInfoList new buffer failed.";
        return VM_ERROR;
    }
    ret = memcpy_s(buffer.buffer, msg.SerializedDataSize(), msg.SerializedData(), msg.SerializedDataSize());
    if (ret != EOK) {
        SafeDeleteArray(buffer.buffer);
        UBSE_LOG_ERROR << "PackVmInfoList memcpy_s failed.";
        return VM_ERROR;
    }
    buffer.length = msg.SerializedDataSize();

    return VM_OK;
}

uint32_t VirtMemFragSdk::GetNodeInfo(const UbseIpcMessage& req, const UbseRequestContext& context)
{
    std::vector<NumaInfo> numaInfos{};
    auto ret = GetNumaInfoList(numaInfos);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Call GetNumaInfoList fail. " << FormatRetCode(ret);
        return ret;
    }
    UbseIpcMessage resp{};
    UBSE_LOG_DEBUG << "numaInfos.size = " << numaInfos.size();
    ret = PackNumaInfoList(numaInfos, resp);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "numaInfos pack failed, " << FormatRetCode(ret);
        return ret;
    }
    ret = SendResponse(VM_OK, context.requestId, resp);
    SafeDeleteArray(resp.buffer);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "GetNodeInfo response send failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_DEBUG << "GetNodeInfo send succeed.";
    return ret;
}

uint32_t VirtMemFragSdk::GetVmInfo(const UbseIpcMessage& req, const UbseRequestContext& context)
{
    std::vector<mempooling::VmDomainInfo> vmInfoList{};
    auto ret = GetVmInfoList(vmInfoList);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Call GetVmInfoList fail. " << FormatRetCode(ret);
        return ret;
    }
    UbseIpcMessage resp{};
    ret = PackVmInfoList(vmInfoList, resp);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "PackVmInfoList pack failed, " << FormatRetCode(ret);
        return ret;
    }
    ret = SendResponse(VM_OK, context.requestId, resp);
    SafeDeleteArray(resp.buffer);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "GetVmInfo response send failed, " << FormatRetCode(ret);
        return ret;
    }
    return ret;
}

bool VirtMemFragSdk::ValidateRequest(const UbseIpcMessage& req)
{
    if (req.buffer == nullptr || req.length == 0) {
        UBSE_LOG_ERROR << "Request buffer is null or empty.";
        return false;
    }
    return true;
}

uint32_t VirtMemFragSdk::UpdateAntiAffinityConfig(
    const std::map<std::string, std::vector<std::string>>& nodeAntiAffinityMap)
{
    const auto updateAntiNode = MempoolingModule::UBSRMRSUpdateAntiNode();
    if (updateAntiNode == nullptr) {
        UBSE_LOG_ERROR << "UpdateAntiNode function is null.";
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "Start to update anti-affinity configuration.";
    try {
        uint32_t ret = updateAntiNode(nodeAntiAffinityMap);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Failed to update anti-affinity configuration. " << FormatRetCode(ret);
            return ret;
        }
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Call UBSRMRSUpdateAntiNode Exception. " << e.what();
        return VM_ERROR;
    }

    UBSE_LOG_INFO << "Succeed to update anti-affinity configuration.";
    return VM_OK;
}

std::pair<string, const uint8_t*> VirtMemFragSdk::ParseKey(const uint8_t* buffer, size_t buffer_size,
                                                           const uint8_t*& ptr)
{
    if (ptr < buffer || ptr > buffer + buffer_size) {
        UBSE_LOG_ERROR << "Pointer out of buffer range.";
        return make_pair(string(), ptr);
    }

    size_t remaining = buffer_size - (ptr - buffer);

    if (remaining < sizeof(uint32_t)) {
        UBSE_LOG_ERROR << "Insufficient buffer to read key length.";
        return make_pair(string(), ptr);
    }

    uint32_t key_length;
    if (memcpy_s(&key_length, sizeof(uint32_t), ptr, sizeof(uint32_t)) != 0) {
        UBSE_LOG_ERROR << "Failed to copy key length.";
        return make_pair(string(), ptr);
    }
    ptr += sizeof(uint32_t);
    remaining -= sizeof(uint32_t);

    if (key_length <= NODE_ANTI_MIN_LENGTH || key_length > remaining) {
        return make_pair(string(), ptr);
    }

    string key;
    key.assign(reinterpret_cast<const char*>(ptr), key_length - 1);
    ptr += key_length;
    return make_pair(key, ptr);
}

vector<string> VirtMemFragSdk::ParseValues(const uint8_t* buffer, size_t buffer_size, const uint8_t*& ptr)
{
    if (ptr > buffer + buffer_size || buffer + buffer_size - ptr < sizeof(uint32_t)) {
        UBSE_LOG_ERROR << "Insufficient buffer to read value count.";
        return {};
    }
    uint32_t value_count;
    if (memcpy_s(&value_count, sizeof(uint32_t), ptr, sizeof(uint32_t)) != 0) {
        UBSE_LOG_ERROR << "Failed to copy value count.";
        return {};
    }
    ptr += sizeof(uint32_t);
    vector<string> values;
    for (uint32_t j = 0; j < value_count; ++j) {
        if (buffer + buffer_size - ptr < sizeof(uint32_t)) {
            UBSE_LOG_ERROR << "Failed to copy value length at index=" << j;
            return {};
        }
        uint32_t value_length;
        if (memcpy_s(&value_length, sizeof(uint32_t), ptr, sizeof(uint32_t)) != 0) {
            UBSE_LOG_ERROR << "Failed to copy value length at index=" << j;
            return {};
        }
        ptr += sizeof(uint32_t);
        if (buffer + buffer_size - ptr < value_length) {
            UBSE_LOG_ERROR << "Failed to copy value length at index=" << j;
            return {};
        }
        if (value_length <= NODE_ANTI_MIN_LENGTH) {
            ptr += value_length;
            values.emplace_back("");
            continue;
        }
        string value_str;
        value_str.assign(reinterpret_cast<const char*>(ptr), value_length - 1);
        ptr += value_length;
        values.push_back(value_str);
    }
    return values;
}

std::pair<string, vector<string>> VirtMemFragSdk::ParseEntry(const uint8_t* buffer, size_t buffer_size,
                                                             const uint8_t*& ptr)
{
    auto [key, new_ptr] = ParseKey(buffer, buffer_size, ptr);
    if (key.empty()) {
        return make_pair(string(), vector<string>());
    }
    ptr = new_ptr;
    auto values = ParseValues(buffer, buffer_size, ptr);
    if (values.empty()) {
        return make_pair(key, vector<string>());
    }
    return make_pair(key, values);
}

uint32_t VirtMemFragSdk::DeserializeNodeAntiDictionary(const uint8_t* buffer, size_t buffer_size,
                                                       std::map<std::string, std::vector<std::string>>& node_dict_map)
{
    if (buffer == nullptr || buffer_size == 0) {
        return VM_ERROR;
    }

    const uint8_t* ptr = buffer;

    std::map<std::string, std::string> tmpNodeAntiAffinityMap;
    std::vector<UbseRoleInfo> roleInfos;
    auto ret = UbseGetAllNodeInfos(roleInfos);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Get node info failed, " << FormatRetCode(ret);
        return VM_ERROR;
    }

    for (const auto& node : roleInfos) {
        UBSE_LOG_DEBUG << "Node=" << node.nodeId;
        tmpNodeAntiAffinityMap[node.nodeId] = "";
    }

    if (ptr + sizeof(uint32_t) > buffer + buffer_size) {
        return VM_ERROR;
    }

    uint32_t entries_count;
    if (memcpy_s(&entries_count, sizeof(uint32_t), ptr, sizeof(uint32_t)) != 0) {
        UBSE_LOG_ERROR << "Failed to copy entries_count.";
        return VM_ERROR;
    }
    ptr += sizeof(uint32_t);

    for (uint32_t i = 0; i < entries_count; ++i) {
        auto entry = ParseEntry(buffer, buffer_size, ptr);
        if (!entry.first.empty()) {
            if (tmpNodeAntiAffinityMap.find(entry.first) == tmpNodeAntiAffinityMap.end()) {
                UBSE_LOG_ERROR << "Invalid node ID=" << entry.first << " is not in the configured node list.";
                return VM_ERROR;
            }
            node_dict_map[entry.first] = entry.second;
        }
    }

    for (const auto& it : tmpNodeAntiAffinityMap) {
        const std::string& key = it.first;
        if (node_dict_map.find(key) == node_dict_map.end()) {
            UBSE_LOG_ERROR << "Node ID=" << key << " is not present in the deserialized data.";
            return VM_ERROR;
        }
    }

    return VM_OK;
}

uint32_t VirtMemFragSdk::NodeAntiAffinity(const UbseIpcMessage& req, const UbseRequestContext& context)
{
    UBSE_LOG_INFO << "NodeAntiAffinity start.";
    UbseIpcMessage resp{};
    // Validate input parameters
    if (!ValidateRequest(req)) {
        UBSE_LOG_ERROR << "Request validation failed.";
        SendResponse(VM_ERROR, context.requestId, resp);
        return VM_ERROR;
    }

    std::map<string, vector<string>> nodeAntiDict;
    // Deserialize NodeAntiDictionary from buffer
    uint32_t ret = DeserializeNodeAntiDictionary(req.buffer, req.length, nodeAntiDict);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to DeserializeNodeAntiDictionary.";
        SendResponse(VM_ERROR, context.requestId, resp);
        return ret;
    }
    // Update anti-affinity configuration
    ret = UpdateAntiAffinityConfig(nodeAntiDict);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to update anti-affinity configuration.";
        SendResponse(VM_ERROR, context.requestId, resp);
        return ret;
    }

    // Send response
    SendResponse(VM_OK, context.requestId, resp);
    UBSE_LOG_INFO << "NodeAntiAffinity end.";
    return VM_OK;
}

uint32_t VirtMemFragSdk::PackBorrowStrategyRsp(const MemBorrowStrategyResult& borrowStrategyResult,
                                               UbseIpcMessage& buffer)
{
    MemFragmentationMemBorrowStrategyOutputMsg msg{};
    auto ret = msg.SetOutputMsg(borrowStrategyResult);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemFragmentationMemBorrowStrategyOutputMsg Set failed.";
        return ret;
    }
    ret = msg.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemFragmentationMemBorrowStrategyMsg Serialize fail, " << FormatRetCode(ret);
        return ret;
    }
    buffer.buffer = msg.SerializedData();
    if (buffer.buffer == nullptr) {
        UBSE_LOG_ERROR << "MemFragmentationMemBorrowStrategyMsg new buffer failed.";
        return VM_ERROR;
    }
    buffer.length = msg.SerializedDataSize();
    if (buffer.length == 0) {
        UBSE_LOG_ERROR << "The length of MemFragmentationMemBorrowStrategyMsg is 0.";
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult CallUBSRMRSMemBorrowStrategy(const SrcMemoryBorrowParam& srcMemBorrowParam, const uint64_t& memBorrowSize,
                                      MemBorrowStrategyResult& borrowStrategyResult)
{
    const auto UBSRMRSMemBorrowStrategy = MempoolingModule::UBSRMRSMemBorrowStrategy();
    if (UBSRMRSMemBorrowStrategy == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSMemBorrowStrategy fail.";
        return VM_ERROR;
    }
    try {
        auto ret = UBSRMRSMemBorrowStrategy(srcMemBorrowParam, memBorrowSize, borrowStrategyResult);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Call UBSRMRSMemBorrowStrategy fail. " << FormatRetCode(ret);
            return ret;
        }
        // The size of destParam is the same as borrowIdSize.
        if (borrowStrategyResult.destParam.size() > MAX_DEST_PARAM_SIZE) {
            UBSE_LOG_ERROR << "The size of destParam is invalid.";
            return VM_INVALID_PARAM_ERROR;
        }
        for (auto& param : borrowStrategyResult.destParam) {
            if (param.destNumaNum != param.destNumaId.size() || param.destNumaNum != param.memSize.size() ||
                param.destNumaNum > MAX_DEST_NUMA_NUM) {
                UBSE_LOG_ERROR << "The destNumaNum is invalid.";
                return VM_INVALID_PARAM_ERROR;
            }
        }
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Call UBSRMRSMemBorrowStrategy Exception. " << e.what();
        return VM_ERROR;
    }
    return VM_OK;
}

uint32_t VirtMemFragSdk::MemBorrowStrategy(const UbseIpcMessage& req, const UbseRequestContext& context)
{
    UBSE_LOG_INFO << "MemBorrowStrategy start.";
    MemFragmentationMemBorrowStrategyInputMsg inputMsg(req.buffer, req.length);
    if (!ValidateRequest(req)) {
        UBSE_LOG_ERROR << "Request validation failed.";
        return VM_ERROR;
    }
    auto ret = inputMsg.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemBorrowStrategyInputMsg deserialize failed, " << FormatRetCode(ret);
        return VM_ERROR_DESERIALIZE_ERROR;
    }
    src_memory_borrow_param tmpMsg = inputMsg.GetInputMsg();
    SrcMemoryBorrowParam srcMemBorrowParam{
        .srcNid = tmpMsg.src_nid, .srcSocketId = tmpMsg.src_socket_id, .srcNumaId = tmpMsg.src_numa_id};
    uint64_t memBorrowSize = tmpMsg.borrow_size;
    MemBorrowStrategyResult borrowStrategyResult;
    // call "MemBorrowStrategy" function
    ret = CallUBSRMRSMemBorrowStrategy(srcMemBorrowParam, memBorrowSize, borrowStrategyResult);
    if (ret != VM_OK) {
        return ret;
    }
    // pack
    UbseIpcMessage resp{};
    ret = PackBorrowStrategyRsp(borrowStrategyResult, resp);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemBorrowStrategy pack failed, " << FormatRetCode(ret);
        SafeDeleteArray(resp.buffer);
        return ret;
    }
    // return data
    ret = SendResponse(VM_OK, context.requestId, resp);
    SafeDeleteArray(resp.buffer);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemBorrowStrategy response send failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "MemBorrowStrategy end.";
    return VM_OK;
}

/**
 * Set source node remote hugepage memory
 *
 * @param srcNid
 * @param borrowExecuteResult
 * @return 0 for success, non-zero for error
 */
VmResult VirtMemFragSdk::SetSrcNodeHugePage(const MemBorrowExecuteResult& borrowExecuteResult)
{
    if (borrowExecuteResult.presentNumaIds.empty()) {
        UBSE_LOG_ERROR << "PresentNumaId is empty.";
        return VM_ERROR;
    }
    // initialize numaBorrowedSizeMap
    std::map<uint16_t, uint64_t> numaBorrowedSizeMap;
    vector<uint16_t> remoteNumaIds = borrowExecuteResult.presentNumaIds;
    // get the Memory Account Borrowing Size
    VmResult ret = MemHandler::GetBorrowedSizeMap(remoteNumaIds, numaBorrowedSizeMap);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to get memInfo from mem, " << FormatRetCode(ret);
        return ret;
    }
    ret = ChangeOverrideCapability(true);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to call ChangeOverrideCapability.";
        (void)ChangeOverrideCapability(false);
        return VM_ERROR;
    }
    // Set source node remote hugepage memory
    for (const auto& [key, value] : numaBorrowedSizeMap) {
        // When value is 0, no need to set hugePage.
        if (value == 0) {
            continue;
        }
        ret = HugePageHandler::SetHugePages(key, value);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Failed to set hugepages"
                           << ", remoteNumaId=" << key << FormatRetCode(ret);
            (void)ChangeOverrideCapability(false);
            return ret;
        }
    }
    (void)ChangeOverrideCapability(false);
    return ret;
}

void FillVMMemoryBorrowParam(const borrow_strategy_c& borrowMsg, const uid_t uid, VMMemoryBorrowParam& vmParam)
{
    vmParam.srcParam.srcNid = borrowMsg.src_host_id;
    vmParam.srcParam.srcSocketId = borrowMsg.src_socket_id;
    vmParam.srcParam.srcNumaId = borrowMsg.src_numa_id;
    vmParam.srcParam.uid = uid;
    vmParam.srcParam.username = VmSystemUtil::GetUsernameByUid(uid);
    for (uint32_t i = 0; i < borrowMsg.dest_numa_infos_size; i++) {
        DestMemoryBorrowParam destParam;
        destParam.destNid = borrowMsg.dest_numa_infos[i].host_id;
        destParam.destSocketId = borrowMsg.dest_numa_infos[i].socket_id;
        destParam.destNumaNum = borrowMsg.dest_numa_infos[i].numa_nums;
        for (uint16_t j = 0; j < destParam.destNumaNum; j++) {
            destParam.destNumaId.push_back(borrowMsg.dest_numa_infos[i].numa_ids[j]);
            destParam.memSize.push_back(borrowMsg.dest_numa_infos[i].mem_sizes[j]);
        }
        vmParam.destParam.push_back(destParam);
    }
    UBSE_LOG_DEBUG << "MemBorrowExecute srcParam=" << vmParam.srcParam.ToString();
    for (auto param : vmParam.destParam) {
        UBSE_LOG_DEBUG << "MemBorrowExecute destParam=" << param.ToString();
    }
}

uint32_t PackMemBorrowRespResult(const mem_borrow_result_c& memBorrowExecuteResult, UbseIpcMessage& buffer)
{
    MemBorrowExecuteResultMsg msg{memBorrowExecuteResult};
    auto ret = msg.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemBorrowExecuteResultMsg Serialize fail, " << FormatRetCode(ret);
        return ret;
    }
    buffer.buffer = msg.SerializedData();
    if (buffer.buffer == nullptr) {
        UBSE_LOG_ERROR << "MemBorrowExecuteResultMsg new buffer failed.";
        return VM_ERROR;
    }
    buffer.length = msg.SerializedDataSize();
    if (buffer.length == 0) {
        UBSE_LOG_ERROR << "The length of MemBorrowExecuteResultMsg is 0.";
        return VM_ERROR;
    }
    return VM_OK;
}

uint32_t VirtMemFragSdk::PackBorrowExecuteRsp(const MemBorrowExecuteResult& memBorrowExecuteResult,
                                              UbseIpcMessage& buffer)
{
    MemFragmentationMemBorrowExecuteOutputMsg msg{memBorrowExecuteResult};
    auto ret = msg.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemFragmentationMemBorrowExecuteMsg Serialize fail, " << FormatRetCode(ret);
        return ret;
    }
    buffer.buffer = msg.SerializedData();
    if (buffer.buffer == nullptr) {
        UBSE_LOG_ERROR << "MemFragmentationMemBorrowExecuteMsg new buffer failed.";
        return VM_ERROR;
    }
    buffer.length = msg.SerializedDataSize();
    if (buffer.length == 0) {
        UBSE_LOG_ERROR << "The length of MemFragmentationMemBorrowExecuteMsg is 0.";
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult ConvertToLegacyResult(const MemBorrowExecuteResult& src, mem_borrow_result_c& dest)
{
    auto ret =
        memset_s(dest.present_numa_ids_ptr, sizeof(dest.present_numa_ids_ptr), 0, sizeof(dest.present_numa_ids_ptr));
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to initialize mem_borrow_result_c, " << ret;
        return VM_ERROR_NOMEM;
    }

    ret = memset_s(dest.borrow_ids_ptr, sizeof(dest.borrow_ids_ptr), 0, sizeof(dest.borrow_ids_ptr));
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to initialize mem_borrow_result_c, ret code=" << ret;
        return VM_ERROR_NOMEM;
    }

    dest.borrow_ids_size = std::min(static_cast<uint32_t>(src.borrowIds.size()), MAX_BORROW_ID_COUNT);
    for (uint32_t i = 0; i < dest.borrow_ids_size; i++) {
        auto ret = StringToC(dest.borrow_ids_ptr[i], src.borrowIds[i], MAX_BORROW_ID_LENGTH);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "StringToC failed for borrow_id=" << src.borrowIds[i];
            return ret;
        }
    }

    dest.present_numa_ids_size = std::min(static_cast<uint32_t>(src.presentNumaIds.size()), MAX_BORROW_ID_COUNT);
    for (uint32_t i = 0; i < dest.present_numa_ids_size; i++) {
        dest.present_numa_ids_ptr[i] = src.presentNumaIds[i];
    }

    return VM_OK;
}

VmResult CallUBSRMRSMemBorrowExecute(const VMMemoryBorrowParam& vmParam, MemBorrowExecuteResult& borrowExecuteResult)
{
    const auto UBSRMRSMemBorrowExecute = MempoolingModule::UBSRMRSMemBorrowExecute();
    if (UBSRMRSMemBorrowExecute == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSMemBorrowExecute ptr failed.";
        return VM_ERROR;
    }
    try {
        auto ret = UBSRMRSMemBorrowExecute(vmParam.srcParam, vmParam.destParam, borrowExecuteResult);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Call UBSRMRSMemBorrowExecute fail. " << FormatRetCode(ret);
            return ret;
        }
        if (borrowExecuteResult.borrowIds.size() > MAX_BORROW_ID_COUNT ||
            borrowExecuteResult.borrowIds.size() != borrowExecuteResult.presentNumaIds.size()) {
            UBSE_LOG_ERROR << "The size of presentNumaIds or borrowIds is invalid.";
            return VM_INVALID_PARAM_ERROR;
        }
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Call UBSRMRSMemBorrowStrategy Exception. " << e.what();
        return VM_ERROR;
    }
    return VM_OK;
}

uint32_t VirtMemFragSdk::StartMemBorrowSync(const std::string& taskId, const VMMemoryBorrowParam& vmParam,
                                            const UbseRequestContext& context)
{
    UBSE_LOG_INFO << "MemBorrowExecuteSync start, taskId=" << taskId;
    ThreadTaskManager::GetInstance().SetTaskThreadId(taskId);
    mem_borrow_result_c borrowResult{};
    MemBorrowExecuteResult borrowExecuteResult;

    auto ret = StringToC(borrowResult.task_id, taskId, MEM_TASK_ID_MAX);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "StringToC failed for taskId, taskId=" << taskId;
        ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, ret, "StringToC failed");
        return ret;
    }

    ret = CallUBSRMRSMemBorrowExecute(vmParam, borrowExecuteResult);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "CallUBSRMRSMemBorrowExecute failed, taskId=" << taskId << ", " << FormatRetCode(ret);
        ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, ret,
                                                          "CallUBSRMRSMemBorrowExecute failed");
        return ret;
    }

    ret = ConvertToLegacyResult(borrowExecuteResult, borrowResult);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Convert to legacy result failed, taskId=" << taskId;
        ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, ret, "Data convert failed");
        return ret;
    }

    ThreadTaskManager::GetInstance().SetMemBorrowResult(taskId, borrowResult);

    ret = SetSrcNodeHugePage(borrowExecuteResult);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to set hugepages, hostId=" << vmParam.srcParam.srcNid << ", taskId=" << taskId;
        ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, ret, "SetHugePage failed");
        return ret;
    }

    UbseIpcMessage resp{};
    ret = PackMemBorrowRespResult(borrowResult, resp);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemBorrowExecuteSync pack failed, taskId=" << taskId;
        SafeDeleteArray(resp.buffer);
        ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, ret, "PackResult failed");
        return ret;
    }

    ret = SendResponse(VM_OK, context.requestId, resp);
    SafeDeleteArray(resp.buffer);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemBorrowExecuteSync response send failed, " << FormatRetCode(ret) << ", taskId=" << taskId;
        ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, ret, "SendResponse failed");
        return ret;
    }

    ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::SUCCESS, VM_OK);
    return VM_OK;
}

uint32_t VirtMemFragSdk::StartMemBorrowAsync(const std::string& taskId, const VMMemoryBorrowParam& vmParam,
                                             const UbseRequestContext& context)
{
    UBSE_LOG_INFO << "MemBorrowExecuteAsync start, taskId=" << taskId << ", requestId=" << context.requestId;

    ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::RUNNING, VM_OK);
    try {
        std::thread([](const std::string& taskId, const VMMemoryBorrowParam& vmParam,
                       const UbseRequestContext& ctx) { ExecuteAsyncMemBorrow(taskId, vmParam, ctx); },
                    taskId, vmParam, context)
            .detach();
    } catch (const std::exception& e) {
        std::string errorMsg = std::string("Exception in create Thread: ") + e.what();
        UBSE_LOG_ERROR << "StartMemBorrowAsync Exception, taskId=" << taskId << ", error=" << e.what();
        ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, VM_ERROR, errorMsg);
        return VM_ERROR;
    }

    mem_borrow_result_c borrowResult{};
    auto ret = StringToC(borrowResult.task_id, taskId, MEM_TASK_ID_MAX);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "StringToC failed for taskId, taskId=" << taskId;
        ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, ret, "StringToC failed");
        return ret;
    }

    UbseIpcMessage resp{};
    ret = PackMemBorrowRespResult(borrowResult, resp);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemBorrowExecuteSync pack failed, taskId=" << taskId;
        SafeDeleteArray(resp.buffer);
        ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, ret,
                                                          "PackMemBorrowResult failed");
        return ret;
    }
    ret = SendResponse(VM_OK, context.requestId, resp);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemBorrowExecuteAsync response send failed, " << FormatRetCode(ret) << ", taskId=" << taskId;
        ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, ret, "SendResponse failed");
        return ret;
    }

    UBSE_LOG_INFO << "MemBorrowExecuteAsync end (async operation in progress), taskId=" << taskId;
    return VM_OK;
}

void VirtMemFragSdk::ExecuteAsyncMemBorrow(const std::string& taskId, const VMMemoryBorrowParam& vmParam,
                                           const UbseRequestContext& context)
{
    ThreadTaskManager::GetInstance().SetTaskThreadId(taskId);
    UBSE_LOG_INFO << "Start async MemBorrow operation, taskId=" << taskId << ", requestId=" << context.requestId
                  << ", nodeId=" << vmParam.srcParam.srcNid;
    uint32_t resultCode = VM_OK;
    std::string errorMsg;
    mem_borrow_result_c borrowResult{};
    errno_t ret = memset_s(&borrowResult, sizeof(borrowResult), 0, sizeof(borrowResult));
    if (ret != 0) {
        UBSE_LOG_ERROR << "memset_s failed for borrowResult, ret=" << ret;
        return;
    }

    MemBorrowExecuteResult borrowExecuteResult;
    resultCode = CallUBSRMRSMemBorrowExecute(vmParam, borrowExecuteResult);
    if (resultCode != VM_OK) {
        errorMsg = "CallUBSRMRSMemBorrowExecute failed with code: " + std::to_string(resultCode);
        UBSE_LOG_ERROR << "Async CallUBSRMRSMemBorrowExecute failed, taskId=" << taskId << ", error=" << errorMsg;

        ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, resultCode, errorMsg);
        return;
    }

    resultCode = SetSrcNodeHugePage(borrowExecuteResult);
    if (resultCode != VM_OK) {
        errorMsg = "SetSrcNodeHugePage failed with code: " + std::to_string(resultCode);
        UBSE_LOG_ERROR << "Async SetSrcNodeHugePage failed, taskId=" << taskId << ", error" << errorMsg;

        ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, resultCode, errorMsg);
        return;
    }

    resultCode = ConvertToLegacyResult(borrowExecuteResult, borrowResult);
    if (resultCode != VM_OK) {
        errorMsg = "ConvertToNewResult failed with code: " + std::to_string(resultCode);
        UBSE_LOG_ERROR << "Async ConvertToNewResult failed, taskId=" << taskId << ", error=" << errorMsg;

        ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, resultCode, errorMsg);
        return;
    }

    ThreadTaskManager::GetInstance().SetMemBorrowResult(taskId, borrowResult);

    UBSE_LOG_INFO << "Async MemBorrow operation completed successfully, taskId=" << taskId;
    ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::SUCCESS, VM_OK);
}

uint32_t VirtMemFragSdk::MemBorrowExecute(const UbseIpcMessage& req, const UbseRequestContext& context)
{
    UBSE_LOG_INFO << "MemBorrowExecute start.";
    if (!ValidateRequest(req)) {
        UBSE_LOG_ERROR << "Request validation failed.";
        return VM_ERROR;
    }
    MemBorrowSettingMsg inputMsg(req.buffer, req.length);
    auto ret = inputMsg.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemBorrowExecuteInputMsg deserialize failed, " << FormatRetCode(ret);
        return VM_ERROR_DESERIALIZE_ERROR;
    }
    borrow_setting_c borrowSetting = inputMsg.GetMemBorrowSettingMsg();

    borrow_strategy_c tmpMsg = borrowSetting.borrow_strategy;
    bool isAsync = borrowSetting.isAsync;

    VMMemoryBorrowParam vmParam{};
    FillVMMemoryBorrowParam(tmpMsg, context.clientInfo.uid, vmParam);

    ThreadTaskManager::GetInstance().CleanupCompletedTasks();
    std::string taskId = ThreadTaskManager::GetInstance().AddTask("memborrow");
    if (taskId.empty()) {
        UBSE_LOG_ERROR << "Failed to create task for nodeId=" << vmParam.srcParam.srcNid;
        return VM_ERROR;
    }
    if (isAsync) {
        return StartMemBorrowAsync(taskId, vmParam, context);
    } else {
        return StartMemBorrowSync(taskId, vmParam, context);
    }
}

VmResult ConvertTaskResult(const AsyncTaskInfo& from, async_task_info_c& to)
{
    // 1. task_id
    VmResult ret = StringToC(to.task_id, from.taskId, MEM_TASK_ID_MAX);
    if (ret != VM_OK) {
        return ret;
    }

    // 2. status
    to.status = static_cast<async_task_status_c>(from.status);

    // 3. resultCode
    to.resultCode = from.resultCode;
    // 4. memBorrowResult
    if (from.msgRawData != nullptr && from.msgRawDataSize > 0) {
        MemBorrowExecuteResultMsg msg(from.msgRawData, from.msgRawDataSize);
        VmResult deserializeRet = msg.Deserialize();
        if (deserializeRet != VM_OK) {
            return VM_ERROR_DESERIALIZE_ERROR;
        }
        mem_borrow_result_c result = msg.GetBorrowResult();
        to.memBorrowResult.borrow_ids_size = result.borrow_ids_size;
        to.memBorrowResult.present_numa_ids_size = result.present_numa_ids_size;

        if (result.borrow_ids_size > MAX_BORROW_ID_COUNT) {
            UBSE_LOG_ERROR << "borrow_ids_size is out of range.";
            return VM_ERROR_INVAL;
        }
        // copy borrow_ids_ptr
        for (uint32_t i = 0; i < result.borrow_ids_size; ++i) {
            errno_t ret =
                strcpy_s(to.memBorrowResult.borrow_ids_ptr[i], MAX_BORROW_ID_LENGTH, result.borrow_ids_ptr[i]);
            if (ret != 0) {
                UBSE_LOG_ERROR << "Failed to copy borrow ID string from MemBorrowExecuteResultMsg.";
                return VM_ERROR_INVAL;
            }
        }

        // copy present_numa_ids_ptr
        if (result.present_numa_ids_size > MAX_BORROW_ID_COUNT) {
            UBSE_LOG_ERROR << "borrow_ids_size is out of range.";
            return VM_ERROR_INVAL;
        }
        for (uint32_t i = 0; i < result.present_numa_ids_size; ++i) {
            to.memBorrowResult.present_numa_ids_ptr[i] = result.present_numa_ids_ptr[i];
        }

        // task_id
        ret = StringToC(to.memBorrowResult.task_id, from.taskId, MEM_TASK_ID_MAX);
        if (ret != VM_OK) {
            return ret;
        }
    } else {
        to.memBorrowResult.borrow_ids_size = 0;
        to.memBorrowResult.present_numa_ids_size = 0;
        to.memBorrowResult.task_id[0] = '\0';
    }

    return VM_OK;
}

uint32_t VirtMemFragSdk::MemTaskQuery(const UbseIpcMessage& req, const UbseRequestContext& context)
{
    UBSE_LOG_INFO << "MemTaskQuery start.";
    if (!ValidateRequest(req)) {
        UBSE_LOG_ERROR << "Request validation failed.";
        return VM_ERROR;
    }
    std::string taskId(reinterpret_cast<char*>(req.buffer), req.length);
    UBSE_LOG_INFO << "MemTaskQuery taskId=" << taskId;

    AsyncTaskInfo taskInfo{};
    async_task_info_c taskInfoC{};
    taskInfoC.status = ASYNC_TASK_NOT_EXIST;
    taskInfoC.resultCode = 0;
    taskInfoC.memBorrowResult = {};
    uint32_t ret = ThreadTaskManager::GetInstance().GetTaskInfo(taskId, taskInfo);
    if (ret != VM_OK) {
        UBSE_LOG_WARN << "Task not found or query failed, taskId=" << taskId;
        VmResult ret = StringToC(taskInfoC.task_id, taskId, MEM_TASK_ID_MAX);
        if (ret != VM_OK) {
            return ret;
        }
    } else {
        VmResult convertRet = ConvertTaskResult(taskInfo, taskInfoC);
        if (convertRet != VM_OK) {
            UBSE_LOG_ERROR << "ConvertTaskResult failed for taskId=" << taskId << ", ret=" << FormatRetCode(convertRet);
            return VM_ERROR;
        }
    }

    std::ostringstream oss;
    oss << "async_task_info_c: task_id=" << taskInfoC.task_id << ", status=" << taskInfoC.status
        << ", resultCode=" << taskInfoC.resultCode << ", borrow_ids_size=" << taskInfoC.memBorrowResult.borrow_ids_size
        << ", numa_ids_size=" << taskInfoC.memBorrowResult.present_numa_ids_size;

    UBSE_LOG_INFO << oss.str();

    MemTaskResultQueryMsg msg(taskInfoC);
    VmResult serializeRet = msg.Serialize();
    if (serializeRet != VM_OK) {
        UBSE_LOG_ERROR << "Failed to serialize task info for taskId=" << taskId << ", " << FormatRetCode(serializeRet);
        return VM_ERROR;
    }

    UbseIpcMessage resp{};
    resp.length = msg.SerializedDataSize();
    resp.buffer = msg.SerializedData();
    ret = SendResponse(VM_OK, context.requestId, resp);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemMigrateStrategy response send failed, " << FormatRetCode(ret);
        return ret;
    }

    return VM_OK;
}

uint32_t VirtMemFragSdk::PackMigrateStrategyRsp(const MigrateStrategyResult& migrateStrategyResult,
                                                UbseIpcMessage& buffer)
{
    MemFragmentationMemMigrateStrategyOutputMsg msg{};
    auto ret = msg.SetOutputMsg(migrateStrategyResult);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemFragmentationMemMigrateMsg SetOutputMsg fail, " << ret;
        return ret;
    }
    ret = msg.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemFragmentationMemMigrateMsg Serialize fail, " << ret;
        return ret;
    }
    buffer.buffer = msg.SerializedData();
    if (buffer.buffer == nullptr) {
        UBSE_LOG_ERROR << "MemFragmentationMemMigrateMsg new buffer failed.";
        return VM_ERROR;
    }
    buffer.length = msg.SerializedDataSize();
    if (buffer.length == 0) {
        UBSE_LOG_ERROR << "MemFragmentationMemMigrateMsg memcpy_s failed.";
        return VM_ERROR;
    }
    return VM_OK;
}

void FillVmMigrateparam(const MemMigrateStrategySrcParam& srcParam, VMMigrateParam& vmParam)
{
    vmParam.borrowInNode = srcParam.borrowInNode;
    vmParam.borrowSize = srcParam.borrowSize;
    if (srcParam.vmInfoList == nullptr) {
        return;
    }
    for (uint32_t i = 0; i < srcParam.vmInfoListSize; i++) {
        VMPresetParam param;
        param.pid = srcParam.vmInfoList[i].pid;
        param.ratio = srcParam.vmInfoList[i].ratio;
        vmParam.vmInfoList.push_back(param);
    }
}

VmResult CallUBSRMRSMigrateStrategy(const VMMigrateParam& vmParam, MigrateStrategyResult& migrateStrategyResult)
{
    const auto UBSRMRSMigrateStrategy = MempoolingModule::UBSRMRSMigrateStrategy();
    if (UBSRMRSMigrateStrategy == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSMigrateStrategy failed.";
        return VM_ERROR;
    }
    UBSE_LOG_DEBUG << "UBSRMRSMigrateStrategy borrowInNode=" << vmParam.borrowInNode
                   << ", borrowSize=" << vmParam.borrowSize << ", vmInfoListSize=" << vmParam.vmInfoList.size();
    try {
        auto ret =
            UBSRMRSMigrateStrategy(vmParam.borrowInNode, vmParam.vmInfoList, vmParam.borrowSize, migrateStrategyResult);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Call UBSRMRSMigrateStrategy fail. ret=" << ret;
            return VM_ERROR;
        }
        // Maximum number of virtual machines: MAX_VM_NUM
        if (migrateStrategyResult.vmInfoList.size() > MAX_VM_NUM) {
            UBSE_LOG_ERROR << "The size of vmInfoList is " << migrateStrategyResult.vmInfoList.size()
                           << ", which is more than " << MAX_VM_NUM << ".";
            return VM_ERROR;
        }
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Call UBSRMRSMigrateStrategy Exception. ret=" << e.what();
        return VM_ERROR;
    }
    return VM_OK;
}

uint32_t VirtMemFragSdk::MemMigrateStrategy(const UbseIpcMessage& req, const UbseRequestContext& context)
{
    UBSE_LOG_INFO << "MemMigrateStrategy start.";
    if (!ValidateRequest(req)) {
        UBSE_LOG_ERROR << "Request validation failed.";
        return VM_ERROR;
    }
    MemFragmentationMemMigrateStrategyInputMsg inputMsg(req.buffer, req.length);
    auto ret = inputMsg.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemMigrateStrategyInputMsg deserialize failed, " << FormatRetCode(ret);
        return VM_ERROR_DESERIALIZE_ERROR;
    }
    MemMigrateStrategySrcParam srcParam = inputMsg.GetInputMsg();
    VMMigrateParam vmParam;
    MigrateStrategyResult migrateStrategyResult;
    FillVmMigrateparam(srcParam, vmParam);
    ret = CallUBSRMRSMigrateStrategy(vmParam, migrateStrategyResult);
    if (ret != VM_OK) {
        return ret;
    }
    // pack
    UbseIpcMessage resp{};
    ret = PackMigrateStrategyRsp(migrateStrategyResult, resp);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MigrateStrategyResult pack failed, " << FormatRetCode(ret);
        SafeDeleteArray(resp.buffer);
        return ret;
    }
    // return data
    ret = SendResponse(VM_OK, context.requestId, resp);
    SafeDeleteArray(resp.buffer);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemMigrateStrategy response send failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "MemMigrateStrategy end.";
    return VM_OK;
}

uint32_t MigrateExecuteParamResolve(const MemMigrateExecuteSrcParam& srcParam, uint64_t& waitingTime,
                                    std::vector<std::string>& borrowIds, std::string& borrowInNode,
                                    std::vector<VMMigrateOutParam>& vmMigrateOutParamList)
{
    waitingTime = srcParam.waitingTime;
    for (uint32_t i = 0; i < srcParam.borrowIdsCount; i++) {
        std::string borrowId = srcParam.borrowIds[i];
        borrowIds.push_back(borrowId);
    }
    borrowInNode = srcParam.borrowInNode;
    for (uint32_t i = 0; i < srcParam.vmInfoListSize; i++) {
        VMMigrateOutParam outParam;
        outParam.desNumaId = srcParam.vmInfoList[i].destNumaId;
        outParam.memSize = srcParam.vmInfoList[i].memSize;
        outParam.pid = srcParam.vmInfoList[i].pid;
        vmMigrateOutParamList.push_back(outParam);
    }
    return VM_OK;
}

uint32_t VirtMemFragSdk::MemMigrateExecute(const UbseIpcMessage& req, const UbseRequestContext& context)
{
    UBSE_LOG_INFO << "MemMigrateExecute start.";
    if (!ValidateRequest(req)) {
        UBSE_LOG_ERROR << "Request validation failed.";
        return VM_ERROR;
    }
    MemFragmentationMemMigrateExecuteInputMsg inputMsg(req.buffer, req.length);
    auto ret = inputMsg.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemMigrateExecuteInputMsg deserialize failed. " << FormatRetCode(ret);
        return VM_ERROR_DESERIALIZE_ERROR;
    }
    MemMigrateExecuteSrcParam srcParam = inputMsg.GetInputMsg();
    uint64_t waitingTime;
    std::vector<std::string> borrowIds;
    std::string borrowInNode;
    std::vector<VMMigrateOutParam> vmMigrateOutParam;
    // parse
    ret = MigrateExecuteParamResolve(srcParam, waitingTime, borrowIds, borrowInNode, vmMigrateOutParam);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Param parse failed. " << FormatRetCode(ret);
        return VM_ERROR;
    }
    const auto UBSRMRSMigrateExecute = MempoolingModule::UBSRMRSMigrateExecute();
    if (UBSRMRSMigrateExecute == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSMigrateExecute ptr fail. ret=" << ret;
        return VM_ERROR;
    }
    try {
        ret = UBSRMRSMigrateExecute(borrowInNode, vmMigrateOutParam, waitingTime, borrowIds);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Call UBSRMRSMigrateExecute fail. ret=" << ret;
            return VM_ERROR;
        }
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Call UBSRMRSMigrateExecute Exception. ret=" << e.what();
        return VM_ERROR;
    }
    UbseIpcMessage resp{};
    ret = SendResponse(VM_OK, context.requestId, resp);
    SafeDeleteArray(resp.buffer);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemMigrateExecute response send failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "MemMigrateExecute end.";
    return VM_OK;
}

bool PackTaskIdToResponse(const std::string& taskId, UbseIpcMessage& resp)
{
    if (taskId.length() > std::numeric_limits<uint32_t>::max()) {
        UBSE_LOG_ERROR << "taskId length is out of range.";
        return false;
    }

    resp.length = static_cast<uint32_t>(taskId.length());
    resp.buffer = new (std::nothrow) uint8_t[resp.length + 1];
    if (resp.buffer == nullptr) {
        UBSE_LOG_ERROR << "Failed to allocate memory for response buffer.";
        return false;
    }

    errno_t copy_result = memcpy_s(resp.buffer, resp.length, taskId.c_str(), taskId.length());
    if (copy_result != 0) {
        UBSE_LOG_ERROR << "memcpy_s failed for taskId=" << taskId;
        SafeDeleteArray(resp.buffer);
        return false;
    }

    reinterpret_cast<char*>(resp.buffer)[resp.length] = '\0';
    return true;
}

void VirtMemFragSdk::ExecuteAsyncMemReturn(const std::string& taskId, const std::string& nodeId,
                                           const UbseRequestContext& context)
{
    ThreadTaskManager::GetInstance().SetTaskThreadId(taskId);
    UBSE_LOG_INFO << "Start async MemFree operation, taskId=" << taskId << ", nodeId=" << nodeId;
    uint32_t resultCode = VM_OK;
    std::string errorMsg;

    try {
        const auto UBSRMRSMemFree = MempoolingModule::UBSRMRSMemFree();
        if (UBSRMRSMemFree == nullptr) {
            throw std::runtime_error("UBSRMRSMemFree function pointer is null");
        }

        resultCode = UBSRMRSMemFree(nodeId);
        if (resultCode != VM_OK) {
            errorMsg = "UBSRMRSMemFree failed with code: " + std::to_string(resultCode);
            UBSE_LOG_ERROR << "Call UBSRMRSMemFree fail. ret=" << resultCode;
            ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, resultCode, errorMsg);
        } else {
            UBSE_LOG_INFO << "Async MemFree operation completed successfully";
            ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::SUCCESS, VM_OK);
        }
    } catch (const std::exception& e) {
        errorMsg = std::string("Exception in UBSRMRSMemFree: ") + e.what();
        UBSE_LOG_ERROR << "Call UBSRMRSMemFree Exception. " << e.what();
        ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, VM_ERROR, errorMsg);
    }
}

uint32_t VirtMemFragSdk::StartMemReturnAsync(const std::string& taskId, const std::string& nodeId,
                                             const UbseRequestContext& context)
{
    try {
        std::thread([taskId, nodeId, context]() { ExecuteAsyncMemReturn(taskId, nodeId, context); }).detach();
    } catch (const std::exception& e) {
        std::string errorMsg = std::string("Exception in create Thread: ") + e.what();
        UBSE_LOG_ERROR << "StartMemReturnAsync Exception, taskId=" << taskId << ", error=" << e.what();
        ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, VM_ERROR, errorMsg);
        return VM_ERROR;
    }
    UbseIpcMessage resp{};
    if (!PackTaskIdToResponse(taskId, resp)) {
        UBSE_LOG_ERROR << "Failed to pack taskId to response for nodeId=" << nodeId;
        return VM_ERROR;
    }
    auto ret = SendResponse(VM_OK, context.requestId, resp);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemReturn response send failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "MemReturn end (async operation in progress), taskId=" << taskId;
    return VM_OK;
}

uint32_t VirtMemFragSdk::StartMemReturnSync(const std::string& taskId, const std::string& nodeId,
                                            const UbseRequestContext& context)
{
    UBSE_LOG_INFO << "Executing MemReturn synchronously for nodeId=" << nodeId << ", taskId=" << taskId;

    uint32_t resultCode = VM_OK;
    std::string errorMsg;

    try {
        const auto UBSRMRSMemFree = MempoolingModule::UBSRMRSMemFree();
        if (UBSRMRSMemFree == nullptr) {
            throw std::runtime_error("UBSRMRSMemFree function pointer is null");
        }

        resultCode = UBSRMRSMemFree(nodeId);
        if (resultCode != VM_OK) {
            errorMsg = "UBSRMRSMemFree failed with code: " + std::to_string(resultCode);
            UBSE_LOG_ERROR << "Call UBSRMRSMemFree fail. ret=" << resultCode;
            ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, resultCode, errorMsg);
        } else {
            UBSE_LOG_INFO << "Sync MemReturn operation completed successfully for nodeId=" << nodeId;
            ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::SUCCESS, VM_OK);
        }
    } catch (const std::exception& e) {
        errorMsg = std::string("Exception in UBSRMRSMemFree: ") + e.what();
        UBSE_LOG_ERROR << "Call UBSRMRSMemFree Exception. " << e.what();
        resultCode = VM_ERROR;
        ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId, AsyncTaskStatus::FAILED, VM_ERROR, errorMsg);
    }

    UbseIpcMessage resp{};
    if (!PackTaskIdToResponse(taskId, resp)) {
        UBSE_LOG_ERROR << "Failed to pack taskId to response for nodeId=" << nodeId;
        return VM_ERROR;
    }
    auto ret = SendResponse(resultCode, context.requestId, resp);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemReturn response send failed, " << FormatRetCode(ret);
        return ret;
    }

    UBSE_LOG_INFO << "MemReturn sync operation completed, taskId=" << taskId;
    return VM_OK;
}

uint32_t VirtMemFragSdk::MemReturn(const UbseIpcMessage& req, const UbseRequestContext& context)
{
    UBSE_LOG_INFO << "MemReturn start.";
    UbseIpcMessage resp{};
    if (!ValidateRequest(req)) {
        UBSE_LOG_ERROR << "Request validation failed.";
        return VM_ERROR;
    }

    if (req.length != sizeof(bool)) {
        UBSE_LOG_ERROR << "Invalid request length, expected sizeof(bool)=" << sizeof(bool) << ", but got "
                       << req.length;
        return VM_ERROR;
    }

    bool isAsync = false;
    if (req.buffer != nullptr) {
        isAsync = *reinterpret_cast<const bool*>(req.buffer);
    }

    auto nodeId = VmConfiguration::GetInstance().GetNodeId();
    if (nodeId.empty()) {
        UBSE_LOG_ERROR << "Failed to get nodeId from VmConfiguration.";
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "MemReturn operation, nodeId=" << nodeId << ", isAsync=" << (isAsync ? "true" : "false");

    ThreadTaskManager::GetInstance().CleanupCompletedTasks();
    std::string taskId = ThreadTaskManager::GetInstance().AddTask("memreturn");
    if (taskId.empty()) {
        UBSE_LOG_ERROR << "Failed to create task for nodeId=" << nodeId;
        return VM_ERROR;
    }

    if (isAsync) {
        return StartMemReturnAsync(taskId, nodeId, context);
    } else {
        return StartMemReturnSync(taskId, nodeId, context);
    }
}

uint32_t VirtMemFragSdk::MemRollback(const UbseIpcMessage& req, const UbseRequestContext& context)
{
    UBSE_LOG_INFO << "MemRollback start.";
    if (!ValidateRequest(req)) {
        UBSE_LOG_ERROR << "Request validation failed.";
        return VM_ERROR;
    }
    UbseIpcMessage resp{};
    // Parsing input
    MemRollbackMsg memRollbackMsg(req.buffer, req.length);
    auto ret = memRollbackMsg.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Mem migrate message deserialize failed. " << FormatRetCode(ret);
        return VM_ERROR_DESERIALIZE_ERROR;
    }
    RollbackParams inputParams = memRollbackMsg.GetRollbackParams();
    UBSE_LOG_INFO << "node_id=" << inputParams.node_id << ", borrow_id_size=" << inputParams.borrow_id_size;
    if (!inputParams.borrow_id_list.empty()) {
        for (uint32_t i = 0; i < inputParams.borrow_id_size; i++) {
            UBSE_LOG_INFO << "borrow_id_list=" << inputParams.borrow_id_list[i];
        }
    }
    const auto UBSRMRSMemBorrowRollback = MempoolingModule::UBSRMRSMemBorrowRollback();
    if (UBSRMRSMemBorrowRollback == nullptr) {
        UBSE_LOG_ERROR << "UBSRMRSMemBorrowRollback ptr is nullptr.";
        return VM_ERROR;
    }

    try {
        if (const auto ret = UBSRMRSMemBorrowRollback(inputParams.node_id, inputParams.borrow_id_list); ret != VM_OK) {
            UBSE_LOG_ERROR << "Call UBSRMRSMemBorrowRollback fail, " << ret;
            return VM_ERROR;
        }
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Call UBSRMRSMemBorrowRollback Exception. ret=" << e.what();
        return VM_ERROR;
    }
    ret = SendResponse(VM_OK, context.requestId, resp);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << " MemRollback response send failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "MemRollback end.";
    return VM_OK;
}

} // namespace vm