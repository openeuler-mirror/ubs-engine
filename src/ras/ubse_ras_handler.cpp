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

#include "ubse_ras_handler.h"
#include <dlfcn.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <mutex>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <utility>
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "adapter_plugins/mti/ubse_smbios.h"

#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_election_module.h"
#include "ubse_error.h"
#include "ubse_event.h"
#include "ubse_logger.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_module.h"
#include "ubse_node_mgr.h"
#include "ubse_pointer_process.h"
#include "ubse_ras_oom_handler.h"
#include "ubse_ras_panic_reboot_handler.h"
#include "ubse_str_util.h"
#include "message/ubse_ras_message.h"
#include "message/ubse_ras_panic_reboot_message.h"
#include "plugin_services/mem/ubse_mem_service.h"
#include "securec.h"

namespace ubse::ras {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::election;
using namespace ubse::log;
using namespace ubse::nodeController;
using namespace ubse::event;
using namespace ubse::adapter_plugins::mti;
using namespace ubse::common::def;
using namespace ubse::com;
using namespace ubse::context;
using namespace ubse::utils;
using namespace ubse::service;
using namespace ubse::service::mem;
using namespace ubse::nodeMgr;
std::unordered_map<ALARM_FAULT_TYPE, std::set<std::string>> g_MSG_ID_MAP{};
std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> g_HANDLER_RESULT{};

struct HandlerResult {
    ALARM_FAULT_TYPE alarmFaultType; // 故障类型
    uint64_t timestamp;              // 记录时的时间戳
    uint32_t retCode;                // 故障处理函数执行结果
};

// handler 执行结果缓存及其互斥锁，由下列存取函数统一保护：
//   IsHandlerDone / SetHandlerResult / GetResultFromHandlersByMsg / ClearHandlerResult / ClearAllHandlerResults
static std::unordered_map<std::string, std::unordered_map<std::string, HandlerResult>>
    g_handlerResultMap{}; // <msg, <handlerName, result>>
static std::mutex g_handlerResultMutex;
static bool IsHandlerDone(const std::string& msg, const std::string& handlerName)
{
    std::lock_guard<std::mutex> lock(g_handlerResultMutex);
    auto msgIt = g_handlerResultMap.find(msg);
    if (msgIt == g_handlerResultMap.end()) {
        return false;
    }
    auto handlerIt = msgIt->second.find(handlerName);
    return handlerIt != msgIt->second.end() && handlerIt->second.retCode == UBSE_OK;
}

static uint64_t GetTimestamp()
{
    auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}

static void SetHandlerResult(ALARM_FAULT_TYPE faultType, const std::string& msg, const std::string& handlerName,
                             uint32_t retCode)
{
    std::lock_guard<std::mutex> lock(g_handlerResultMutex);
    g_handlerResultMap[msg][handlerName] = {faultType, GetTimestamp(), retCode};
}

static UbseResult GetResultFromHandlersByMsg(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(g_handlerResultMutex);
    auto msgIt = g_handlerResultMap.find(msg);
    if (msgIt == g_handlerResultMap.end()) {
        return UBSE_OK;
    }
    for (const auto& result : msgIt->second) {
        if (result.second.retCode != UBSE_OK) {
            return static_cast<UbseResult>(result.second.retCode);
        }
    }
    return UBSE_OK;
}

static void ClearAllHandlerResults()
{
    std::lock_guard<std::mutex> lock(g_handlerResultMutex);
    g_handlerResultMap.clear();
}

constexpr uint64_t UBSE_RAS_FAULT_HANDLE_RESULT_EXPIRE_TIME_MS =
    1000 * 60 * 10; // 故障结果过期时间，单位：MS，默认10分钟
static void ClearExpiredHandlerResult()
{
    UBSE_LOG_INFO << "Start clear expired handler result";
    std::lock_guard<std::mutex> lock(g_handlerResultMutex);
    auto now = GetTimestamp();
    for (auto& msgResult : g_handlerResultMap) {
        for (auto it = msgResult.second.begin(); it != msgResult.second.end();) {
            if (now - it->second.timestamp > UBSE_RAS_FAULT_HANDLE_RESULT_EXPIRE_TIME_MS &&
                it->second.alarmFaultType == ALARM_OOM_EVENT) { // 影响最小化，只清除OOM故障结果
                UBSE_LOG_INFO << "Clear expired OOM handler result for msg: " << msgResult.first
                              << ", handlerName: " << it->first;
                it = msgResult.second.erase(it);
            } else {
                ++it;
            }
        }
    }
    UBSE_LOG_INFO << "Finish clear expired handler result";
}

static void SubmitClearExpiredHandlerResult()
{
    auto taskModule = ubse::context::UbseContext::GetInstance().GetModule<task_executor::UbseTaskExecutorModule>();
    if (taskModule == nullptr) {
        UBSE_LOG_ERROR << "Get task module failed";
        return;
    }
    ubse::task_executor::UbseTaskExecutorPtr executor = taskModule->Get(UBSE_RAS_FAULT_HANDLE_THREAD_POOL);
    if (executor == nullptr) {
        UBSE_LOG_WARN << "Get oom fault handle thread pool executor failed";
        return;
    }
    bool submitSuccess = executor->Execute([]() -> void { ClearExpiredHandlerResult(); });
    if (!submitSuccess) {
        UBSE_LOG_WARN << "Submit clear expired handler result task failed";
        return;
    }
}
struct DebtInfo {
    std::string name; // 资源名称标识
    std::string borrowNodeId;
    std::string lentNodeId;
    uint64_t size{}; // 总借用内存大小（字节）
    std::string borrowType;
};

// 辅助函数：检查节点是否在静态列表中
bool IsNodeInStaticList(const std::string& nodeId, const std::vector<UbseNodeStaticInfo>& staticNodeInfoList)
{
    return std::any_of(staticNodeInfoList.begin(), staticNodeInfoList.end(),
                       [nodeId](const auto& node) { return node.nodeId == nodeId; });
}

// 辅助函数：获取借入节点ID
std::string GetBorrowNodeId(const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult)
{
    if (!algoResult.importNumaInfos.empty()) {
        return algoResult.importNumaInfos.front().nodeId;
    }
    return "";
}

// 辅助函数：获取借出节点ID
std::string GetLentNodeId(const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult)
{
    if (!algoResult.exportNumaInfos.empty()) {
        return algoResult.exportNumaInfos.front().nodeId;
    }
    return "";
}

// 辅助函数：处理导出对象
void ProcessExportObj(const std::string& type, const std::string& resourceId,
                      const ubse::adapter_plugins::mmi::UbseMemBorrowExportBaseObj& numaExportObj,
                      const std::string& nodeId, std::unordered_map<std::string, DebtInfo>& numaMemoryDebtInfoMap)
{
    if (numaExportObj.status.state != ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_EXPORT_SUCCESS) {
        return;
    }
    std::string borrowNodeId = GetBorrowNodeId(numaExportObj.algoResult);
    std::string lentNodeId = GetLentNodeId(numaExportObj.algoResult);
    if (nodeId != borrowNodeId && nodeId != lentNodeId) {
        return;
    }

    // 获取或创建账本信息
    auto it = numaMemoryDebtInfoMap.find(resourceId);
    if (it == numaMemoryDebtInfoMap.end()) {
        DebtInfo debtInfo;
        debtInfo.name = resourceId;
        numaMemoryDebtInfoMap[resourceId] = debtInfo;
        it = numaMemoryDebtInfoMap.find(resourceId);
    }

    DebtInfo& debtInfo = it->second;

    debtInfo.borrowNodeId = borrowNodeId;
    debtInfo.lentNodeId = lentNodeId;
    debtInfo.size = 0;
    debtInfo.borrowType = type;

    for (const auto& exportNumaInfo : numaExportObj.algoResult.exportNumaInfos) {
        debtInfo.size += exportNumaInfo.size;
    }
}

// 辅助函数：处理导入对象
void ProcessImportObj(const std::string& type, const std::string& resourceId,
                      const ubse::adapter_plugins::mmi::UbseMemBorrowImportBaseObj& numaExportObj,
                      const std::string& nodeId, std::unordered_map<std::string, DebtInfo>& numaMemoryDebtInfoMap)
{
    std::string borrowNodeId = GetBorrowNodeId(numaExportObj.algoResult);
    std::string lentNodeId = GetLentNodeId(numaExportObj.algoResult);
    if (nodeId != borrowNodeId && nodeId != lentNodeId) {
        return;
    }

    // 获取或创建账本信息
    auto nameAndNodeId = resourceId + "_" + borrowNodeId;
    auto it = numaMemoryDebtInfoMap.find(nameAndNodeId);
    if (it == numaMemoryDebtInfoMap.end()) {
        DebtInfo debtInfo;
        debtInfo.name = nameAndNodeId;
        numaMemoryDebtInfoMap[nameAndNodeId] = debtInfo;
        it = numaMemoryDebtInfoMap.find(nameAndNodeId);
    }

    DebtInfo& debtInfo = it->second;

    debtInfo.borrowNodeId = borrowNodeId;
    debtInfo.lentNodeId = lentNodeId;
    debtInfo.size = 0;
    debtInfo.borrowType = type;

    for (const auto& exportNumaInfo : numaExportObj.algoResult.exportNumaInfos) {
        debtInfo.size += exportNumaInfo.size;
    }
}

// 辅助函数：处理所有账本信息
std::unordered_map<std::string, DebtInfo> ProcessDebtInfo(
    const ubse::adapter_plugins::mmi::NodeMemDebtInfoMap& memDebtInfoMap, const std::string& nodeId,
    const std::unordered_map<std::string, UbseNodeInfo>& nodeMap)
{
    std::unordered_map<std::string, DebtInfo> numaMemoryDebtInfoMap;

    // 遍历所有节点账本信息
    for (const auto& nodeDebtInfoPair : memDebtInfoMap) {
        const std::string& tmpNodeId = nodeDebtInfoPair.first;
        const auto& nodeDebtInfo = nodeDebtInfoPair.second;

        // 处理Numa导入对象
        for (const auto& numaImportObjPair : nodeDebtInfo.numaImportObjMap) {
            const std::string& resourceId = numaImportObjPair.first;
            const auto& numaImportObj = numaImportObjPair.second;
            ProcessImportObj("Numa", resourceId, numaImportObj, nodeId, numaMemoryDebtInfoMap);
        }

        // 处理Numa导出对象
        for (const auto& numaExportObjPair : nodeDebtInfo.numaExportObjMap) {
            const std::string& resourceId = numaExportObjPair.first;
            const auto& numaExportObj = numaExportObjPair.second;
            ProcessExportObj("Numa", resourceId, numaExportObj, nodeId, numaMemoryDebtInfoMap);
        }

        // 处理Fd导入对象
        for (const auto& fdImportObjPair : nodeDebtInfo.fdImportObjMap) {
            const std::string& resourceId = fdImportObjPair.first;
            const auto& fdImportObj = fdImportObjPair.second;
            ProcessImportObj("Fd", resourceId, fdImportObj, nodeId, numaMemoryDebtInfoMap);
        }

        // 处理Fd导出对象
        for (const auto& fdExportObjPair : nodeDebtInfo.fdExportObjMap) {
            const std::string& resourceId = fdExportObjPair.first;
            const auto& fdExportObj = fdExportObjPair.second;
            ProcessExportObj("Fd", resourceId, fdExportObj, nodeId, numaMemoryDebtInfoMap);
        }
    }
    return numaMemoryDebtInfoMap;
}

void LogMemDebtInfoWithNode(ALARM_FAULT_TYPE faultType, const std::string& nodeId)
{
    // 参数校验
    if (nodeId.empty()) {
        UBSE_LOG_ERROR << "Invalid argument, nodeId is empty!";
        return;
    }

    // 获取节点信息
    auto staticNodeInfoList = ubse::nodeMgr::GetAllNodes();
    std::unordered_map<std::string, UbseNodeInfo> nodeMap = UbseNodeController::GetInstance().GetAllNodes();

    // 检查节点是否存在
    if (!IsNodeInStaticList(nodeId, staticNodeInfoList)) {
        UBSE_LOG_ERROR << "Invalid argument, nodeId not exist static node list!nodeId=" << nodeId;
        return;
    }

    // 获取账本信息
    ubse::adapter_plugins::mmi::NodeMemDebtInfoMap memDebtInfoMap;
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return;
    }
    uint32_t ret = memService->UbseGetMemDebtInfoFromMaster("", memDebtInfoMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "The UbseGetMemDebtInfo method call failed.";
        return;
    }

    // 处理账本信息
    auto debtInfos = ProcessDebtInfo(memDebtInfoMap, nodeId, nodeMap);
    for (const auto& info : debtInfos) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << ", Alarm type=" << faultType << ". name=" << info.second.name
                      << ", ImportNode=" << info.second.lentNodeId << ", ExportNode=" << info.second.borrowNodeId
                      << ", BorrowType=" << info.second.borrowType << ", RequestSize=" << info.second.size << " byte. ";
    }
}

UbseRasHandler UbseRasHandler::instance;

UbseRasHandler& UbseRasHandler::GetInstance()
{
    return instance;
}

UbseRasHandler::UbseRasHandler() noexcept = default;

UbseRasHandler::~UbseRasHandler() = default;

UbseResult UbseRasHandler::NodeFaultHandle(alarm_msg* alarmMsgPtr)
{
    if (alarmMsgPtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    std::string faultInfo(alarmMsgPtr->pucParas);
    switch (alarmMsgPtr->usAlarmId) {
        case ALARM_REBOOT_EVENT:
            return HandleBMCFault(faultInfo);
        case ALARM_OOM_EVENT:
            return HandleOomFault(alarmMsgPtr);
        case ALARM_PANIC_EVENT:
            return HandlePanicAndRebootFault(ALARM_PANIC_EVENT, faultInfo);
        case ALARM_KERNEL_REBOOT_EVENT:
            return HandlePanicAndRebootFault(ALARM_KERNEL_REBOOT_EVENT, faultInfo);
        case ALARM_MEM_FAULT:
            return HandleMemoryFault(ALARM_MEM_FAULT, faultInfo);
        default:
            UBSE_LOG_WARN << "fault type is invalid, type=" << alarmMsgPtr->usAlarmId << ", info=" << faultInfo;
            return UBSE_ERROR;
    }
}

UbseResult ReportAckToSysSentry(ALARM_FAULT_TYPE alarmFaultType, const std::string& message)
{
    auto size = message.size() + 1;
    auto ack = new (std::nothrow) char[size];
    if (ack == nullptr) {
        UBSE_LOG_ERROR << "create ack failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = strncpy_s(ack, size, message.c_str(), message.size());
    if (ret != EOK) {
        UBSE_LOG_WARN << "[RAS] strnpy_s fail. ErrorCode=" << ret;
        SafeDeleteArray(ack, size);
        return UBSE_ERROR_IO;
    }
    auto xalarmHandle = dlopen("libxalarm.so", RTLD_LAZY);
    if (xalarmHandle == nullptr) {
        UBSE_LOG_WARN << "[RAS] dlopen libxalarm.so fail";
        SafeDeleteArray(ack, size);
        return UBSE_RAS_ERROR_DLOPEN_XALARMD;
    }
    auto xalarmReportFunc = (XalarmReportEventFunc)dlsym(xalarmHandle, "xalarm_report_event");
    if (xalarmReportFunc == nullptr) {
        UBSE_LOG_WARN << "[RAS] Resolve xalarm_report_event failed, alarmFaultType=" << alarmFaultType;
        SafeDeleteArray(ack, size);
        dlclose(xalarmHandle);
        return UBSE_RAS_ERROR_DLSYM_XALARMD;
    }
    ret = xalarmReportFunc(alarmFaultType, ack, strlen(ack));
    if (ret < 0) {
        SafeDeleteArray(ack, size);
        dlclose(xalarmHandle);
        UBSE_LOG_WARN << "[RAS] Failed to send msg, ErrorCode=" << ret;
        return UBSE_RAS_ERROR_REPORT_TO_XALARMD;
    }
    SafeDeleteArray(ack, size);
    dlclose(xalarmHandle);
    return UBSE_OK;
}

// 判断当前是否采用分层选举。
// 指定根节点沿用单层主备语义；无指定根节点时分别查询组内备和全局备。
static bool IsHierarchicalElection()
{
    if (!ubse::adapter_plugins::smbios::UbseSmbios::GetInstance().IsClosType()) {
        return false;
    }
    return ubse::nodeMgr::GetRootIpList().empty();
}

// 向单个备节点发送升主报文，并同时检查远端执行结果和 RPC 传输结果。
UbseResult SendSwitchRoleMessage(const std::shared_ptr<UbseComModule>& comModule, const std::string& targetNodeId)
{
    // 不复用报文对象，避免多个备节点之间相互污染 RPC 响应状态。
    UbseRasMessagePtr request = new (std::nothrow) UbseRasMessage();
    if (request == nullptr) {
        UBSE_LOG_ERROR << "Allocate switch-role request failed, targetNodeId=" << targetNodeId;
        return UBSE_ERROR_NULLPTR;
    }
    UbseRasMessagePtr response = new (std::nothrow) UbseRasMessage();
    if (response == nullptr) {
        UBSE_LOG_ERROR << "Allocate switch-role response failed, targetNodeId=" << targetNodeId;
        return UBSE_ERROR_NULLPTR;
    }
    SendParam param{targetNodeId, static_cast<uint16_t>(UbseModuleCode::RAS),
                    static_cast<uint16_t>(UbseRasOpCode::UBSE_RAS_SWITCH_ROLE)};
    // 远端执行失败优先返回；远端成功但 RPC 异常时返回传输错误。
    const auto rpcRet = comModule->RpcSend(param, request, response);
    const auto responseRet = response->GetResult();
    if (responseRet != UBSE_OK || rpcRet != UBSE_OK) {
        UBSE_LOG_WARN << "Switch-role RPC failed, targetNodeId=" << targetNodeId << ", "
                      << "response" << FormatRetCode(responseRet) << ", rpc" << FormatRetCode(rpcRet);
        return responseRet != UBSE_OK ? responseRet : rpcRet;
    }
    UBSE_LOG_INFO << "Switch-role RPC succeeded, targetNodeId=" << targetNodeId;
    return UBSE_OK;
}

// 查询组内备节点 ID；由调用方按组网模式决定查询失败是否可忽略。
static UbseResult GetLocalStandbyId(const std::shared_ptr<UbseElectionModule>& electionModule,
                                    const std::string& currentNodeId, std::string& standbyId)
{
    Node localStandby;
    const auto ret = electionModule->GetLocalStandbyNode(localStandby);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get local standby failed, currentNodeId=" << currentNodeId << ", " << FormatRetCode(ret);
        return ret;
    }
    standbyId = localStandby.id;
    if (standbyId.empty()) {
        UBSE_LOG_WARN << "Local standby lookup returned an empty node ID, currentNodeId=" << currentNodeId;
    }
    return UBSE_OK;
}

// 仅当当前机柜主也是全局主时查询全局备；查询失败只告警，不阻断组内倒换。
static std::string GetGlobalStandbyId(const UbseRoleInfo& curRoleInfo, const std::string& localStandbyId)
{
    UbseRoleInfo globalMaster;
    auto ret = UbseGetMasterInfo(globalMaster);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get global master failed, currentNodeId=" << curRoleInfo.nodeId << ", " << FormatRetCode(ret);
        return {};
    }
    // 机柜主不是全局主时，只需要处理组内倒换。
    if (curRoleInfo.nodeId != globalMaster.nodeId) {
        UBSE_LOG_INFO << "Skip global standby lookup for non-global local master, currentNodeId=" << curRoleInfo.nodeId
                      << ", globalMasterNodeId=" << globalMaster.nodeId;
        return {};
    }
    UbseRoleInfo globalStandby;
    ret = UbseGetStandbyInfo(globalStandby);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get global standby failed, currentNodeId=" << curRoleInfo.nodeId << ", "
                      << FormatRetCode(ret);
        return {};
    }
    if (globalStandby.nodeId.empty()) {
        UBSE_LOG_WARN << "Global standby lookup returned an empty node ID, currentNodeId=" << curRoleInfo.nodeId;
        return {};
    }
    // 同一节点同时是组内备和全局备时只发送一次。
    if (globalStandby.nodeId == localStandbyId) {
        UBSE_LOG_INFO << "Deduplicate identical local and global standby, currentNodeId=" << curRoleInfo.nodeId
                      << ", targetNodeId=" << localStandbyId;
        return {};
    }
    return globalStandby.nodeId;
}

// 在降主前解析全部通知目标，避免角色切换后无法取得原选举信息。
// 单层查询失败沿用原逻辑返回错误；分层缺少一类备节点时继续解析另一类备节点。
static UbseResult GetSwitchRoleTargets(const UbseRoleInfo& curRoleInfo,
                                       const std::shared_ptr<UbseElectionModule>& electionModule,
                                       std::string& standbyId, std::string& globalStandbyId)
{
    if (!IsHierarchicalElection()) {
        // 1D 和指定根节点沿用原单层备节点查询。
        UbseRoleInfo standby;
        const auto ret = UbseGetStandbyInfo(standby);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Get single-layer standby failed, currentNodeId=" << curRoleInfo.nodeId << ", "
                           << FormatRetCode(ret);
            return ret;
        }
        standbyId = standby.nodeId;
        if (standbyId.empty()) {
            UBSE_LOG_WARN << "Single-layer standby lookup returned an empty node ID, currentNodeId="
                          << curRoleInfo.nodeId;
        }
        return UBSE_OK;
    }
    // 分层选举中组内备不存在是允许场景，继续判断是否需要通知全局备。
    const auto localRet = GetLocalStandbyId(electionModule, curRoleInfo.nodeId, standbyId);
    if (localRet != UBSE_OK) {
        UBSE_LOG_INFO << "Continue resolving global standby after local standby lookup failed, currentNodeId="
                      << curRoleInfo.nodeId << ", " << FormatRetCode(localRet);
    }
    globalStandbyId = GetGlobalStandbyId(curRoleInfo, standbyId);
    UBSE_LOG_INFO << "Resolved hierarchical switch-role targets, currentNodeId=" << curRoleInfo.nodeId
                  << ", localStandbyNodeId=" << standbyId << ", globalStandbyNodeId=" << globalStandbyId;
    return UBSE_OK;
}

// 完成一次主动降主并分别通知组内/单层备和全局备，全局失败只记录告警。
static UbseResult SwitchRoleToTargets(const std::shared_ptr<UbseElectionModule>& electionModule,
                                      const std::string& currentNodeId, const std::string& standbyId,
                                      const std::string& globalStandbyId)
{
    if (standbyId.empty() && globalStandbyId.empty()) {
        UBSE_LOG_INFO << "No standby target found; continue BMC fault handling, currentNodeId=" << currentNodeId;
        return UBSE_OK;
    }
    // 所有目标已在降主前解析完成，避免角色对象清理后丢失全局备信息。
    UBSE_LOG_INFO << "Demote local master before notifying standby nodes, currentNodeId=" << currentNodeId
                  << ", localOrSingleStandbyNodeId=" << standbyId << ", globalStandbyNodeId=" << globalStandbyId;
    electionModule->SwitchAgentFromMaster();
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Communication module is unavailable after local master demotion, currentNodeId="
                       << currentNodeId << ", localOrSingleStandbyNodeId=" << standbyId
                       << ", globalStandbyNodeId=" << globalStandbyId;
        // 仅有全局备时沿用“全局倒换失败只告警”；单层/组内目标无法通知时返回模块错误。
        return standbyId.empty() ? UBSE_RAS_ERROR_SWITCH_ROLE : UBSE_ERROR_NULLPTR;
    }
    // 组内/单层通知失败不阻止全局通知尝试，最终仍优先返回组内/单层错误。
    auto standbyRet = UBSE_OK;
    if (!standbyId.empty()) {
        UBSE_LOG_INFO << "Notify standby to take over, currentNodeId=" << currentNodeId
                      << ", targetType=local-or-single-layer-standby, targetNodeId=" << standbyId;
        standbyRet = SendSwitchRoleMessage(comModule, standbyId);
    }
    if (!globalStandbyId.empty()) {
        UBSE_LOG_INFO << "Notify standby to take over, currentNodeId=" << currentNodeId
                      << ", targetType=global-standby, targetNodeId=" << globalStandbyId;
        const auto globalRet = SendSwitchRoleMessage(comModule, globalStandbyId);
        if (globalRet != UBSE_OK) {
            UBSE_LOG_WARN << "Notify global standby failed, currentNodeId=" << currentNodeId
                          << ", targetNodeId=" << globalStandbyId << ", " << FormatRetCode(globalRet);
        }
    }
    if (standbyRet != UBSE_OK) {
        UBSE_LOG_WARN << "Switch-role notification finished with a local or single-layer target failure, "
                      << "currentNodeId=" << currentNodeId << ", targetNodeId=" << standbyId << ", "
                      << FormatRetCode(standbyRet);
        return standbyRet;
    }
    UBSE_LOG_INFO << "Switch-role notification completed, currentNodeId=" << currentNodeId;
    // 已尝试通知可接管节点，用专用返回码阻止故障节点继续以原主角色处理本机故障。
    return UBSE_RAS_ERROR_SWITCH_ROLE;
}

// 当前节点的 master 表示组内主角色；存在接管目标时主动降主并发送升主报文。
// 非主节点或没有任何备节点时返回成功，让后续正常主节点继续处理 BMC 故障。
UbseResult SendSwitchRoleToStandby(const UbseRoleInfo& curRoleInfo, const std::string& msgId)
{
    // 非组内主不持有需要让出的本地角色，直接继续故障处理。
    if (curRoleInfo.nodeRole != ELECTION_ROLE_MASTER) {
        UBSE_LOG_INFO << "Skip switch-role notification for non-master node, currentNodeId=" << curRoleInfo.nodeId;
        return UBSE_OK;
    }
    auto electionModule = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "Get election module failed, currentNodeId=" << curRoleInfo.nodeId;
        return UBSE_ERROR_NULLPTR;
    }
    // 必须先收集全部目标，再执行唯一一次主动降主。
    std::string standbyId;
    std::string globalStandbyId;
    const auto ret = GetSwitchRoleTargets(curRoleInfo, electionModule, standbyId, globalStandbyId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Resolve switch-role targets failed, currentNodeId=" << curRoleInfo.nodeId
                       << ", msgId=" << msgId << ", " << FormatRetCode(ret);
        return ret;
    }
    return SwitchRoleToTargets(electionModule, curRoleInfo.nodeId, standbyId, globalStandbyId);
}

void ClearFaultHandlerResult(const std::string& msgId)
{
    UBSE_LOG_INFO << "Clear fault handler result for msgId=" << msgId;
    std::lock_guard<std::mutex> lock(g_handlerResultMutex);
    g_handlerResultMap[msgId].clear();
}

UbseResult ReportBMCFaultToMaster(const std::string& info, const std::string& faultNodeId,
                                  const std::string& masterNodeId)
{
    if (faultNodeId == masterNodeId) {
        UBSE_LOG_WARN << "Fault node is master, cannot process BMC itself";
        return UBSE_ERROR;
    }
    UbseRasMessagePtr request = new (std::nothrow) UbseRasMessage();
    UbseRasMessagePtr response = new (std::nothrow) UbseRasMessage();
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "new ubse ras message failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    request->SetData(faultNodeId);
    request->SetMsg(info);
    SendParam param{masterNodeId, static_cast<uint16_t>(UbseModuleCode::RAS),
                    static_cast<uint16_t>(UbseRasOpCode::UBSE_RAS_BMC_REBOOT)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Get com module failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RpcSend(param, request, response);
    if (ret != UBSE_OK || response->GetResult() != UBSE_OK) {
        UBSE_LOG_WARN << "Fault execute may fail, " << FormatRetCode(response->GetResult());
        UBSE_LOG_WARN << "RpcSend may fail, " << FormatRetCode(ret);
        if (response->GetResult() != UBSE_OK) {
            return response->GetResult();
        }
        return ret;
    }
    return UBSE_OK;
}

bool IsOnlyOneNodeInCluster()
{
    auto electionModule = UbseContext::GetInstance().GetModule<ubse::election::UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Getting the election module failed.";
        return false;
    }
    Node master;
    Node standby;
    std::vector<Node> agents;
    auto ret = electionModule->UbseGetAllNodes(master, standby, agents);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get all nodes failed, " << FormatRetCode(ret);
        return false;
    }
    Node currentNode;
    ret = electionModule->GetCurrentNode(currentNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get current node failed, " << FormatRetCode(ret);
        return false;
    }
    if (!currentNode.id.empty() && currentNode.id == master.id && standby.id.empty() && agents.empty()) {
        return true;
    }
    return false;
}

// 判断指定根节点场景是否没有单层主备可以处理故障。
// 指定根节点不存在全局备概念，沿用原单层选举的主、备角色进行 ACK 决策。
static bool ShouldSkipSpecifiedRootNodeBmcFault(const UbseRoleInfo& curRoleInfo,
                                                const std::vector<std::string>& rootIps)
{
    // 当前节点为主时，只要存在备节点就必须先执行主备倒换
    UbseRoleInfo standby;
    const auto standbyRet = UbseGetStandbyInfo(standby);
    if (curRoleInfo.nodeRole == ELECTION_ROLE_MASTER) {
        if (standbyRet != UBSE_OK) {
            UBSE_LOG_WARN << "Skip BMC fault handling, electionMode=specified-root-node, nodeId=" << curRoleInfo.nodeId
                          << ", reason=master-without-standby, standby" << FormatRetCode(standbyRet);
            return true;
        }
        UBSE_LOG_INFO << "Continue BMC fault handling, electionMode=specified-root-node, nodeId=" << curRoleInfo.nodeId
                      << ", reason=standby-available, standbyNodeId=" << standby.nodeId;
        return false;
    }
    // 当前节点是根节点时，本身仍可能参与选举，不能直接 ACK
    UbseRoleInfo master;
    const auto masterRet = UbseGetMasterInfo(master);
    const auto currentNode = ubse::nodeMgr::GetCurrentNode();
    const bool isRoot = std::find(rootIps.begin(), rootIps.end(), currentNode.addr) != rootIps.end();
    if (!isRoot && masterRet != UBSE_OK && standbyRet != UBSE_OK) {
        UBSE_LOG_WARN << "Skip BMC fault handling, electionMode=specified-root-node, nodeId=" << curRoleInfo.nodeId
                      << ", nodeAddr=" << currentNode.addr << ", reason=non-root-without-master-or-standby, master"
                      << FormatRetCode(masterRet) << ", standby" << FormatRetCode(standbyRet);
        return true;
    }
    UBSE_LOG_INFO << "Continue BMC fault handling, electionMode=specified-root-node, nodeId=" << curRoleInfo.nodeId
                  << ", nodeAddr=" << currentNode.addr << ", isRoot=" << isRoot << ", masterNodeId=" << master.nodeId
                  << ", standbyNodeId=" << standby.nodeId << ", master" << FormatRetCode(masterRet) << ", standby"
                  << FormatRetCode(standbyRet);
    return false;
}

// 判断故障机柜是否存在可接管的组内备；仅机柜主需要执行该判断。
static bool HasLocalStandbyForBmcFault(const UbseRoleInfo& curRoleInfo,
                                       const std::shared_ptr<UbseElectionModule>& electionModule)
{
    if (curRoleInfo.nodeRole != ELECTION_ROLE_MASTER) {
        return false;
    }
    std::string localStandbyId;
    const auto ret = GetLocalStandbyId(electionModule, curRoleInfo.nodeId, localStandbyId);
    if (ret != UBSE_OK || localStandbyId.empty()) {
        return false;
    }
    UBSE_LOG_INFO << "Continue BMC fault handling, electionMode=hierarchical, nodeId=" << curRoleInfo.nodeId
                  << ", reason=local-standby-available, standbyNodeId=" << localStandbyId;
    return true;
}

// 仅统计管理组内除故障节点外的主、备和 agent；挂载组节点不能参与全局选举。
static bool HasGlobalElectionCandidate(const std::shared_ptr<UbseElectionModule>& electionModule,
                                       const std::string& faultNodeId)
{
    HaTopologyInfo topology;
    const auto ret = electionModule->GetCurNodeGlobalTopoInfo(topology);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get global topology failed while evaluating BMC fault, electionMode=hierarchical, nodeId="
                      << faultNodeId << ", reason=global-election-candidate-unknown, " << FormatRetCode(ret);
        return false;
    }
    const auto hasCandidate = [&faultNodeId](const GroupTopology& group) {
        if (!group.isManagingGroup) {
            return false;
        }
        const auto valid = [&faultNodeId](const std::string& nodeId) {
            return !nodeId.empty() && nodeId != faultNodeId;
        };
        return valid(group.groupMasterId) || valid(group.groupStandbyId) ||
               std::any_of(group.groupNodes.begin(), group.groupNodes.end(), valid);
    };
    const bool found = hasCandidate(topology.currentGroup) ||
                       std::any_of(topology.groups.begin(), topology.groups.end(), hasCandidate);
    if (found) {
        UBSE_LOG_INFO << "Continue BMC fault handling, electionMode=hierarchical, nodeId=" << faultNodeId
                      << ", reason=visible-managing-group-candidate";
    }
    return found;
}

// 判断分层选举是否可直接 ACK：仍有组内备或全局候选时继续处理，否则允许故障节点下电。
static bool ShouldSkipHierarchicalElectionBmcFault(const UbseRoleInfo& curRoleInfo,
                                                   const std::shared_ptr<UbseElectionModule>& electionModule)
{
    UbseRoleInfo globalMaster, globalStandby;
    auto masterRet = UbseGetMasterInfo(globalMaster);
    auto standbyRet = UbseGetStandbyInfo(globalStandby);
    if (masterRet != UBSE_OK && standbyRet != UBSE_OK) {
        if (HasGlobalElectionCandidate(electionModule, curRoleInfo.nodeId)) {
            return false;
        }
        // ACK 前再查一次全局主备，避免切主窗口内误判为没有接管节点。
        masterRet = UbseGetMasterInfo(globalMaster);
        standbyRet = UbseGetStandbyInfo(globalStandby);
        if (masterRet != UBSE_OK && standbyRet != UBSE_OK) {
            if (HasLocalStandbyForBmcFault(curRoleInfo, electionModule)) {
                return false;
            }
            UBSE_LOG_WARN << "Skip BMC fault handling, electionMode=hierarchical, reason=no-confirmed-global-"
                          << "election-candidate, nodeId=" << curRoleInfo.nodeId << ", master"
                          << FormatRetCode(masterRet) << ", standby" << FormatRetCode(standbyRet);
            return true;
        }
    }
    // 全局主尚未产生但已有全局备时，等待全局主产生后继续处理
    if (masterRet != UBSE_OK) {
        UBSE_LOG_INFO << "Continue BMC fault handling, electionMode=hierarchical, nodeId=" << curRoleInfo.nodeId
                      << ", reason=global-standby-available, standbyNodeId=" << globalStandby.nodeId << ", master"
                      << FormatRetCode(masterRet);
        return false;
    }
    // 已有全局主时：健康主直接处理；故障节点为全局主时继续判断全局备。
    if (curRoleInfo.nodeId != globalMaster.nodeId) {
        UBSE_LOG_INFO << "Continue BMC fault handling, electionMode=hierarchical, nodeId=" << curRoleInfo.nodeId
                      << ", reason=healthy-global-master-available, masterNodeId=" << globalMaster.nodeId;
        return false;
    }
    if (standbyRet == UBSE_OK) {
        UBSE_LOG_INFO << "Continue BMC fault handling, electionMode=hierarchical, nodeId=" << curRoleInfo.nodeId
                      << ", reason=global-standby-available, standbyNodeId=" << globalStandby.nodeId;
        return false;
    }
    // 故障节点是全局主且没有全局备时，最后确认是否存在组内备。
    if (HasLocalStandbyForBmcFault(curRoleInfo, electionModule)) {
        return false;
    }
    UBSE_LOG_WARN << "Skip BMC fault handling, electionMode=hierarchical, nodeId=" << curRoleInfo.nodeId
                  << ", reason=global-master-without-local-or-global-standby, globalStandby"
                  << FormatRetCode(standbyRet);
    return true;
}

// 按 1D、指定根节点和分层选举的角色语义判断是否直接 ACK
static bool ShouldSkipBmcFaultHandling(const UbseRoleInfo& curRoleInfo)
{
    // 选举模块为空时无法判断接管节点，直接 ACK 允许 SysSentry 执行下电。
    auto electionModule = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "Skip BMC fault handling because election module is unavailable, topology=clos, nodeId="
                       << curRoleInfo.nodeId << ", reason=election-module-unavailable";
        return true;
    }
    const auto rootIps = ubse::nodeMgr::GetRootIpList();
    if (!rootIps.empty()) {
        return ShouldSkipSpecifiedRootNodeBmcFault(curRoleInfo, rootIps);
    }
    return ShouldSkipHierarchicalElectionBmcFault(curRoleInfo, electionModule);
}

// 尝试直接回复 BMC 故障：有返回值表示本次处理已结束，空值表示继续主备倒换和故障转发。
static std::optional<UbseResult> TryAcknowledgeBmcFault(const std::string& msgId, UbseRoleInfo& curRoleInfo)
{
    const bool isClosType = ubse::adapter_plugins::smbios::UbseSmbios::GetInstance().IsClosType();
    // 1D 单节点直接 ACK 且无需查询当前角色。
    bool shouldAck = !isClosType && IsOnlyOneNodeInCluster();
    if (!shouldAck) {
        const auto ret = UbseGetCurrentNodeInfo(curRoleInfo);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Get current node info failed, msgId=" << msgId << ", " << FormatRetCode(ret);
            return ret;
        }
        shouldAck = isClosType && ShouldSkipBmcFaultHandling(curRoleInfo);
    }
    if (!shouldAck) {
        return std::nullopt;
    }
    // 所有组网共用唯一提前 ACK 出口，避免重复拼接和上报。
    UBSE_LOG_INFO << "Acknowledge BMC fault without role switch, topology=" << (isClosType ? "clos" : "one-dimensional")
                  << ", nodeId=" << curRoleInfo.nodeId << ", msgId=" << msgId;
    const std::string ackStr = msgId + "_" + std::to_string(UBSE_OK);
    return ReportAckToSysSentry(ALARM_REBOOT_ACK_EVENT, ackStr);
}

// 处理 BMC 故障：必要时先完成主备倒换，再由正常主节点处理故障并回复 SysSentry。
UbseResult UbseRasHandler::HandleBMCFault(const std::string& info)
{
    uint64_t validateMsgId; // 仅用于外部数据校验，无需使用
    if (ubse::utils::ConvertStrToUint64(info, validateMsgId) != UBSE_OK) {
        UBSE_LOG_ERROR << "Invalid msg id, expect integer represented as a string";
        return UBSE_ERROR_INVAL;
    }
    UbseRoleInfo curRoleInfo;
    if (const auto ackRet = TryAcknowledgeBmcFault(info, curRoleInfo); ackRet.has_value()) {
        return ackRet.value();
    }
    UbseRoleInfo masterRoleInfo;
    auto ret = SendSwitchRoleToStandby(curRoleInfo, info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Do switch role failed, " << FormatRetCode(ret);
        return ret;
    }
    ret = UbseGetMasterInfo(masterRoleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master node info failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "master nodeId=" << masterRoleInfo.nodeId << ", current nodeId=" << curRoleInfo.nodeId;
    ret = ReportBMCFaultToMaster(info, curRoleInfo.nodeId, masterRoleInfo.nodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Report BMC fault to master failed, msgId=" << info << ", faultNodeId=" << curRoleInfo.nodeId
                       << ", masterNodeId=" << masterRoleInfo.nodeId << ", " << FormatRetCode(ret);
        return ret;
    }
    auto ackStr = std::to_string(ret);
    ackStr = info + "_" + ackStr;
    return ReportAckToSysSentry(ALARM_REBOOT_ACK_EVENT, ackStr);
}

// OOM事件信息，从info字符串中解析得到
struct OomEventInfo {
    std::string msgId;     // 消息ID
    int sync;              // 是否需要响应sysSentry
    int reason;            // OOM原因，取值为2代表大页OOM
    int nrNid;             // 触发OOM的NUMA节点ID数量
    std::vector<int> nids; // NUMA节点ID列表
};

static std::vector<int> SplitNids(const std::string& nidStr)
{
    std::vector<int> nids;
    std::stringstream ss(nidStr);
    std::string item;
    while (std::getline(ss, item, ',')) {
        try {
            int nid = std::stoi(item);
            if (nid >= 0) {
                nids.push_back(nid);
            }
        } catch (const std::exception& e) {
            UBSE_LOG_ERROR << "SplitNids exception=" << e.what();
        }
    }
    return nids;
}

// 从OOM info中解析出sync、reason、nr_nid、nid
static UbseResult ParseOomEventInfo(const std::string& info, OomEventInfo& eventInfo)
{
    std::regex pattern(R"(^(\d+)_\{nr_nid:(\d+),nid:\[(-?\d+(?:,-?\d+)*)\],)"
                       R"(sync:(\d+),timeout:(\d+),reason:(\d+)\})");
    std::smatch match;
    if (!std::regex_match(info, match, pattern)) {
        UBSE_LOG_ERROR << "The oom message format is invalid, info=" << info;
        return UBSE_ERROR_INVAL;
    }
    constexpr uint32_t nrNidIdx = 2;
    constexpr uint32_t nidArrayIdx = 3;
    constexpr uint32_t syncIdx = 4;
    constexpr uint32_t reasonIdx = 6;
    UbseResult convertRet = UBSE_OK;
    eventInfo.msgId = match[1].str();
    uint64_t unusedMsgId;
    if (ubse::utils::ConvertStrToUint64(eventInfo.msgId, unusedMsgId) != UBSE_OK) {
        UBSE_LOG_ERROR << "Invalid msg id, expect integer represented as a string";
        return UBSE_ERROR_INVAL;
    }
    convertRet |= ubse::utils::ConvertStrToInt(match[nrNidIdx].str(), eventInfo.nrNid);
    eventInfo.nids = SplitNids(match[nidArrayIdx].str());
    convertRet |= ubse::utils::ConvertStrToInt(match[syncIdx].str(), eventInfo.sync);
    convertRet |= ubse::utils::ConvertStrToInt(match[reasonIdx].str(), eventInfo.reason);
    if (convertRet != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to parse oom event info";
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

void UbseRasHandler::ExecuteFaultHandlerTask(ALARM_FAULT_TYPE faultType, const std::string& faultInfo,
                                             const std::string& msgId, const std::string& faultId, bool needReportAck)
{
    UBSE_LOG_INFO << "ExecuteFaultHandlerTask, faultType=" << faultType << ", faultInfo=" << faultInfo
                  << ", msgId=" << msgId << ", faultId=" << faultId
                  << ", needReportAck=" << static_cast<int>(needReportAck);
    if (!UbseRasHandler::GetInstance().AddPendingFaultId(faultId)) {
        UBSE_LOG_INFO << "Fault is being processed by another thread, skip, faultId=" << faultId;
        return;
    }
    auto ret = UbseRasHandler::GetInstance().ExecuteFaultHandler(faultType, faultInfo, faultId);
    if (ret == UBSE_OK) {
        UBSE_LOG_INFO << "Fault handle success, faultId=" << faultId;
        // 完全处理成功后，响应sysSentry
        std::string ackStr = msgId + "_" + std::to_string(ret);
        if (needReportAck && ReportAckToSysSentry(faultType + 1, ackStr) != UBSE_OK) {
            UBSE_LOG_WARN << "Report ack to sysSentry failed, msgId=" << msgId;
        }
    }
    UBSE_LOG_INFO << "Fault handle end, ret=" << FormatRetCode(ret);
    UbseRasHandler::GetInstance().DelPendingFaultId(faultId);
}

std::string BuildOomStrFromEventInfo(const OomEventInfo& eventInfo)
{
    // 格式为"nrNid_numa1_numa2_..._reason"
    std::string oomStr = std::to_string(eventInfo.nids.size());
    for (int nid : eventInfo.nids) {
        oomStr += "_" + std::to_string(nid);
    }
    oomStr += "_" + std::to_string(eventInfo.reason);
    return oomStr;
}

UbseResult UbseRasHandler::HandleOomFault(alarm_msg* msg)
{
    if (msg == nullptr) {
        UBSE_LOG_ERROR << "msg is nullptr";
        return UBSE_ERROR_NULLPTR;
    }
    std::string info(msg->pucParas);
    OomEventInfo eventInfo;
    if (ParseOomEventInfo(info, eventInfo) != UBSE_OK) {
        UBSE_LOG_WARN << "Oom message format is invalid";
        return UBSE_ERROR_INVAL;
    }
    std::string msgId = eventInfo.msgId;
    auto taskModule = ubse::context::UbseContext::GetInstance().GetModule<task_executor::UbseTaskExecutorModule>();
    if (taskModule == nullptr) {
        UBSE_LOG_ERROR << "Get task module failed";
        return UBSE_ERROR;
    }
    ubse::task_executor::UbseTaskExecutorPtr executor = taskModule->Get(UBSE_RAS_FAULT_HANDLE_THREAD_POOL);
    if (executor == nullptr) {
        UBSE_LOG_WARN << "Get oom fault handle thread pool executor failed";
        return UBSE_ERROR_NULLPTR;
    }
    const std::string oomStr = BuildOomStrFromEventInfo(eventInfo);
    bool submitSuccess = executor->Execute([needReportAck = eventInfo.sync == 1, msgId, oomStr]() -> void {
        UbseRasHandler::GetInstance().ExecuteFaultHandlerTask(ALARM_OOM_EVENT, oomStr, msgId, msgId, needReportAck);
    });
    if (!submitSuccess) {
        UBSE_LOG_WARN << "Submit oom fault handler task failed, msgId=" << msgId;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseRasHandler::HandleMemoryFault(ALARM_FAULT_TYPE faultType, std::string info)
{
    auto ret = ExecuteFaultHandler(faultType, info);
    UBSE_LOG_DEBUG << "Received alarm message=" << info;
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Fault execute failed, " << FormatRetCode(ret);
        return ret;
    }
    std::string ackStr = info + "_" + std::to_string(ret);
    UBSE_LOG_DEBUG << "Fault execute result is " << ackStr;
    return ret;
}

// 注册RAS模块全部RPC服务（BMC重启、主备切换、PANIC/内核重启转发与结果通知）
static UbseResult RegisterRasRpcServices(std::shared_ptr<UbseComModule>& comModulePtr)
{
    UbseComBaseMessageHandlerPtr ubseRasComHandlerPtr = new (std::nothrow) UbseRasComHandler();
    UbseComBaseMessageHandlerPtr ubseRasSwitchHandlerPtr = new (std::nothrow) UbseRasSwitchRoleHandler();
    UbseComBaseMessageHandlerPtr ubseRasPanicRebootHandlerPtr = new (std::nothrow) UbseRasPanicRebootHandler();
    UbseComBaseMessageHandlerPtr ubseRasPanicResultHandlerPtr = new (std::nothrow) UbseRasPanicRebootResultHandler();
    if (ubseRasComHandlerPtr == nullptr || ubseRasSwitchHandlerPtr == nullptr ||
        ubseRasPanicRebootHandlerPtr == nullptr || ubseRasPanicResultHandlerPtr == nullptr) {
        UBSE_LOG_ERROR << "Ubse ras com handler ptr is nullptr. ";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModulePtr->RegRpcService<UbseRasMessage, UbseRasMessage>(ubseRasComHandlerPtr);
    ret |= comModulePtr->RegRpcService<UbseRasMessage, UbseRasMessage>(ubseRasSwitchHandlerPtr);
    ret |=
        comModulePtr->RegRpcService<UbseRasPanicRebootMessage, UbseRasPanicRebootMessage>(ubseRasPanicRebootHandlerPtr);
    ret |=
        comModulePtr->RegRpcService<UbseRasPanicRebootMessage, UbseRasPanicRebootMessage>(ubseRasPanicResultHandlerPtr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Reg rpc service fail, " << FormatRetCode(ret);
    }
    return ret;
}

// 订阅集群拓扑变化事件，按网络故障执行已注册的处理回调
UbseResult UbseRasHandler::SubscribeClusterTopoChangeEvent()
{
    std::string eventId = UBSE_EVENT_CLUSTER_TOPOLOGY_CHANGE;
    auto ret = UbseSubEvent(eventId, [](std::string& eventId, const std::string& eventMessage) {
        auto ret = UbseRasHandler::GetInstance().ExecuteFaultHandler(ALARM_NET_FAULT, eventMessage);
        UBSE_LOG_INFO << "Execute net fault finish. ";
        return ret;
    });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Rack sub event failed, " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult UbseRasHandler::StartRasHandler()
{
    auto comModulePtr = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModulePtr == nullptr) {
        UBSE_LOG_ERROR << "Get ubse com module ptr fail. ";
        return UBSE_ERROR_INVAL;
    }
    if (auto ret = RegisterRasRpcServices(comModulePtr); ret != UBSE_OK) {
        return ret;
    }
    if (auto ret = SubscribeClusterTopoChangeEvent(); ret != UBSE_OK) {
        return ret;
    }
    if (auto ret = SubscribePanicRebootForwardFaultEvent(); ret != UBSE_OK) {
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseRasHandler::RegisterAlarmFaultHandler(const AlarmHandler& alarmHandler)
{
    if (alarmHandler.name.empty()) {
        UBSE_LOG_WARN << "The fault handler's name is empty. ";
    }
    if (alarmHandler.handler == nullptr) {
        UBSE_LOG_ERROR << "Register alarm handler failed, handler is null. ";
        return UBSE_ERROR_NULLPTR;
    }
    faultHandlerMap[alarmHandler.alarmFaultEvent][alarmHandler.priority].emplace_back(
        std::make_pair(alarmHandler.name, alarmHandler.handler));
    return UBSE_OK;
}

UbseResult UbseRasHandler::ExecuteFaultHandler(ALARM_FAULT_TYPE faultType, const std::string& faultInfo,
                                               const std::string& msg)
{
    if (faultHandlerMap.find(faultType) == faultHandlerMap.end()) {
        UBSE_LOG_WARN << "No handler register, type=" << faultType << "; info=" << faultInfo;
        return UBSE_OK;
    }
    auto handlersMap = faultHandlerMap[faultType];
    for (const auto& handlers : handlersMap) {
        for (const auto& handler : handlers.second) {
            UBSE_LOG_INFO << "Handler execute, type=" << faultType << "; priority=" << static_cast<int>(handlers.first)
                          << "; name=" << handler.first;
            if (IsHandlerDone(msg, handler.first)) {
                UBSE_LOG_INFO << "Handler " << handler.first << " is already done. ";
                continue;
            }
            if (handler.second == nullptr) {
                continue;
            }
            auto retTmp = handler.second(faultType, faultInfo);
            SetHandlerResult(faultType, msg, handler.first, retTmp);
            UBSE_LOG_INFO << "Handler execute finished, type=" << faultType << "; name=" << handler.first
                          << "; priority=" << static_cast<int>(handlers.first) << "; result=" << retTmp;
        }
    }
    return GetResultFromHandlersByMsg(msg);
}

UbseResult UbseRasHandler::ExecuteFaultHandler(ALARM_FAULT_TYPE faultType, const std::string& faultInfo)
{
    if (faultHandlerMap.find(faultType) == faultHandlerMap.end()) {
        UBSE_LOG_WARN << "No handler register, type=" << faultType << ", info=" << faultInfo;
        return UBSE_OK;
    }
    auto handlersMap = faultHandlerMap[faultType];
    UbseResult result = UBSE_OK;
    for (const auto& handlers : handlersMap) {
        for (const auto& handler : handlers.second) {
            UBSE_LOG_DEBUG << "Handler execute, type=" << faultType << "; priority=" << static_cast<int>(handlers.first)
                           << "; name=" << handler.first;
            auto retTmp = handler.second(faultType, faultInfo);
            result |= retTmp;
            UBSE_LOG_INFO << "Handler execute finished, type=" << faultType << "; name=" << handler.first
                          << "; priority=" << static_cast<int>(handlers.first) << "; result=" << retTmp;
        }
    }
    return result;
}

uint32_t UbseRasHandler::UnRegisterAlarmFaultHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string& name)
{
    if (faultHandlerMap.find(alarmFaultEvent) == faultHandlerMap.end()) {
        UBSE_LOG_ERROR << "Can't find alarm fault event, event=" << alarmFaultEvent << ", name=" << name;
        return UBSE_ERROR_NULLPTR;
    }
    for (auto& handlers : faultHandlerMap[alarmFaultEvent]) {
        for (size_t i = 0; i < handlers.second.size(); i++) {
            if (handlers.second[i].first == name) {
                handlers.second.erase(handlers.second.begin() + i);
                return UBSE_OK;
            }
        }
    }
    return UBSE_ERROR;
}

bool IsMemInitFinished()
{
    auto curNode = ubse::nodeController::UbseNodeController::GetInstance().GetCurNode();
    if (curNode.nodeId.empty()) {
        UBSE_LOG_WARN << "Get current node failed, node id is empty. ";
        return false;
    }
    if (curNode.clusterState != ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING) {
        UBSE_LOG_WARN << "Ubse node controller init didn't finish. ";
        return false;
    }
    return true;
}

std::string ToLowerEid(const std::string& eid)
{
    std::string lowerEid;
    lowerEid.reserve(eid.size());
    std::transform(eid.begin(), eid.end(), std::back_inserter(lowerEid),
                   [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
    return lowerEid;
}

std::string QueryNodeIdByEid(const std::string& eid)
{
    const std::string lowerEid = ToLowerEid(eid);
    std::map<adapter_plugins::mti::UbseMtiIouInfo, adapter_plugins::mti::UbseMtiEidGroup> comUrmaInfoMap{};
    auto result = UbseMtiInterface::GetInstance().GetMtiComEid(comUrmaInfoMap);
    if (result != UBSE_OK) {
        UBSE_LOG_WARN << "Get all socket eid failed, " << ubse::log::FormatRetCode(result);
        return "";
    }
    std::unordered_map<std::string, std::string> eids;
    for (const auto& info : comUrmaInfoMap) {
        eids[ToLowerEid(info.second.primaryEid)] = info.first.slotId;
    }
    if (eids.find(lowerEid) == eids.end()) {
        UBSE_LOG_INFO << "Query EID=" << lowerEid;
        for (const auto& item : comUrmaInfoMap) {
            UBSE_LOG_INFO << "SlotId=" << item.first.slotId << "; eid=" << ToLowerEid(item.second.primaryEid);
        }
        return "";
    }
    return eids[lowerEid];
}

UbseResult UbseRasHandler::RegisterNodeHandler(const NodeHandlerType& handlerType, const NodeHandler& handler)
{
    if (static_cast<int>(handlerType) >= static_cast<int>(NodeHandlerType::NODE_HANDLER_TYPE_NUM)) {
        UBSE_LOG_ERROR << "Handler type invalid, type=" << static_cast<int>(handlerType);
        return UBSE_ERROR_INVAL;
    }
    if (handler == nullptr) {
        UBSE_LOG_ERROR << "Handler is nullptr";
        return UBSE_ERROR_NULLPTR;
    }
    nodeHandlerMap[handlerType].emplace_back(handler);
    UBSE_LOG_INFO << "Success to register a node handler for type=" << static_cast<int>(handlerType);
    return UBSE_OK;
}

const int CALL_NODE_HANDLE_RETRY_CNT = NO_64;
const int CALL_NODE_HANDLE_RETRY_WAIT_SECOND = NO_2;
UbseResult CallOneNodeHandleRetry(NodeHandler& handler, const std::string& nodeId)
{
    UBSE_LOG_INFO << "Start to call node handler";
    int cnt = 0;
    auto ret = handler(nodeId);
    while (cnt < CALL_NODE_HANDLE_RETRY_CNT && ret != UBSE_OK) {
        sleep(CALL_NODE_HANDLE_RETRY_WAIT_SECOND);
        ++cnt;
        ret = handler(nodeId);
    }
    UBSE_LOG_INFO << "Call node handler finish";
    return ret;
}

UbseResult UbseRasHandler::CallNodeHandle(const NodeHandlerType& handlerType, const std::string& nodeId)
{
    if (nodeHandlerMap.find(handlerType) == nodeHandlerMap.end() || nodeHandlerMap[handlerType].empty()) {
        UBSE_LOG_ERROR << "Handler not exist, type=" << static_cast<int>(handlerType);
        return UBSE_ERROR_INVAL;
    }
    if (nodeId.empty()) {
        UBSE_LOG_ERROR << "Node id is empty.";
        return UBSE_ERROR_INVAL;
    }
    // 如果执行失败，则故障重新上报后重试
    for (auto& handler : nodeHandlerMap[handlerType]) {
        if (handler == nullptr) {
            UBSE_LOG_ERROR << "Node handler is empty";
            return UBSE_ERROR;
        }
        if (handler(nodeId) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to call node handler for type=" << static_cast<int>(handlerType);
            return UBSE_ERROR;
        }
    }
    UBSE_LOG_INFO << "Success to call node handler for type=" << static_cast<int>(handlerType);
    return UBSE_OK;
}

void UbseRasHandler::ClearAllMsgId()
{
    UBSE_LOG_INFO << "Clear all processed msg id";
    ClearAllHandlerResults();
}

bool UbseRasHandler::IsPendingFaultExisted(const std::string& faultId)
{
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&pendingFaultIdLock);
    return pendingFaultId.find(faultId) != pendingFaultId.end();
}

bool UbseRasHandler::AddPendingFaultId(const std::string& faultId)
{
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&pendingFaultIdLock);
    if (pendingFaultId.find(faultId) != pendingFaultId.end()) {
        UBSE_LOG_WARN << "Add pending fault id failed, faultId=" << faultId << ", it has been added";
        return false;
    }
    pendingFaultId.insert(faultId);
    UBSE_LOG_INFO << "Add pending fault id success, faultId=" << faultId;
    return true;
}

void UbseRasHandler::DelPendingFaultId(const std::string& faultId)
{
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&pendingFaultIdLock);
    pendingFaultId.erase(faultId);
    UBSE_LOG_INFO << "Delete pending fault id success, faultId=" << faultId;
}

UbseResult UbseRasHandler::RegisterFaultHandleResultClearTimer()
{
    UBSE_LOG_INFO << "Register fault handle result clean timer";
    const uint32_t cleanInterval = 5 * 60; // 故障结果清理间隔，单位: 秒，默认5分钟
    auto ret = ubse::timer::UbseTimerHandlerRegister(
        UBSE_RAS_FAULT_HANDLE_RESULT_CLEAN_TIMER,
        []() -> UbseResult {
            if (g_globalStop) {
                UBSE_LOG_INFO << "detect global stop flag, will stop fault handle result clean timer";
                ubse::timer::UbseTimerHandlerUnregister(UBSE_RAS_FAULT_HANDLE_RESULT_CLEAN_TIMER);
                return UBSE_OK;
            }
            SubmitClearExpiredHandlerResult();
            return UBSE_OK;
        },
        cleanInterval);
    return ret;
}

} // namespace ubse::ras
