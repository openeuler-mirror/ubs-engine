/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/
#include "ubse_mem_controller_fault_handle.h"

#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>

#include "ubse_context.h"
#include "ubse_error.h"

#include "ubse_api_server_module.h"
#include "ubse_conf_module.h"
#include "ubse_election_module.h"
#include "ubse_logger.h"
#include "ubse_mmi_interface.h"

#include "ubse_com_op_code.h"
#include "ubse_ipc_common_def.h"
#include "ubse_ipc_utils.h"
#include "ubse_json_util.h"
#include "ubse_mem_controller_share_api.h"
#include "ubse_mem_debt_info.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_serial_util.h"

namespace ubse::mem::controller {
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::election;
using namespace ubse::context;
using namespace ubse::ipc;
using namespace api::server;
using namespace ubse::config;
using namespace ubse::com;
using namespace ubse::serial;

UBSE_DEFINE_THIS_MODULE("ubse");

struct ThreadControl {
    std::mutex mtx;
    std::condition_variable cv;
    bool ready = false;
    std::set<std::string> replyNodes;
};

const std::string MEM_FAULT_ALAUBSE_NAME = "MemFaultAlarm";
const std::string MEM_FAULT_DELIVER_TASK_PREFIX = "MemFaultDeliver";
const std::string MEM_FAULT_INFO_JSON_MEM_ID = "memid";
const std::string MEM_FAULT_INFO_JSON_RAS_TYPE = "raw_ubus_mem_err_type";
const uint8_t RAS_REPORT_RETRY_TIME = 5;
const uint8_t RAS_REPORT_WAIT_TIME = 3;
const uint8_t RAS_NOTIFY_WAIT_TIME = 5;
const uint8_t CHECK_INTERVAL = 1;
const uint8_t REPORT_RETRY_TIMES = 5;    // 内存故障场景，故障节点向主节点上报故障最大重试次数
const uint8_t REPORT_WAIT_INTERVAL = 20; // 内存故障场景，故障节点向主节点上报故障重试间隔，单位秒

UbseTaskExecutorPtr UbseMemFaultManager::executorPtr = nullptr;
std::unordered_map<std::string, ThreadControl> g_reportTaskCtrlMap; // memName, ctrlInfo
std::shared_mutex g_reportTaskCtrlMapMtx;

// 调用前需加锁
template <typename value>
static void SafeDeleteMapElem(const std::string &key, std::unordered_map<std::string, value> &map)
{
    auto itor = map.find(key);
    if (itor != map.end()) {
        map.erase(itor);
    }
}

static UbseResult GetCurrentNodeRole(std::string &role)
{
    UbseRoleInfo curNodeInfo{};
    auto ret = UbseGetCurrentNodeInfo(curNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get the information of current node.";
        return UBSE_ERROR;
    }
    role = curNodeInfo.nodeRole;
    return ret;
}

static UbseResult GetCurrentNodeId(std::string &nodeId)
{
    UbseRoleInfo curNodeInfo{};
    auto ret = UbseGetCurrentNodeInfo(curNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get the information of current node.";
        return UBSE_ERROR;
    }
    nodeId = curNodeInfo.nodeId;
    return ret;
}

static UbseResult GetMasterNodeId(std::string &nodeId)
{
    UbseRoleInfo masterInfo{};
    auto ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get the information of master node.";
        return UBSE_ERROR;
    }
    nodeId = masterInfo.nodeId;
    return ret;
}

static UbseComEndpoint GetMemFaultComEndpoint(uint16_t opCode, const std::string &nodeId)
{
    return UbseComEndpoint{
        .moduleId = static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_FAULT),
        .serviceId = static_cast<uint32_t>(opCode),
        .address = nodeId,
    };
}

static UbseResult GetFaultTypeFromMsg(const UbseMemFaultMsg &msg, UbMemFaultType &type)
{
    rapidjson::Document doc;
    doc.Parse(msg.info.faultInfo_.c_str());
    if (doc.HasParseError()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to parse fault info which memName=" << msg.info.memName_ << ", "
                       << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    uint32_t faultType = UB_MEM_HEALTHY;
    auto ret = UbseJsonUtil::GetUintFromJsonPtr(doc, MEM_FAULT_INFO_JSON_RAS_TYPE, faultType);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get fault type from json which memName=" << msg.info.memName_
                       << ", " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    type = static_cast<UbMemFaultType>(faultType);
    return UBSE_OK;
}

static std::string QueryMemNameById(const std::unordered_map<std::string, UbseMemShareBorrowExportObj> &objMap,
                                    uint64_t memId)
{
    // 查找obj中是否包含目标memId
    auto searchIdLambda = [memId](std::pair<std::string, UbseMemShareBorrowExportObj> objPair) -> bool {
        auto &obmmInfoVec = objPair.second.status.exportObmmInfo;
        auto targetObmm = std::find_if(obmmInfoVec.begin(), obmmInfoVec.end(),
                                       [memId](UbseMemObmmInfo info) -> bool { return info.memId == memId; });
        return targetObmm != obmmInfoVec.end();
    };
    auto itor = std::find_if(objMap.begin(), objMap.end(), searchIdLambda);
    return itor == objMap.end() ? "" : itor->first;
}

static UbseResult SerializeMemFaultMsg(const UbseMemFaultMsg &msg, UbseByteBuffer &outData)
{
    UbseSerialization output;
    output << msg.info.memId_ << msg.nodeNum << msg.info.memName_ << msg.faultNode << msg.notifiedNode
           << msg.info.faultInfo_;
    if (!output.Check()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to serialize mem fault info, "
                       << FormatRetCode(UBSE_ERROR_SERIALIZE_FAILED);
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    outData = UbseByteBuffer{
        .data = output.GetBuffer(true), .len = output.GetLength(), .freeFunc = [](uint8_t *data) -> void {
            UbseSerialFreeFunc(data);
        }};
    return UBSE_OK;
}

static UbseResult DeserializeMemFaultMsg(const UbseByteBuffer &outData, UbseMemFaultMsg &msg)
{
    UbseDeSerialization input(outData.data, outData.len);
    input >> msg.info.memId_ >> msg.nodeNum >> msg.info.memName_ >> msg.faultNode >> msg.notifiedNode >>
        msg.info.faultInfo_;
    if (!input.Check()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to deserialize mem fault info, "
                       << FormatRetCode(UBSE_ERROR_DESERIALIZE_FAILED);
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return UBSE_OK;
}

static UbseResult SendMemFaultInfo(const UbseMemFaultMsg &msg, const std::string &nodeId, uint16_t opType)
{
    UbseByteBuffer request{};
    auto ret = SerializeMemFaultMsg(msg, request);
    if (ret != UBSE_OK) {
        return ret;
    }

    auto respHandler = [](void *ctx, const UbseByteBuffer &respData, uint32_t resCode) -> void {
        return;
    };
    return UbseRpcAsyncSend(GetMemFaultComEndpoint(opType, nodeId), request, nullptr, respHandler);
}

static std::vector<std::string> GetFaultMemUsers(const std::string &memName)
{
    auto debt = GetNodeMemDebtInfoMap();
    std::vector<std::string> memUsers{};

    for (auto [nodeId, nodeDebt] : debt) {
        if (nodeDebt.shareImportObjMap.find(memName) != nodeDebt.shareImportObjMap.end() &&
            nodeDebt.shareImportObjMap[memName].status.state != UBSE_MEM_IMPORT_DESTROYED &&
            nodeDebt.shareImportObjMap[memName].status.state != UBSE_MEM_IMPORT_DESTROYING &&
            nodeDebt.shareImportObjMap[memName].status.state != UBSE_MEM_IMPORT_DESTROYING_WAIT) {
            memUsers.emplace_back(nodeId);
        }
    }
    return memUsers;
}

static UbseResult GenerateLongLinkRequst(const UbseMemFaultMsg &msg, UbseRequestMessage &req)
{
    req.header.opCode = UBSE_LONGLINK_FAULT;
    req.header.moduleCode = ubse_ipc_module_code_t::UBSE_LONG_LINK_REGISTER;

    UbMemFaultType faultType = UB_MEM_HEALTHY;
    auto ret = GetFaultTypeFromMsg(msg, faultType);
    if (ret != UBSE_OK) {
        return ret;
    }

    UbseShmFault shmFault{
        .shmName = msg.info.memName_,
        .memId = msg.info.memId_,
        .type = static_cast<UbseIpcMemFaultType>(faultType),
    };

    uint8_t *buffer = nullptr;
    size_t size = 0;
    ret = SerializeShmFault(shmFault, buffer, size);
    if (ret != UBSE_OK || buffer == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to generate share memory fault message which memName="
                       << shmFault.shmName << ", " << FormatRetCode(ret);
        return ret;
    }

    req.body = buffer;
    req.header.bodyLen = size;
    return UBSE_OK;
}

static void WaitLongLinkResponse(const UbseMemFaultMsg &faultMsg, const std::vector<uint64_t> reqList, uint32_t *count)
{
    std::atomic<bool> stopFlag{false};
    auto fut = std::async(
        std::launch::async,
        [reqList](uint32_t *ctx, std::atomic<bool> &flag) -> bool {
            while (!flag.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(CHECK_INTERVAL));
                if (ctx == nullptr) {
                    UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get ctx pointer when check long link response.";
                    return false;
                }
                uint32_t *msgCount = static_cast<uint32_t *>(ctx);
                if (*msgCount >= reqList.size()) {
                    return true;
                }
            }
            return false;
        },
        count, std::ref(stopFlag));

    std::string masterNode;
    auto ret = GetMasterNodeId(masterNode);
    if (ret != UBSE_OK) {
        return;
    }
    if (fut.wait_for(std::chrono::seconds(RAS_NOTIFY_WAIT_TIME)) == std::future_status::timeout) {
        stopFlag.store(true);
    }
    bool asyncRet = fut.get();
    if (asyncRet) {
        SendMemFaultInfo(faultMsg, masterNode,
                         static_cast<uint16_t>(UbseMemFaultOpCode::UBSE_MEM_FAULT_NOTIFY_REPLY));
        return;
    }
    UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to reply notification to master which memName=" << faultMsg.info.memName_
                   << ".";
}

static void StartResponseCheckTask(const UbseMemFaultMsg &msg)
{
    std::thread(
        [](UbseMemFaultMsg faultMsg) -> void {
            // 调用长连接发送消息
            std::vector<uint64_t> reqList;
            auto &ctxRef = UbseContext::GetInstance();
            auto apiServerPtr = ctxRef.GetModule<UbseApiServerModule>();
            if (apiServerPtr == nullptr) {
                UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get api server module."
                               << FormatRetCode(UBSE_ERROR_NULLPTR);
                return;
            }

            auto respHandler = [](void *ctx, const UbseResponseMessage &resp) -> void {
                if (ctx == nullptr) {
                    UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get ctx pointer from long link handler.";
                    return;
                }
                uint32_t *count = static_cast<uint32_t *>(ctx);
                (*count) += 1;
            };
            uint32_t *respCount = new (std::nothrow) uint32_t;
            if (respCount == nullptr) {
                UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to create response counter.";
                return;
            }
            *respCount = 0;

            UbseRequestMessage longLinkReq{};
            auto ret = GenerateLongLinkRequst(faultMsg, longLinkReq);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to generate long link request." << FormatRetCode(ret);
                SafeDelete(respCount);
                return;
            }

            ret = apiServerPtr->AsyncSendLongLink(longLinkReq, static_cast<void *>(respCount), respHandler, reqList);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to send long link message." << FormatRetCode(ret);
                SafeDelete(respCount);
                return;
            }
            WaitLongLinkResponse(faultMsg, reqList, respCount);
            SafeDelete(respCount);
        },
        msg)
        .detach();
}

static bool ValidateAndGetShareHandleInfo(const std::string &faultId,
                                          std::shared_ptr<UbseApiServerModule> &apiServerPtr,
                                          debt::ShareHandleInfoVec &handleInfo)
{
    auto &ctxRef = UbseContext::GetInstance();
    apiServerPtr = ctxRef.GetModule<UbseApiServerModule>();
    if (apiServerPtr == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get api server module." << FormatRetCode(UBSE_ERROR_NULLPTR);
        return false;
    }

    std::string currentNodeId;
    auto ret = UbseGetCurrentNodeId(currentNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node id, ret=" << ret;
        return false;
    }
    if (currentNodeId == faultId) {
        UBSE_LOG_INFO << "Current node id is faultId, stop reporting.";
        return false;
    }

    ret = UbseQueryShareImportHandleByExportNodeId(currentNodeId, faultId, handleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get share handle info in faultId." << FormatRetCode(ret);
        return false;
    }
    if (handleInfo.empty()) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] Share handle vec is empty in faultId, stop reporting.";
        return false;
    }
    return true;
}

static void SendFaultShareHandleMessages(std::shared_ptr<UbseApiServerModule> &apiServerPtr,
    const debt::ShareHandleInfoVec &handleInfo,
    std::shared_ptr<std::atomic<uint32_t>> respCount)
{
    UbseRequestMessage longLinkReq{};
    longLinkReq.header.opCode = UBSE_LONGLINK_FAULT;
    longLinkReq.header.moduleCode = ubse_ipc_module_code_t::UBSE_LONG_LINK_REGISTER;
    std::vector<uint64_t> reqList;

    auto respHandler = [respCount](void *, const UbseResponseMessage &) {
        respCount->fetch_add(1);
    };

    for (const auto &info : handleInfo) {
        for (const auto &memId : info.memIds) {
            UbseShmFault shmFault{
                .shmName = info.name,
                .memId = memId,
                .type = static_cast<UbseIpcMemFaultType>(MEM_EXPORT_NODE_FAULT),
            };
            uint8_t *buffer = nullptr;
            size_t size = 0;
            auto ret = SerializeShmFault(shmFault, buffer, size);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to generate share handle info message, "
                               << FormatRetCode(ret);
                continue;
            }
            longLinkReq.body = buffer;
            longLinkReq.header.bodyLen = size;
            ret = apiServerPtr->AsyncSendLongLink(longLinkReq, nullptr, respHandler, reqList);
            SafeDelete(buffer);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to send long link message." << FormatRetCode(ret);
                return;
            }
        }
    }
}

static void StartFaultMemResponseCheckTask(const std::string &faultId)
{
    std::thread([faultId] {
        std::shared_ptr<UbseApiServerModule> apiServerPtr;
        debt::ShareHandleInfoVec handleInfo;
        if (!ValidateAndGetShareHandleInfo(faultId, apiServerPtr, handleInfo)) {
            return;
        }
        auto respCount = std::make_shared<std::atomic<uint32_t>>(0);
        SendFaultShareHandleMessages(apiServerPtr, handleInfo, respCount);
    }).detach();
}

UbseResult UbseMemFaultManager::CreateTaskExecutor(const std::string &name)
{
    auto &ctxRef = UbseContext::GetInstance();
    auto taskExec = ctxRef.GetModule<UbseTaskExecutorModule>();
    if (taskExec == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get task executor module, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }

    auto ret = taskExec->Create(name, NO_10, NO_1000);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to create executor tasks which memName=" << name << "."
                       << FormatRetCode(ret);
        return ret;
    }

    executorPtr = taskExec->Get(name);
    return ret;
}

UbseResult UbseMemFaultManager::RemoveTaskExecutor(const std::string &name)
{
    auto &ctxRef = UbseContext::GetInstance();
    auto taskExec = ctxRef.GetModule<UbseTaskExecutorModule>();
    if (taskExec == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get task executor module, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }

    taskExec->Remove(name);
    return UBSE_OK;
}

void UbseMemFaultManager::MemFaultReportTask(UbseMemFaultMsg msg)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Started to report mem fault to master.";
    // 记录任务信息
    {
        std::unique_lock lock(g_reportTaskCtrlMapMtx);
        g_reportTaskCtrlMap[msg.info.memName_];
    }

    // 重发机制
    auto retryTime = REPORT_RETRY_TIMES;
    for (uint32_t i = 0; i < retryTime; i++) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] Had sent for " << (i + 1) << " times.";
        std::string master;
        auto ret = GetMasterNodeId(master);
        if (ret != UBSE_OK) {
            continue;
        }
        ret = SendMemFaultInfo(msg, master,
                               static_cast<uint16_t>(UbseMemFaultOpCode::UBSE_MEM_FAULT_REPORT));
        if (ret != UBSE_OK) {
            std::this_thread::sleep_for(std::chrono::seconds(REPORT_WAIT_INTERVAL));
            // 发送失败重试
            UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to report mem fault to master node. "
                           << "Retry time is " << i;
            continue;
        }
        {
            UBSE_LOG_INFO << "[MEM_CONTROLLER] start to wait fault handler " << (i + 1) << " times.";
            std::shared_lock mapLock(g_reportTaskCtrlMapMtx);
            if (g_reportTaskCtrlMap.find(msg.info.memName_) == g_reportTaskCtrlMap.end()) {
                UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get thread info for memName=" << msg.info.memName_
                               << ".";
                break;
            }
            auto &ctrl = g_reportTaskCtrlMap[msg.info.memName_];
            std::unique_lock<std::mutex> ctrlLock(ctrl.mtx);
            // 超时等待
            ctrl.cv.wait_for(ctrlLock, std::chrono::seconds(REPORT_WAIT_INTERVAL), [&ctrl] { return ctrl.ready; });
            if (ctrl.ready) {
                UBSE_LOG_INFO << "[MEM_CONTROLLER] The report thread of memName=" << msg.info.memName_ << " is ready.";
                break;
            } else {
                UBSE_LOG_ERROR << "[MEM_CONTROLLER] wait timeout. " << (i + 1) << " times.";
            }
        }
    }
    // 执行完毕删除任务
    std::unique_lock lock(g_reportTaskCtrlMapMtx);
    SafeDeleteMapElem(msg.info.memName_, g_reportTaskCtrlMap);
    UBSE_LOG_INFO << "[MEM_CONTROLLER] The process of reporting mem fault is over. Fault information is : memId="
                  << msg.info.memId_ << ", memName=" << msg.info.memName_ << ".";
}

UbseResult UbseMemFaultManager::InitMemFaultManager()
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Started to register mem fault alarm.";
    auto ret = RegisterAlarmFaultHandler(ALARM_MEM_FAULT, MEM_FAULT_ALAUBSE_NAME, MemFaultHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to register mem fault alarm.";
        return UBSE_ERROR;
    }
    ret = CreateTaskExecutor(MEM_FAULT_DELIVER_TASK_PREFIX);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to create task executors. " << FormatRetCode(ret);
        return ret;
    }
    std::vector<UbseResult> regRets;
    // 主节点上报处理函数
    regRets.emplace_back(UbseRegRpcService(GetMemFaultComEndpoint(
        static_cast<uint16_t>(UbseMemFaultOpCode::UBSE_MEM_FAULT_REPORT), ""),
        MemFaultReportHandler));
    // 主节点故障通知回复处理函数
    regRets.emplace_back(UbseRegRpcService(GetMemFaultComEndpoint(
        static_cast<uint16_t>(UbseMemFaultOpCode::UBSE_MEM_FAULT_NOTIFY_REPLY), ""),
        MemFaultNotifyReplyHandler));
    // 故障内存借入节点通知处理函数
    regRets.emplace_back(UbseRegRpcService(GetMemFaultComEndpoint(
        static_cast<uint16_t>(UbseMemFaultOpCode::UBSE_MEM_FAULT_NOTIFY), ""),
        MemFaultNotifyHandler));
    // 故障内存发生节点上报回复函数
    regRets.emplace_back(UbseRegRpcService(GetMemFaultComEndpoint(
        static_cast<uint16_t>(UbseMemFaultOpCode::UBSE_MEM_FAULT_REPORT_REPLY), ""),
        MemFaultReportReplyHandler));
    for (auto regRet : regRets) {
        if (regRet != UBSE_OK) {
            UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to register handlers. " << FormatRetCode(ret);
            return ret;
        }
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Succeed to register mem fault alarm.";
    return ret;
}

UbseResult UbseMemFaultManager::DeInitMemFaultManager()
{
    std::string alarmName = MEM_FAULT_ALAUBSE_NAME;
    auto ret = UnRegisterAlarmFaultHandler(ALARM_MEM_FAULT, alarmName);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to unregister mem fault alarm.";
        return UBSE_ERROR;
    }
    ret = RemoveTaskExecutor(MEM_FAULT_DELIVER_TASK_PREFIX);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to remove task executors. " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Succeed to unregister mem fault alarm.";
    return ret;
}
UbseResult UbseMemFaultManager::MemReportWhenExportNodeOnFault(ALARM_FAULT_TYPE faultType, std::string &faultId)
{
    if (faultId.empty()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] faultNodeId is empty..";
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Starting to handle mem reports caused by export node failures, faultId="
                  << faultId << ".";
    StartFaultMemResponseCheckTask(faultId);
    return UBSE_OK;
}

UbseResult UbseMemFaultManager::GetMemNameById(uint64_t memId, std::string &memName)
{
    std::string nodeId;
    auto ret = GetCurrentNodeId(nodeId);
    if (ret != UBSE_OK) {
        return ret;
    }

    NodeMemDebtInfo debtInfo = GetNodeMemDebtInfoById(nodeId);
    // 需要保证memId对应的Name唯一性
    memName = QueryMemNameById(debtInfo.shareExportObjMap, memId);
    if (memName.empty()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get mem name. Query result is empty.";
    }
    return ret;
}

void UbseMemFaultManager::StartMemFaultReportTask(const UbseMemFaultMsg &msg)
{
    try {
        std::thread([=]() { MemFaultReportTask(msg); }).detach();
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Exception message is " << e.what();
    }
}

void UbseMemFaultManager::StartMemFaultDeliverTask(const std::vector<std::string> &users, const UbseMemFaultMsg &msg)
{
    for (auto &user : users) {
        executorPtr->Execute([=]() -> void {
            // 记录通知的节点
            UbseMemFaultMsg updatedMsg(msg);
            updatedMsg.notifiedNode = user;

            auto sendRet = SendMemFaultInfo(updatedMsg, user,
                                            static_cast<uint16_t>(UbseMemFaultOpCode::UBSE_MEM_FAULT_NOTIFY));
            if (sendRet != UBSE_OK) {
                UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to deliver mem fault to nodeId=" << user << ".";
            }
        });
    }
}

uint32_t UbseMemFaultManager::MemFaultHandler(ubse::ras::ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Started to handle ras fault report. faultInfo =" << faultInfo << ".";
    rapidjson::Document doc(rapidjson::kObjectType);
    doc.Parse(faultInfo.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to parse alarm info to json string.";
        return UBSE_ERROR;
    }

    uint64_t memId = 0;
    auto ret = UbseJsonUtil::GetUint64FromJsonPtr(doc, MEM_FAULT_INFO_JSON_MEM_ID, memId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Json content is [" << faultInfo << "].";
        return ret;
    }

    std::string memName;
    // 将memId转换为memName
    ret = GetMemNameById(memId, memName);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get mem name by id.";
        return ret;
    }
    auto msg = UbseMemFaultMsg(memId, memName, faultInfo);

    // 获取故障类型
    UbMemFaultType type;
    ret = GetFaultTypeFromMsg(msg, type);
    if (ret != UBSE_OK) {
        return ret;
    }

    // 记录故障信息
    std::string nodeId;
    ret = GetCurrentNodeId(nodeId);
    if (ret != UBSE_OK) {
        return ret;
    }
    msg.faultNode = nodeId;
    ret = UpdateFaultShareExportObj(nodeId, memId, memName, static_cast<UbMemFaultType>(type));
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to update object info for memory fault.";
        return ret;
    }

    // 启动异步线程任务进行故障上报
    StartMemFaultReportTask(msg);
    return ret;
}

void UbseMemFaultManager::MemFaultReportHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Started to handle mem fault report.";
    // 确定是主节点才能执行
    std::string role;
    auto ret = GetCurrentNodeRole(role);
    if (ret != UBSE_OK || role != ELECTION_ROLE_MASTER) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get node info or current node is not master.";
        return;
    }

    UbseMemFaultMsg msg;
    ret = DeserializeMemFaultMsg(req, msg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to deserialize mem fault report message.";
        return;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Mem fault report message is : memId=" << msg.info.memId_
                   << ", memName=" << msg.info.memName_ << ".";

    // 主节点记录故障信息
    UbMemFaultType type;
    ret = GetFaultTypeFromMsg(msg, type);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get fault type from fault report message.";
        return;
    }

    ret = UpdateFaultShareExportObj(msg.faultNode, msg.info.memId_, msg.info.memName_, type);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to update object info for memory fault.";
        return;
    }
    // 根据name查找共享内存使用方
    auto users = GetFaultMemUsers(msg.info.memName_);
    if (users.empty()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get any fault memory user.";
        return;
    }
    msg.nodeNum = users.size();

    // 异步发送通知借入端
    StartMemFaultDeliverTask(users, msg);
}

static bool IsReportReplyOver(const UbseMemFaultMsg &msg, ThreadControl &ctrl)
{
    ctrl.replyNodes.insert(msg.notifiedNode);
    return ctrl.replyNodes.size() == msg.nodeNum;
}

void UbseMemFaultManager::MemFaultReportReplyHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Started to handle mem fault report back.";
    UbseMemFaultMsg msg;
    auto ret = DeserializeMemFaultMsg(req, msg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to handle report reply due to deserialization error.";
        return;
    }

    // 唤醒指定wait的线程
    std::shared_lock ctrlLock(g_reportTaskCtrlMapMtx);
    auto ctrlItor = g_reportTaskCtrlMap.find(msg.info.memName_);
    if (ctrlItor == g_reportTaskCtrlMap.end()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get thread info for memName=" << msg.info.memName_ << ".";
        return;
    }
    if (!IsReportReplyOver(msg, ctrlItor->second)) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] Waiting for all fault memory user reply to memName=" << msg.info.memName_
                       << ".";
        return;
    }
    if (ctrlItor != g_reportTaskCtrlMap.end()) {
        ctrlItor->second.ready = true;
        ctrlItor->second.cv.notify_one();
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Finished handle mem fault report back.";
}

void UbseMemFaultManager::MemFaultNotifyHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Received notification from master.";
    UbseMemFaultMsg msg;
    auto ret = DeserializeMemFaultMsg(req, msg);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] Failed to deserialize notification.";
        return;
    }
    try {
        StartResponseCheckTask(msg);
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Exception message is " << e.what();
    }
}

void UbseMemFaultManager::MemFaultNotifyReplyHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Received reply from notified node.";
    // 确定是主节点才能执行
    std::string role;
    auto ret = GetCurrentNodeRole(role);
    if (ret != UBSE_OK || role != ELECTION_ROLE_MASTER) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get node info or current node is not master.";
        return;
    }
    UbseMemFaultMsg msg;
    ret = DeserializeMemFaultMsg(req, msg);
    if (ret != UBSE_OK) {
        return;
    }

    ret = SendMemFaultInfo(msg, msg.faultNode,
                           static_cast<uint16_t>(UbseMemFaultOpCode::UBSE_MEM_FAULT_REPORT_REPLY));
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to reply to mem fault node=" << msg.faultNode << ", "
                       << FormatRetCode(UBSE_ERROR_NULLPTR);
    }
}
} // namespace ubse::mem::controller