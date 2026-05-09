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

#include "container_sdk_server.h"

#include <string>

#include <ubse_api_server.h>
#include <ubse_logger.h>

#include "container_service.h"
#include "mem_container_msg.h"
#include "os_helper.h"
#include "ubs_virt_agent_object_def.h"
#include "vm_sdk_def.h"
#include "vm_system_util.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;
using namespace api::server;
using namespace vm::overcommit;

VmResult VirtContainerSdk::Register()
{
    auto ret = RegisterIpcHandler(UBS_VA_CONTAINER, UBS_VA_CONTAINER_GET_MEM_INFO_FOR_PID, GetMemInfoForPid,
                                  UBS_VA_CONTAINER_PERMISSION);
    ret |= RegisterIpcHandler(UBS_VA_CONTAINER, UBS_VA_CONTAINER_UPDATE_WATERLINE, InjectWaterLineHandler,
                              UBS_VA_CONTAINER_PERMISSION);
    ret |= RegisterIpcHandler(UBS_VA_CONTAINER, UBS_VA_CONTAINER_GET_CONTAINER_PIDS, GetContainerPidsHandler,
                              UBS_VA_CONTAINER_PERMISSION);
    ret |= RegisterIpcHandler(UBS_VA_CONTAINER, UBS_VA_CONTAINER_WATERLINE_MEM_BORROW, WaterLineMemBorrow,
                              UBS_VA_CONTAINER_PERMISSION);
    ret |= RegisterIpcHandler(UBS_VA_CONTAINER, UBS_VA_CONTAINER_WATERLINE_MEM_MIGRATE, WaterLineMemMigrate,
                              UBS_VA_CONTAINER_PERMISSION);
    ret |= RegisterIpcHandler(UBS_VA_CONTAINER, UBS_VA_CONTAINER_WATERLINE_MEM_RETURN, WaterLineMemReturn,
                              UBS_VA_CONTAINER_PERMISSION);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Registration of VirtContainerSdk failed. " << FormatRetCode(ret);
        return ret;
    }

    UBSE_LOG_DEBUG << "VirtContainerSdk registration succeed.";
    return VM_OK;
}

VmResult GetContainerMemInfoWithStructure(const SrcMemoryBorrowParam &borrowParam, const std::vector<pid_t> &pids,
                                          std::vector<PidInfo> &pidInfos)
{
    // Get NUMA information collection function
    const auto UBSRMRSPidNumaInfoCollect = MempoolingModule::UBSRMRSPidNumaInfoCollect();
    if (UBSRMRSPidNumaInfoCollect == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSPidNumaInfoCollect function failed.";
        return VM_ERROR;
    }
    try {
        // Collect PID memory information
        VmResult ret = UBSRMRSPidNumaInfoCollect(borrowParam, pids, pidInfos);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "UBSRMRSPidNumaInfoCollect failed, " << FormatRetCode(ret);
            return VM_ERROR;
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "UBSRMRSPidNumaInfoCollect exception: " << e.what();
        return VM_ERROR;
    }

    return VM_OK;
}

VmResult InjectWaterLine(const WaterMark &waterMark)
{
    UBSE_LOG_DEBUG << "Start to inject waterLine.";
    double highWaterMark = waterMark.highWaterMark;
    double lowWaterMark = waterMark.lowWaterMark;
    if (highWaterMark != static_cast<int>(highWaterMark) || lowWaterMark != static_cast<int>(lowWaterMark)) {
        UBSE_LOG_ERROR << "waterMark must be an integer.";
        return VM_ERROR;
    }
    // 100 is the maximum watermark.
    if ((lowWaterMark < 0 || highWaterMark > 100) || (highWaterMark < lowWaterMark)) {
        UBSE_LOG_ERROR << "The lowWaterMark and highWaterMark must range from 0 to 100, and the lowWaterMark must be "
                          "less than the highWaterMark.";
        return VM_ERROR;
    }

    const auto UBSRMRSSetWaterMark = MempoolingModule::UBSRMRSSetWaterMark();
    if (UBSRMRSSetWaterMark == nullptr) {
        UBSE_LOG_ERROR << "load mempooling UBSRMRSSetWaterMark func failed.";
        return VM_ERROR;
    }
    try {
        auto ret = UBSRMRSSetWaterMark(waterMark);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "failed to inject waterLine, " << FormatRetCode(ret);
            return VM_ERROR;
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "UBSRMRSSetWaterMark exception: " << e.what();
        return VM_ERROR;
    }

    UBSE_LOG_DEBUG << "inject waterLine succeeded.";
    return VM_OK;
}

VmResult ConvertToContainerIds(container_id_list_for_c &containerIdList, std::unordered_set<std::string> &containerIds)
{
    containerIds.clear();
    if (containerIdList.containerIdSize == 0) {
        return VM_ERROR;
    }

    for (size_t i = 0; i < containerIdList.containerIdSize; ++i) {
        size_t len = strnlen(containerIdList.containerId[i], sizeof(containerIdList.containerId[i]));
        if (len > 0) {
            containerIds.insert(std::string(containerIdList.containerId[i], len));
        }
    }

    return VM_OK;
}

VmResult ConvertToContainerPidInfos(std::unordered_map<std::string, std::vector<pid_t>> &containerInfos,
                                    std::vector<container_pid_info_for_c> &result)
{
    result.clear();
    if (containerInfos.empty()) {
        UBSE_LOG_ERROR << "containerInfos is empty.";
        return VM_ERROR;
    }
    try {
        result.reserve(containerInfos.size());
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Failed to reserve memory for result vector: " << e.what();
        return VM_ERROR_NOMEM;
    }

    for (const auto &entry : containerInfos) {
        container_pid_info_for_c info = {};

        info.containerId = const_cast<char *>(entry.first.c_str());

        info.pidsCount = std::min(entry.second.size(), static_cast<size_t>(NO_2048));
        for (size_t i = 0; i < info.pidsCount; ++i) {
            info.pids[i] = entry.second[i];
        }

        result.push_back(info);
    }

    return VM_OK;
}

VmResult GetContainerPidsByContainerIds(std::unordered_set<std::string> &containerIds,
                                        std::unordered_map<std::string, std::vector<pid_t>> &containerInfos)
{
    auto ret = OsHelper::GetPidsByContainerIds(containerIds, containerInfos);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "failed to get container info on filesystem.";
        return VM_ERROR;
    }
    if (containerInfos.size() != containerIds.size()) {
        // When a container is not found, return error
        UBSE_LOG_ERROR << "The parameter contains a non-existent container.";
        return VM_ERROR;
    }
    return VM_OK;
}

uint32_t VirtContainerSdk::GetContainerPidsHandler(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    // Parse the input data
    UBSE_LOG_DEBUG << "Start to get container pid info.";
    SrcMemoryBorrowParam borrowParam{};
    std::vector<pid_t> pids{};

    ContainerIdListForCInputMsg inputInfo(req.buffer, req.length);
    auto ret = inputInfo.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "ContainerIdListForCInputMsg Deserialize fail. ret = " << ret;
        return ret;
    }
    container_id_list_for_c originContainerListId = inputInfo.GetContainerPidInfo();
    std::unordered_set<std::string> containerIds;
    if (ret = ConvertToContainerIds(originContainerListId, containerIds); ret != VM_OK) {
        UBSE_LOG_ERROR << "ConvertToContainerIds failed. ret = " << ret;
        return ret;
    }

    // Queries container information
    std::unordered_map<std::string, std::vector<pid_t>> containerInfos;
    ret = GetContainerPidsByContainerIds(containerIds, containerInfos);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "GetContainerPidsByContainerIds failed. ret = " << ret;
        return ret;
    }

    std::vector<container_pid_info_for_c> responseData;
    ret = ConvertToContainerPidInfos(containerInfos, responseData);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "ConvertToContainerPidInfos failed," << FormatRetCode(ret);
        return ret;
    }
    // Serialized Response
    ContainerPidsForCInputMsg outputInfo(responseData);
    ret = outputInfo.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "ContainerPidsForCInputMsg Serialize failed," << FormatRetCode(ret);
        return ret;
    }
    UbseIpcMessage response = {.buffer = outputInfo.SerializedData(), .length = outputInfo.SerializedDataSize()};
    UBSE_LOG_DEBUG << "Container info response size = " << response.length;

    // Communication Return Data
    ret = SendResponse(ret, context.requestId, response);
    SafeDeleteArray(response.buffer);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "ContainerInfo response send failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_DEBUG << "ContainerInfo send succeeded.";
    return ret;
}

uint32_t VirtContainerSdk::GetMemInfoForPid(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    // Parse the input data
    UBSE_LOG_DEBUG << "Start to get container info";
    SrcMemoryBorrowParam borrowParam{};
    std::vector<pid_t> pids{};

    MemContainerPidMemInfoInputMsg inputInfo(req.buffer, req.length);
    auto ret = inputInfo.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemContainerPidMemInfoInputMsg Deserialize fail. " << FormatRetCode(ret);
        return ret;
    }
    borrowParam = inputInfo.GetBorrowParam();
    pids = inputInfo.GetPids();

    // Queries container information
    std::vector<PidInfo> pidInfos{};
    ret = GetContainerMemInfoWithStructure(borrowParam, pids, pidInfos);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Call GetContainerMemInfoWithStructure fail. " << FormatRetCode(ret);
        return ret;
    }

    // Serialized Response
    MemContainerPidMemInfoOutputMsg outputInfo(pidInfos);
    ret = outputInfo.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemContainerPidMemInfoOutputMsg Serialize fail. " << FormatRetCode(ret);
        return ret;
    }
    UbseIpcMessage response = {.buffer = outputInfo.SerializedData(), .length = outputInfo.SerializedDataSize()};
    UBSE_LOG_DEBUG << "Container info response size = " << response.length;

    // Communication Return Data
    ret = SendResponse(VM_OK, context.requestId, response);
    SafeDeleteArray(response.buffer);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "ContainerInfo response send failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_DEBUG << "ContainerInfo send succeeded.";
    return ret;
}

uint32_t VirtContainerSdk::InjectWaterLineHandler(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    // Parse the input data
    UBSE_LOG_DEBUG << "Start to inject waterline.";
    WaterMark waterMark;
    UpdateWaterLineForCInputMsg inputInfo(req.buffer, req.length);
    auto ret = inputInfo.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "UpdateWaterLineForCInputMsg Deserialize fail. ret = " << ret;
        return ret;
    }
    waterMark = inputInfo.GetWaterMark();

    // inject waterLine
    ret = InjectWaterLine(waterMark);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "InjectWaterLine failed. ret = " << ret;
        return ret;
    }

    // Communication Return Data
    UbseIpcMessage response = {nullptr, 0};
    ret = SendResponse(ret, context.requestId, response);
    SafeDeleteArray(response.buffer);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "ContainerInfo response send failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_DEBUG << "ContainerInfo send succeeded.";
    return ret;
}

uint32_t VirtContainerSdk::WaterLineMemBorrow(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    UBSE_LOG_DEBUG << "Start to do water line mem borrow.";
    // Parse input params
    MemContainerWaterLineMemBorrowInputMsg inputMsg(req.buffer, req.length);
    auto ret = inputMsg.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Call WaterLineMemBorrowInputMsg Deserialize failed. " << FormatRetCode(ret);
        return ret;
    }
    NodeLocInfo nodeLocInfo;
    std::vector<uint64_t> borrowSizes;
    WaterMark waterMark;
    ret = inputMsg.GetParams(nodeLocInfo, borrowSizes, waterMark);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Get input params failed. " << FormatRetCode(ret);
        return ret;
    }
    auto uid = context.clientInfo.uid;
    UserInfo userInfo{.uid = uid, .username = VmSystemUtil::GetUsernameByUid(uid)};
    UBSE_LOG_INFO << "nodeLocInfo = " << nodeLocInfo.toString()
                  << ", borrowSizes = " << VectorUtil::VectorToString(borrowSizes)
                  << ", highWaterMark = " << waterMark.highWaterMark << ", lowWaterMark = " << waterMark.lowWaterMark;
    // Get water line mem borrow result
    ContainerService containerService{};
    MemBorrowExecuteResult borrowResult{};
    ret = containerService.MemBorrow(nodeLocInfo, borrowSizes, waterMark, userInfo, borrowResult);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Mem borrow failed. " << FormatRetCode(ret);
        return ret;
    }
    // Build response
    MemContainerWaterLineMemBorrowOutputMsg outputMsg{borrowResult.borrowIds};
    ret = outputMsg.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemContainerWaterLineMemBorrowOutputMsg Serialize failed. " << FormatRetCode(ret);
        return ret;
    }
    UbseIpcMessage response{};
    response = {.buffer = outputMsg.SerializedData(), .length = outputMsg.SerializedDataSize()};
    // Send response
    ret = SendResponse(VM_OK, context.requestId, response);
    SafeDeleteArray(response.buffer);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "WaterLineMemBorrow response send failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_DEBUG << "WaterLineMemBorrow send succeeded.";

    return VM_OK;
}

uint32_t VirtContainerSdk::WaterLineMemMigrate(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    UBSE_LOG_DEBUG << "Start to do water line mem migrate.";
    // Parse input params
    MemContainerWaterLineMemMigrateInputMsg inputMsg(req.buffer, req.length);
    auto ret = inputMsg.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Call WaterLineMemMigrateInputMsg Deserialize failed. " << FormatRetCode(ret);
        return ret;
    }
    NodeLocInfo nodeLocInfo;
    std::unordered_set<std::string> borrowIdSet;
    std::vector<VMPresetParam> vmPresetParamList;
    ret = inputMsg.GetParams(nodeLocInfo, borrowIdSet, vmPresetParamList);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Get input params failed. " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "nodeLocInfo = " << nodeLocInfo.toString();
    for (auto &borrowId : borrowIdSet) {
        UBSE_LOG_INFO << "borrowIdSet = " << borrowId;
    }
    for (auto &param : vmPresetParamList) {
        UBSE_LOG_INFO << "ratio = " << param.ratio << ", pid = " << param.pid;
    }
    // Get water line mem migrate result
    ContainerService containerService{};
    ret = containerService.MemMigrate(nodeLocInfo, borrowIdSet, vmPresetParamList);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Mem migrate failed. " << FormatRetCode(ret);
        return VM_ERROR;
    }
    // Send response
    UbseIpcMessage response{};
    ret = SendResponse(VM_OK, context.requestId, response);
    SafeDeleteArray(response.buffer);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "WaterLineMemMigrate response send failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_DEBUG << "WaterLineMemMigrate send succeeded.";
    return VM_OK;
}

uint32_t VirtContainerSdk::WaterLineMemReturn(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    UBSE_LOG_DEBUG << "Start to do water line mem return.";
    // Parse input params
    MemContainerWaterLineMemReturnInputMsg inputMsg(req.buffer, req.length);
    auto ret = inputMsg.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Call WaterLineMemReturnInputMsg Deserialize failed. " << FormatRetCode(ret);
        return ret;
    }
    NodeLocInfo nodeLocInfo;
    std::vector<std::string> borrowIds;
    std::vector<pid_t> pids;
    ret = inputMsg.GetParams(nodeLocInfo, borrowIds, pids);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Get input params failed. " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "nodeLocInfo = " << nodeLocInfo.toString();
    UBSE_LOG_INFO << "borrowIdSet = " << VectorUtil::VectorToString(borrowIds);
    UBSE_LOG_INFO << "pids = " << VectorUtil::VectorToString(pids);

    // Get water line mem return result
    ContainerService containerService{};
    ret = containerService.MemReturn(nodeLocInfo, borrowIds, pids);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Mem return failed. " << FormatRetCode(ret);
        return VM_ERROR;
    }
    // Send response
    UbseIpcMessage response{};
    ret = SendResponse(VM_OK, context.requestId, response);
    SafeDeleteArray(response.buffer);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "WaterLineMemReturn response send failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_DEBUG << "WaterLineMemReturn send succeeded.";
    return VM_OK;
}
} // namespace vm