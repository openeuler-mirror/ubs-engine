/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "process_mem_pid_bridge.h"

#include <algorithm>
#include <map>
#include <unordered_map>

#include <dlfcn.h>
#include <linux/capability.h>

#include <securec.h>

#include "ubse_api_server.h"
#include "ubse_com.h"
#include "ubse_com_op_code.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "ubse_node_controller.h"
#include "ubse_security_module.h"
#include "ubse_serial_util.h"
#include "process_mem_pid_collect.h"
#include "process_mem_pid_config_manager.h"
#include "process_mem_pid_decision.h"
#include "process_mem_pid_info_manager.h"
namespace process_mem::pid::bridge {
UBSE_DEFINE_THIS_MODULE("process_mem");
// 具名权限对象, 便于在 [auth.role.default] 中按角色授权; 未授权时非内置用户 fail-closed
const std::string PROCESS_MEM_PERMISSION = "process_mem";

uint32_t ProcessMemPidBridge::MemoryReturn(const std::string& name)
{
    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.nodeId = ubse::nodeController::UbseNodeController::GetInstance().GetCurrentNodeId();
    if (borrower.nodeId.empty()) {
        UBSE_LOG_ERROR << "GetCurrentNodeId failed";
        return UBSE_ERROR;
    }
    auto errCode = UbseMemNumaDelete(name, borrower);
    if (errCode != UBSE_OK) {
        return errCode;
    }
    UBSE_LOG_INFO << name << "UbseMemNumaDelete Success";
    return UBSE_OK;
}

namespace {
ubse::com::UbseComEndpoint GetProcessMemReturnEndpoint(uint16_t opCode, const std::string& nodeId)
{
    return ubse::com::UbseComEndpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::UBSE_MEM_FAULT),
        .serviceId = static_cast<uint32_t>(opCode),
        .address = nodeId,
    };
}
} // namespace

uint32_t ProcessMemPidBridge::SendReturnRequestToNode(const std::string& nodeId,
                                                      const std::vector<def::ReturnRequestItem>& items)
{
    // 消息头携带原始目标节点(借入节点): 请求统一发往主节点, 主节点按该字段转发或直接执行
    ubse::serial::UbseSerialization output;
    output << nodeId;
    output << ubse::serial::array_len_insert(items.size());
    for (const auto& item : items) {
        output << item.name << item.size;
    }
    if (!output.Check()) {
        UBSE_LOG_ERROR << "SendReturnRequestToNode: serialization failed, nodeId=" << nodeId;
        return UBSE_ERROR;
    }

    std::string masterNode;
    if (ubse::election::UbseGetMasterNodeId(masterNode) != UBSE_OK || masterNode.empty()) {
        UBSE_LOG_ERROR << "SendReturnRequestToNode: get master node failed, target=" << nodeId << ", retry next round";
        return UBSE_ERROR;
    }
    auto endpoint = GetProcessMemReturnEndpoint(
        static_cast<uint16_t>(ubse::com::UbseMemFaultOpCode::UBSE_PROCESS_MEM_RETURN_REQUEST), masterNode);
    UbseByteBuffer request{.data = output.GetBuffer(), .len = output.GetLength(), .freeFunc = nullptr};
    auto ret = ubse::com::UbseRpcSend(endpoint, request, nullptr, [](void*, const UbseByteBuffer&, uint32_t) {});
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SendReturnRequestToNode: rpc send failed to master=" << masterNode << ", target=" << nodeId
                       << ", ret=" << ret;
    } else {
        UBSE_LOG_INFO << "SendReturnRequestToNode: rpc send ok to master=" << masterNode << ", target=" << nodeId
                      << ", items=" << items.size();
    }
    return ret;
}

void ProcessMemPidBridge::ProcessMemReturnRequestHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    ubse::serial::UbseDeSerialization deserializer{req.data, req.len};
    std::string targetNodeId;
    deserializer >> targetNodeId;
    ubse::serial::common_len itemCount = 0;
    deserializer >> ubse::serial::array_len_capture(itemCount);
    if (!deserializer.Check()) {
        UBSE_LOG_ERROR << "ProcessMemReturnRequestHandler: deserialize failed";
        return;
    }
    // 借出节点非主节点时请求先到主节点, 主节点不是目标借入节点则按消息携带的目标转发
    auto currentNode = ubse::nodeController::UbseNodeController::GetInstance().GetCurrentNodeId();
    if (targetNodeId != currentNode) {
        auto endpoint = GetProcessMemReturnEndpoint(
            static_cast<uint16_t>(ubse::com::UbseMemFaultOpCode::UBSE_PROCESS_MEM_RETURN_REQUEST), targetNodeId);
        UbseByteBuffer forwardReq{.data = req.data, .len = req.len, .freeFunc = nullptr};
        auto fwdRet =
            ubse::com::UbseRpcSend(endpoint, forwardReq, nullptr, [](void*, const UbseByteBuffer&, uint32_t) {});
        if (fwdRet != UBSE_OK) {
            UBSE_LOG_ERROR << "ProcessMemReturnRequestHandler: forward failed to target=" << targetNodeId
                           << ", ret=" << fwdRet;
        } else {
            UBSE_LOG_INFO << "ProcessMemReturnRequestHandler: forwarded to target=" << targetNodeId
                          << ", items=" << itemCount;
        }
        return;
    }
    if (itemCount == 0) {
        UBSE_LOG_WARN << "ProcessMemReturnRequestHandler: empty item list, ignore";
        return;
    }
    constexpr ubse::serial::common_len MAX_RETURN_ITEMS = 4096;
    if (itemCount > MAX_RETURN_ITEMS) {
        UBSE_LOG_ERROR << "ProcessMemReturnRequestHandler: invalid itemCount=" << itemCount
                       << ", max=" << MAX_RETURN_ITEMS;
        return;
    }
    std::vector<def::ReturnRequestItem> items;
    items.reserve(static_cast<size_t>(itemCount));
    for (ubse::serial::common_len i = 0; i < itemCount; ++i) {
        def::ReturnRequestItem item;
        deserializer >> item.name >> item.size;
        if (!deserializer.Check()) {
            UBSE_LOG_ERROR << "ProcessMemReturnRequestHandler: deserialize failed at item " << i;
            return;
        }
        items.push_back(std::move(item));
    }
    auto ret = decision::ProcessMemPidDecision::GetInstance().HandleReturnRequest(items);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "ProcessMemReturnRequestHandler: HandleReturnRequest failed, ret=" << ret;
    }
}

uint32_t SendPidSetResponse(int successCode, const std::string& errorMsg, uint64_t requestId)
{
    api::server::UbseIpcMessage response{};
    ubse::serial::UbseSerialization serial;
    serial << successCode << errorMsg;
    response.buffer = serial.GetBuffer();
    if (!response.buffer) {
        UBSE_LOG_ERROR << "Serialization response failed.";
        return UBSE_ERROR_NULLPTR;
    }
    response.length = static_cast<uint32_t>(serial.GetLength());
    auto ret = SendResponse(UBSE_OK, requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Send response failed, " << ubse::log::FormatRetCode(ret);
    }
    return ret;
}

namespace {
bool DeserializeProcMemConfigRequest(const api::server::UbseIpcMessage& request,
                                     def::ProcessMemNewConfigInfo& outConfig, const char* callerName)
{
    ubse::serial::UbseDeSerialization deserializer{request.buffer, request.length};
    auto ret = outConfig.Deserialize(deserializer);
    if (ret != UBSE_OK || !deserializer.Check()) {
        UBSE_LOG_ERROR << callerName << ": deserialize failed";
        return false;
    }
    return true;
}
} // namespace

uint32_t SetProcMemConfig(const api::server::UbseIpcMessage& request, const api::server::UbseRequestContext& context)
{
    def::ProcessMemNewConfigInfo newConfig{};
    if (!DeserializeProcMemConfigRequest(request, newConfig, "SetProcMemConfig")) {
        return SendPidSetResponse(0, "Invalid request", context.requestId);
    }

    if (newConfig.identifier.empty()) {
        UBSE_LOG_ERROR << "SetProcMemConfig: identifier is empty";
        return SendPidSetResponse(0, "Identifier is empty", context.requestId);
    }

    auto ret = manager::ProcessMemPidInfoManager::GetInstance().SetProcMemConfig(newConfig);
    if (ret == UBSE_ERR_NOT_EXIST) {
        if (newConfig.isPid) {
            return SendPidSetResponse(0, "PID does not exist on current node", context.requestId);
        }
        return SendPidSetResponse(0, "No running process matches name: " + newConfig.identifier, context.requestId);
    }
    if (ret == UBSE_ERR_ACCESS_DENIED) {
        return SendPidSetResponse(
            0, "PID " + newConfig.identifier + " is a root process, rejected by filter_root_process=true",
            context.requestId);
    }
    if (ret == UBSE_ERR_INVALID_ARG) {
        return SendPidSetResponse(0, "Invalid process-mem config (size/ratio/name out of range)", context.requestId);
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SetProcMemConfig failed for " << newConfig.identifier
                       << ", ret=" << ubse::log::FormatRetCode(ret);
        return SendPidSetResponse(0, "Failed to set process-mem config", context.requestId);
    }

    UBSE_LOG_INFO << "SetProcMemConfig: " << (newConfig.isPid ? "pid=" : "name=") << newConfig.identifier
                  << ", maxMemory=" << newConfig.maxMemory << ", remoteRatio=" << newConfig.remoteRatio;
    return SendPidSetResponse(1, "", context.requestId);
}

uint32_t RemoveProcMemConfig(const api::server::UbseIpcMessage& request, const api::server::UbseRequestContext& context)
{
    def::ProcessMemNewConfigInfo removeTarget{};
    if (!DeserializeProcMemConfigRequest(request, removeTarget, "RemoveProcMemConfig")) {
        return SendPidSetResponse(0, "Invalid request", context.requestId);
    }

    auto ret = manager::ProcessMemPidInfoManager::GetInstance().RemoveProcMemConfig(removeTarget.isPid,
                                                                                    removeTarget.identifier);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RemoveProcMemConfig failed for " << (removeTarget.isPid ? "pid=" : "name=")
                       << removeTarget.identifier << ", ret=" << ubse::log::FormatRetCode(ret);
        if (ret == UBSE_ERR_NOT_EXIST) {
            if (removeTarget.isPid) {
                return SendPidSetResponse(0, "pid " + removeTarget.identifier + " is not managed by process_mem",
                                          context.requestId);
            }
            return SendPidSetResponse(0, "name '" + removeTarget.identifier + "' is not configured", context.requestId);
        }
        return SendPidSetResponse(0, "Failed to remove process-mem config", context.requestId);
    }

    UBSE_LOG_INFO << "RemoveProcMemConfig: " << (removeTarget.isPid ? "pid=" : "name=") << removeTarget.identifier
                  << " removed successfully";
    return SendPidSetResponse(1, "", context.requestId);
}

namespace {

void SortConfigEntries(std::vector<def::ProcessMemDisplayEntry>& entries)
{
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        if ((a.pid > 0) != (b.pid > 0)) {
            return a.pid > 0;
        }
        if (a.pid > 0) {
            return a.pid < b.pid;
        }
        return a.name < b.name;
    });
}

void SortPidEntries(std::vector<def::ProcessMemDisplayEntry>& entries)
{
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.pid < b.pid; });
}

uint32_t SendDisplayEntries(const api::server::UbseIpcMessage& request, const api::server::UbseRequestContext& context,
                            const std::vector<def::ProcessMemDisplayEntry>& entries)
{
    api::server::UbseIpcMessage response{nullptr, 0};
    ubse::serial::UbseSerialization serializer;
    size_t entryCount = entries.size();
    serializer << entryCount;
    for (const auto& entry : entries) {
        auto serRet = entry.Serialize(serializer);
        if (serRet != UBSE_OK) {
            UBSE_LOG_ERROR << "DisplayProcMem: entry serialize failed for pid=" << entry.pid;
            return serRet;
        }
    }

    response.buffer = serializer.GetBuffer();
    if (!response.buffer) {
        UBSE_LOG_ERROR << "DisplayProcMem: serialization response failed.";
        return UBSE_ERROR_NULLPTR;
    }
    response.length = static_cast<uint32_t>(serializer.GetLength());
    auto sendRet = SendResponse(UBSE_OK, context.requestId, response);
    if (sendRet != UBSE_OK) {
        UBSE_LOG_ERROR << "DisplayProcMem: Send response failed, " << ubse::log::FormatRetCode(sendRet);
    }
    return sendRet;
}

} // namespace

void BuildConfigEntries(std::vector<def::ProcessMemDisplayEntry>& entries,
                        const std::vector<def::ProcessMemNewConfigInfo>& newConfigs)
{
    for (const auto& cfg : newConfigs) {
        if (cfg.isPid) {
            auto pidOpt = def::ParsePidFromIdentifier(cfg.identifier);
            if (!pidOpt.has_value()) {
                UBSE_LOG_WARN << "BuildConfigEntries: skipping PID config with unparseable identifier="
                              << cfg.identifier;
                continue;
            }
            def::ProcessMemDisplayEntry entry;
            entry.pid = static_cast<int32_t>(pidOpt.value());
            entry.name = "N/A";
            entry.maxMemory = cfg.maxMemory;
            entry.remoteRatio = cfg.remoteRatio;
            entries.push_back(entry);
        } else {
            def::ProcessMemDisplayEntry entry;
            entry.pid = 0;
            entry.name = cfg.identifier;
            entry.maxMemory = cfg.maxMemory;
            entry.remoteRatio = cfg.remoteRatio;
            entries.push_back(entry);
        }
    }
    SortConfigEntries(entries);
}

void BuildProcDetailEntries(std::vector<def::ProcessMemDisplayEntry>& entries,
                            const std::map<pid_t, def::ManagedPidEntry>& managedSnapshot)
{
    // 仅回显 process_mem 真正托管(匹配)到的进程, 取托管缓存而非配置/实时 /proc 扫描;
    // pid 配置命中时 name 强制 N/A (与旧行为一致), 否则显示实际匹配的配置名
    for (const auto& [pid, entry] : managedSnapshot) {
        def::ProcessMemDisplayEntry display;
        display.pid = static_cast<int32_t>(pid);
        bool hasPidConfig = (entry.sources & static_cast<uint8_t>(def::ConfigSource::PID_CONFIG)) != 0;
        display.name = hasPidConfig ? "N/A" : (entry.nameConfigName.empty() ? "N/A" : entry.nameConfigName);
        display.maxMemory = entry.maxMemory;
        display.remoteRatio = entry.remoteRatio;
        entries.push_back(display);
    }
    SortPidEntries(entries);
}

uint32_t DisplayProcMemConfig(const api::server::UbseIpcMessage& request,
                              const api::server::UbseRequestContext& context)
{
    std::vector<def::ProcessMemNewConfigInfo> newConfigs;
    manager::ProcessMemPidInfoManager::GetInstance().GetAllProcMemConfigs(newConfigs);

    std::vector<def::ProcessMemDisplayEntry> entries;
    BuildConfigEntries(entries, newConfigs);

    UBSE_LOG_INFO << "DisplayProcMemConfig: returning " << entries.size() << " config entries";
    return SendDisplayEntries(request, context, entries);
}

uint32_t DisplayProcMemDetail(const api::server::UbseIpcMessage& request,
                              const api::server::UbseRequestContext& context)
{
    std::vector<def::ProcessMemDisplayEntry> entries;
    auto managedSnapshot = manager::ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    BuildProcDetailEntries(entries, managedSnapshot);

    UBSE_LOG_INFO << "DisplayProcMemDetail: returning " << entries.size() << " managed entries";
    return SendDisplayEntries(request, context, entries);
}

namespace {
bool LoadMemPoolingLibrary()
{
    void* handle = dlopen(MEMPOOLING_PATH, RTLD_LAZY);
    if (!handle) {
        return false;
    }
    ProcessMemPidBridge::memPoolingHandle = handle;
    using MigrateOutFunc = int (*)(const std::vector<mempooling::smap::MigrateOutPayload>&, int);
    ProcessMemPidBridge::rmrsMigrateOut = reinterpret_cast<MigrateOutFunc>(dlsym(handle, "UBSRMRSMigrateOut"));
    ProcessMemPidBridge::rmrsRemove =
        reinterpret_cast<int (*)(const uint16_t, const std::vector<pid_t>&, int)>(dlsym(handle, "UBSRMRSRemove"));
    ProcessMemPidBridge::rmrsFreeWithMigrate =
        reinterpret_cast<uint32_t (*)(const std::string)>(dlsym(handle, "UBSRMRSMemFreeWithMigrate"));
    ProcessMemPidBridge::rmrsRemoteToRemote =
        reinterpret_cast<int (*)(const mempooling::smap::MigrateEscapeMsg&)>(dlsym(handle, "UBSRMRSRemoteNumaMigrate"));
    ProcessMemPidBridge::rmrsProcessConfigQuery =
        reinterpret_cast<int (*)(int, mempooling::smap::ProcessPayload*, int, int*)>(
            dlsym(handle, "UBSRMRSProcessConfigQuery"));
    if (!ProcessMemPidBridge::rmrsMigrateOut || !ProcessMemPidBridge::rmrsRemove) {
        dlclose(handle);
        ProcessMemPidBridge::memPoolingHandle = nullptr;
        return false;
    }
    if (!ProcessMemPidBridge::rmrsRemoteToRemote) {
        UBSE_LOG_WARN << "UBSRMRSRemoteNumaMigrate not exported by libmempooling.so, "
                         "remote-to-remote return will be skipped";
    }
    return true;
}

} // namespace

uint32_t ProcessMemPidBridge::RegisterConfigIpcHandlers()
{
    auto ret =
        api::server::RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_PROC_MEM_SET, SetProcMemConfig, PROCESS_MEM_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register SetProcMemConfig IPC Handler failed, " << ubse::log::FormatRetCode(ret);
        return ret;
    }
    ret = api::server::RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_PROC_MEM_REMOVE, RemoveProcMemConfig,
                                          PROCESS_MEM_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register RemoveProcMemConfig IPC Handler failed, " << ubse::log::FormatRetCode(ret);
        return ret;
    }
    ret = api::server::RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_PROC_MEM_DISPLAY_CONFIG, DisplayProcMemConfig,
                                          PROCESS_MEM_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register DisplayProcMemConfig IPC Handler failed, " << ubse::log::FormatRetCode(ret);
        return ret;
    }
    ret = api::server::RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_PROC_MEM_DISPLAY_DETAIL, DisplayProcMemDetail,
                                          PROCESS_MEM_PERMISSION);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register DisplayProcMemDetail IPC Handler failed, " << ubse::log::FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

uint32_t ProcessMemPidBridge::Init()
{
    auto ret = RegisterConfigIpcHandlers();
    if (ret != UBSE_OK) {
        return ret;
    }

    auto returnEndpoint = GetProcessMemReturnEndpoint(
        static_cast<uint16_t>(ubse::com::UbseMemFaultOpCode::UBSE_PROCESS_MEM_RETURN_REQUEST), "");
    auto rpcRet = ubse::com::UbseRegRpcService(returnEndpoint, ProcessMemPidBridge::ProcessMemReturnRequestHandler);
    if (rpcRet != UBSE_OK) {
        UBSE_LOG_ERROR << "Register ProcessMemReturnRequest RPC handler failed, " << ubse::log::FormatRetCode(rpcRet);
        return rpcRet;
    }

    if (!LoadMemPoolingLibrary()) {
        UBSE_LOG_WARN << "Failed to load libmempooling.so, memory borrowing/migration will be unavailable";
        process_mem::manager::ProcessMemPidInfoManager::GetInstance().RefreshProcMemConfigCache();
        return UBSE_OK;
    }

    std::vector<__u32> dacReadSearchCap = {CAP_DAC_READ_SEARCH};
    ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities(dacReadSearchCap, true);

    process_mem::manager::ProcessMemPidInfoManager::GetInstance().Init();

    auto decisionRet = process_mem::decision::ProcessMemPidDecision::GetInstance().Init();
    if (decisionRet != UBSE_OK) {
        UBSE_LOG_ERROR << "ProcessMemPidDecision Init failed, " << ubse::log::FormatRetCode(decisionRet);
    }

    return UBSE_OK;
}

uint32_t ProcessMemPidBridge::UnInit()
{
    if (memPoolingHandle != nullptr) {
        dlclose(memPoolingHandle);
    }
    process_mem::decision::ProcessMemPidDecision::GetInstance().UnInit();
    process_mem::manager::ProcessMemPidInfoManager::GetInstance().UnInit();
    return UBSE_OK;
}

} // namespace process_mem::pid::bridge
