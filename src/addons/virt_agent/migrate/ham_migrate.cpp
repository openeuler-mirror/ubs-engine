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

#include "ham_migrate.h"

#include <climits>
#include <ubse_api_server.h>
#include <ubse_api_server_def.h>
#include <ubse_com.h>
#include <ubse_election.h>
#include <ubse_error.h>
#include <ubse_logger.h>
#include <ubse_mem_controller.h>
#include <ubse_node.h>
#include <fstream>
#include <regex>
#include <set>
#include "ham_migrate_dst_info_message.h"
#include "ham_migrate_vm_info_storage.h"
#include "libvirt_helper.h"
#include "migrate_strategy.h"
#include "resource_query.h"
#include "response_info_message.h"
#include "ubs_virt_agent_object_def.h"
#include "vm_configuration.h"
#include "vm_http_util.h"
#include "vm_json_util.h"
#include "vm_migrate_handler.h"
#include "vm_sdk_def.h"
#include "vm_string_util.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::election;
using namespace ubse::mem::controller;
using namespace ubse::com;
using namespace ubse::nodeController;
using namespace api::server;
DynamicPriorityQueue<HamMigrateVmInfo> HamMigrate::clearQueue{};
std::mutex HamMigrate::clearMutex{};
std::condition_variable HamMigrate::clearCv{};
std::atomic<bool> HamMigrate::exitFlag(false);
std::atomic_bool HamMigrate::runFlag(false);
// Maximum retry count for rollback configuration
static const int MAX_RETRY_COUNT = 10;
// Maximum retry interval for rollback configuration
static const int MAX_RETRY_INTERVAL = 60;
// Cold-hot scan for deterministic migration
static const int HAM_SCAN_TYPE = 0;
static const int HAM_SCAN_TIME = 50;
// Cold-hot scan for waterline
static const int WATER_SCAN_TYPE = 1;
static const int WATER_SCANT_TIME = 200;
static const std::string HAM_MIGRATE = "ham_migrate";
static const std::string HAM_BORROW_START_WITH = "ham_";
static const std::string BORROW_ACTION = "borrow";
static const std::string CLEAR_ACTION = "clear";
static const int TRY_ABORT_HAM_MIGRATE_TIMES = 5;
static const int CANCEL_LEN_MAX = 32;
static unsigned int RESP_INFO_CODE_FOR_SUCCESS = 200;
VmResult HamMigrate::Start()
{
    uint16_t moduleId = VM_MODULE_CODE;
    UbseComEndpoint endpoint{
        .moduleId = moduleId,
        .serviceId = HAM_MIGRATE_CANCEL,
    };
    VmResult ret =
        RegisterIpcHandler(UBS_VA_VM_MIGRATE, UBS_VA_HAM_NORTH, HamMigrateNorth, UBS_VA_VM_MIGRATE_PERMISSION);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "UbseRegIpcService failed, moduleId = " << UBS_VA_VM_MIGRATE
                       << ", opId = " << UBS_VA_HAM_NORTH;
        return ret;
    }
    LibvirtHelper::GetInstance().Init();
    HamMigrate::Run();
    VmMigrateHandler::FlushExpireDataThread();
    ret = UbseRegRpcService(endpoint, HamMigrateCancel);
    endpoint.serviceId = HAM_MIGRATE_MESSAGE_TO_MASTER;
    ret |= UbseRegRpcService(endpoint, MasterDstInfoHandler);
    endpoint.serviceId = HAM_MIGRATE_MESSAGE_TO_AGENT;
    ret |= UbseRegRpcService(endpoint, AgentDstInfoHandler);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "UbseRegRpcService failed, moduleId = " << moduleId << ", serviceId = " << HAM_MIGRATE_CANCEL;
        return ret;
    }

    ret = RegisterAlarmFaultHandler(ALARM_PANIC_EVENT, HAM_MIGRATE, PanicEventHandler, AlarmHandlerPriority::HIGH);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "RegisterAlarmFaultHandler failed, name=" << HAM_MIGRATE
                       << ", ALARM_FAULT_TYPE=" << ALARM_PANIC_EVENT;
        return ret;
    }

    ret = VirtMigrateStrategy::Register();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Register migrate strategy failed. " << FormatRetCode(ret);
        return ret;
    }

    UBSE_LOG_INFO << "HamMigrateMaxTimeout=" << VmConfiguration::GetInstance().GetHamMigrateMaxTimeout();
    return VM_OK;
}

VmResult HamMigrate::Run()
{
    bool expected = false;
    if (!runFlag.compare_exchange_strong(expected, true)) {
        UBSE_LOG_INFO << "HamMigrate ClearThread completed successfully.";
        return VM_OK;
    }
    std::thread ClearThread(&HamMigrate::ClearQueueOperation);
    ClearThread.detach();
    UBSE_LOG_INFO << "HamMigrate ClearThread start.";
    return VM_OK;
}

VmResult HamMigrate::Stop()
{
    exitFlag.store(true);
    runFlag.exchange(false);
    clearCv.notify_all();
    UBSE_LOG_INFO << "HamMigrate ClearThread end.";
    return VM_OK;
}

void HamMigrate::LoadData()
{
    // Load data from the database into the queue after the process starts
    std::vector<HamMigrateVmInfo> hamMigrateVmInfos;
    HamMigrateVmInfoStorage::GetAllHamMigrateVmInfos(hamMigrateVmInfos);
    bool isChange = false;
    time_point<system_clock> timeZero;
    for (auto &info : hamMigrateVmInfos) {
        // After a restart, if a task is not in migration, it needs to be cleaned immediately
        if (!IsMigrating(info) && info.timeout != timeZero) {
            info.timeout = timeZero;
            isChange = true;
        }
    }
    if (isChange) {
        HamMigrateVmInfoStorage::SetHamMigrateVmInfos(hamMigrateVmInfos);
    }
    EnterClearQueue(hamMigrateVmInfos);
}

void HamMigrate::ClearQueueOperation()
{
    LoadData();
    while (true) {
        std::unique_lock<std::mutex> clearLock(clearMutex);
        auto until_time = system_clock::now() + seconds(VmConfiguration::GetInstance().GetHamMigrateMaxTimeout());
        if (!clearQueue.Empty()) {
            HamMigrateVmInfo hamMigrateVmInfo = clearQueue.Top();
            until_time = hamMigrateVmInfo.timeout;
        }
        if (exitFlag.load()) {
            UBSE_LOG_INFO << "[ham migrate] ClearQueue exit, flag=" << exitFlag.load();
            exitFlag.store(false);
            return;
        }
        clearCv.wait_until(clearLock, until_time);
        if (clearQueue.Empty() || clearQueue.Top().timeout > system_clock::now()) {
            continue;
        }
        UBSE_LOG_INFO << "[ham migrate] ClearQueue size=" << clearQueue.Size();
        HamMigrateVmInfo hamMigrateVmInfo = clearQueue.Top();
        clearQueue.Pop();
        VmResult ret = Rollback(hamMigrateVmInfo);
        if (ret != VM_OK) {
            // Exponential backoff: 1, 2, 4, 8s...
            auto time =
                system_clock::now() + seconds(1) * std::min(MAX_RETRY_INTERVAL, 1 << (hamMigrateVmInfo.count - 1));
            hamMigrateVmInfo.count++;
            hamMigrateVmInfo.timeout = time;
            clearQueue.Push(hamMigrateVmInfo);
        }
    }
}

VmResult HamMigrate::ProcessResponse(const RespInfo &respInfo, UbseIpcMessage &resp, uint64_t requestId)
{
    std::string respJson = respInfo.ToJson();
    const char *res = respJson.c_str();
    size_t node_id_len = strlen(res);
    resp.length = static_cast<uint32_t>(node_id_len + 1);
    resp.buffer = new (std::nothrow) uint8_t[resp.length];
    if (resp.buffer == nullptr) {
        UBSE_LOG_ERROR << "Failed to allocate memory for response buffer.";
        return VM_ERROR;
    }
    errno_t copy_result = memcpy_s(resp.buffer, resp.length, res, node_id_len + 1);
    if (copy_result != 0) {
        UBSE_LOG_ERROR << "memcpy_s failed with error code=" << copy_result;
        SafeDeleteArray(resp.buffer);
        return VM_ERROR;
    }
    SendResponse(VM_OK, requestId, resp);
    SafeDeleteArray(resp.buffer);
    return VM_OK;
}

/**  param example
 *   {"action":"borrow","srcHostname":"computer01","srcPid":xxx,"dstHostname":"computer02","dstPid":xxx,
 *      "valist":[{"start":xxx,"length":xxx}]}
 *   {"action":"clear","type":1,"srcHostname":"computer01","name":"xxx","srcPid":xxx,
 *      "dstHostname":"computer02","dstPid":xxx}
 */
uint32_t HamMigrate::HamMigrateNorth(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    UbseIpcMessage resp{};
    if (req.buffer == nullptr) {
        return VM_ERROR;
    }
    std::string body(reinterpret_cast<char *>(req.buffer), req.length);
    UBSE_LOG_INFO << "HamMigrate request=" << body;
    Document msgJson;
    msgJson.Parse(body.c_str());
    if (msgJson.HasParseError()) {
        UBSE_LOG_ERROR << "Bad Json Format=" << body;
        return VM_ERROR;
    }
    std::string action;
    if (VMJsonUtil::GetString(msgJson, "action", action) != VM_OK) {
        UBSE_LOG_ERROR << "Failed to get action from json str=" << body;
        return VM_ERROR;
    }
    RespInfo respInfo;
    respInfo.code = RESP_INFO_CODE_FOR_SUCCESS;
    auto ret = VM_ERROR;
    if (action == BORROW_ACTION) {
        BorrowInfo borrowInfo;
        ret = ConvertToBorrow(msgJson, borrowInfo);
        if (ret == VM_OK) {
            BorrowResponse borrowResponse;
            ret = Borrow(borrowInfo, borrowResponse);
            if (ret == VM_OK) {
                respInfo.message = borrowResponse.ToJson();
                UBSE_LOG_INFO << "HamMigrate borrowResponse=" << respInfo.ToJson();
                return ProcessResponse(respInfo, resp, context.requestId);
            }
        }
    } else if (action == CLEAR_ACTION) {
        ClearInfo clearInfo;
        ret = ConvertToClear(msgJson, clearInfo);
        if (ret == VM_OK) {
            ret = Clear(clearInfo);
            if (ret == VM_OK) {
                return ProcessResponse(respInfo, resp, context.requestId);
            }
        }
    } else {
        UBSE_LOG_ERROR << "unknown for action=" << action;
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "HamMigrate response error.";
    return VM_ERROR;
}

void HamMigrate::HamMigrateCancel(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOG_INFO << "[HamMigrateCancel] req start.";
    if (req.data == nullptr || req.len == 0 || req.len > CANCEL_LEN_MAX) {
        UBSE_LOG_ERROR << "Invalid argument.";
        HttpUtil::SetResp(resp, VM_ERROR_INVAL, "");
        return;
    }
    std::string nodeId{req.data, req.data + req.len};
    UBSE_LOG_INFO << "req nodeId = " << nodeId;
    std::vector<HamMigrateVmInfo> hamMigrateVmInfos;

    HamMigrateVmInfoStorage::GetHamMigrateVmInfosByDstNodeId(nodeId, hamMigrateVmInfos);
    if (hamMigrateVmInfos.empty()) {
        UBSE_LOG_WARN << "hamMigrateVmInfos is empty.";
        HttpUtil::SetResp(resp, VM_OK, nodeId);
        return;
    }
    VmResult res = VM_OK;
    for (auto &hamMigrateVmInfo : hamMigrateVmInfos) {
        hamMigrateVmInfo.dstNodeState = NodeState::PANIC;
        if (hamMigrateVmInfo.uuid.empty()) {
            UBSE_LOG_WARN << "req uuid is empty.";
            continue;
        }
        UBSE_LOG_INFO << "hamMigrateVmInfo.uuid=" << hamMigrateVmInfo.uuid;
        auto ret = LibvirtHelper::GetInstance().DomainAbortJobFlags(
            hamMigrateVmInfo.uuid, libvirt::VirDomainAbortJobFlagsValues::VIR_DOMAIN_ABORT_JOB_HAM,
            TRY_ABORT_HAM_MIGRATE_TIMES);
        if (ret != VM_OK) {
            time_point<system_clock> timeZero;
            hamMigrateVmInfo.timeout = timeZero;
            HamMigrateVmInfoStorage::SetHamMigrateVmInfo(hamMigrateVmInfo);
            EnterClearQueue(hamMigrateVmInfo, true);
            res = VM_ERROR;
        }
    }
    HttpUtil::SetResp(resp, res, nodeId);
    UBSE_LOG_INFO << "[HamMigrateCancel] req end.";
}

void HamMigrate::SrcNodeInfoReplyHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)
{
    UBSE_LOG_INFO << "SrcNodeInfoReplyHandler start.";
    if (ctx == nullptr) {
        UBSE_LOG_ERROR << "ctx is nullptr.";
        return;
    }
    auto result = static_cast<unsigned int *>(ctx);
    if (resCode != VM_OK) {
        *result = VM_ERROR;
        UBSE_LOG_ERROR << "error, " << FormatRetCode(resCode);
        return;
    }
    ResponseInfoMessage responseInfoSimpo(respData.data, respData.len);
    UBSE_LOG_INFO << "DstInfoReplyHandler responseInfoSimpo=" << responseInfoSimpo.ToString()
                  << ",respData.len=" << respData.len;
    if (const auto ret = responseInfoSimpo.Deserialize(); ret != VM_OK) {
        *result = VM_ERROR;
        UBSE_LOG_ERROR << "deserialize failed, ret=" << ret;
        return;
    }
    const auto [code, message] = responseInfoSimpo.GetResponseInfo();
    *result = code;
    UBSE_LOG_INFO << "SrcNodeInfoReplyHandler end, result=" << code;
}

void HamMigrate::MasterDstInfoHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOG_INFO << "[MasterDstInfoHandler] req start.";
    if (req.data == nullptr || req.len == 0) {
        UBSE_LOG_ERROR << "[MasterDstInfoHandler] params invalid.";
        HttpUtil::SetResp(resp, VM_ERROR_INVAL, "");
        return;
    }
    HamMigrateDstInfoMessage hamMigrateDstInfoMessage(req.data, req.len);
    auto ret = hamMigrateDstInfoMessage.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "hamMigrateDstInfoMessage deserialize failed. " << FormatRetCode(ret);
        HttpUtil::SetResp(resp, ret, "");
        return;
    }
    HamMigrateDstInfo hamMigrateDstInfo = hamMigrateDstInfoMessage.GetHamMigrateDstInfo();
    UBSE_LOG_INFO << "hamMigrateDstInfo.dstNodeId=" << hamMigrateDstInfo.dstNodeId
                  << ", hamMigrateDstInfo.dstPid=" << hamMigrateDstInfo.dstPid;
    const ubse::com::UbseComEndpoint endpoint = {
        .moduleId = VM_MODULE_CODE, .serviceId = HAM_MIGRATE_MESSAGE_TO_AGENT, .address = hamMigrateDstInfo.dstNodeId};
    const UbseByteBuffer reqData = {
        .data = (uint8_t *)&hamMigrateDstInfo.dstPid, .len = sizeof(uint64_t), .freeFunc = nullptr};
    unsigned int result = VM_ERROR;
    UbseRpcSend(endpoint, reqData, &result, MasterDstInfoReplyHandler);
    HttpUtil::SetResp(resp, result, "");
    UBSE_LOG_INFO << "[MasterDstInfoHandler] req end.";
}

void HamMigrate::MasterDstInfoReplyHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)
{
    UBSE_LOG_INFO << "MasterDstInfoReplyHandler start.";
    if (ctx == nullptr) {
        UBSE_LOG_ERROR << "ctx is nullptr.";
        return;
    }
    auto result = static_cast<unsigned int *>(ctx);
    if (resCode != VM_OK) {
        *result = VM_ERROR;
        UBSE_LOG_ERROR << "error, " << FormatRetCode(resCode);
        return;
    }
    ResponseInfoMessage responseInfoSimpo(respData.data, respData.len);
    UBSE_LOG_INFO << "DstInfoReplyHandler responseInfoSimpo=" << responseInfoSimpo.ToString()
                  << ",respData.len=" << respData.len;
    if (const auto ret = responseInfoSimpo.Deserialize(); ret != VM_OK) {
        *result = VM_ERROR;
        UBSE_LOG_ERROR << "deserialize failed, ret=" << ret;
        return;
    }
    const auto [code, message] = responseInfoSimpo.GetResponseInfo();
    *result = code;
    UBSE_LOG_INFO << "MasterDstInfoReplyHandler end, result=" << code;
}

void HamMigrate::AgentDstInfoHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOG_INFO << "[AgentDstInfoHandler] req start.";
    if (req.data == nullptr || req.len != sizeof(uint64_t)) {
        UBSE_LOG_ERROR << "[AgentDstInfoHandler] params invalid.";
        HttpUtil::SetResp(resp, VM_ERROR_INVAL, "");
        return;
    }
    uint64_t pid;
    if (memcpy_s(&pid, req.len, req.data, req.len) != EOK) {
        UBSE_LOG_ERROR << "memcpy_s failed.";
        HttpUtil::SetResp(resp, VM_ERROR_INVAL, "");
        return;
    }
    UBSE_LOG_INFO << "pid=" << pid;
    HttpUtil::SetResp(resp, PidIsVm(pid), "");
    UBSE_LOG_INFO << "[AgentDstInfoHandler] req end.";
}

VmResult HamMigrate::PidIsVm(const uint64_t pid)
{
    auto path = "/proc/" + std::to_string(pid) + "/numa_maps";
    std::string line;
    std::regex hugePageRegex("^(?=.*file=/dev/hugepages)(?!.*kernelpagesize_kB=2048).+$");
    std::ifstream file(path);
    if (!file.is_open()) {
        std::error_code ec(errno, std::system_category());
        UBSE_LOG_ERROR << "open " << path << " failed, " << ec.message();
        return VM_ERROR;
    }
    while (std::getline(file, line)) {
        std::smatch n1Match;
        if (std::regex_search(line, hugePageRegex)) {
            UBSE_LOG_INFO << "Found a line containing 'file=/dev/hugepages' and without 'kernelpagesize_kB=2048'";
            file.close();
            return VM_OK;
        }
    }
    UBSE_LOG_ERROR << "pid is not VM or not starting.";
    file.close();
    return VM_ERROR;
}

void HamMigrate::HamMigrateCancelReply(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)
{
    UBSE_LOG_INFO << "[HamMigrateCancel] reply start.";
    if (resCode != VM_OK) {
        UBSE_LOG_ERROR << "HamMigrateCancelReply error, " << FormatRetCode(resCode);
        return;
    }
    ResponseInfoMessage responseInfoMessage(respData.data, respData.len);
    if (const auto ret = responseInfoMessage.Deserialize(); ret != VM_OK) {
        UBSE_LOG_ERROR << "deserialize failed, " << FormatRetCode(ret);
        return;
    }
    const auto [code, nodeId] = responseInfoMessage.GetResponseInfo();
    if (code == VM_OK) {
        UBSE_LOG_INFO << "reply nodeId=" << nodeId << ", success.";
    } else {
        UBSE_LOG_ERROR << "reply nodeId=" << nodeId << ", " << FormatRetCode(code);
    }
    UBSE_LOG_INFO << "[HamMigrateCancel] reply end.";
}

VmResult HamMigrate::PanicEventHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo)
{
    UBSE_LOG_INFO << "PanicEventHandler alarmFaultEvent=" << alarmFaultEvent << ", faultInfo=" << faultInfo;
    std::string dstNodeId = faultInfo;
    std::vector<UbseMemAddrDesc> debtInfos;
    std::set<std::string> srcNodes;
    auto rets = UbseGetAddrMemDebtInfoWithNode(faultInfo, debtInfos);
    if (rets != UBSE_OK && rets != UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS) {
        UBSE_LOG_ERROR << "Get numaMemDebtInfo failed, res= " << static_cast<uint32_t>(rets);
        return VM_ERROR;
    }
    for (const auto &debtInfo : debtInfos) {
        if (debtInfo.name.length() > HAM_BORROW_START_WITH.length() &&
            debtInfo.name.substr(0, HAM_BORROW_START_WITH.length()) == HAM_BORROW_START_WITH &&
            std::to_string(debtInfo.importNode.slotId) != dstNodeId) {
            srcNodes.insert(std::to_string(debtInfo.importNode.slotId));
        }
    }
    if (srcNodes.empty()) {
        UBSE_LOG_INFO << "srcNodes is empty";
        return VM_OK;
    }
    bool isFail = false;
    for (auto &srcNode : srcNodes) {
        const UbseComEndpoint endpoint = {
            .moduleId = VM_MODULE_CODE, .serviceId = HAM_MIGRATE_CANCEL, .address = srcNode};
        UbseByteBuffer reqData;
        auto ret = HttpUtil::ToUbseByteBuffer(dstNodeId, reqData);
        if (ret != VM_OK) {
            isFail = true;
            UBSE_LOG_ERROR << "ToUbseByteBuffer failed.";
            continue;
        }
        UBSE_LOG_INFO << "srcNode = " << srcNode;
        ret = UbseRpcAsyncSend(endpoint, reqData, nullptr, HamMigrateCancelReply);
        if (ret != VM_OK) {
            isFail = true;
            UBSE_LOG_WARN << "srcNode = " << srcNode << "failed, " << FormatRetCode(ret);
        }
    }
    return isFail ? VM_ERROR : VM_OK;
}

VmResult HamMigrate::ConvertToBorrow(const Value &msgJson, BorrowInfo &borrowInfo)
{
    double srcPid;
    std::string srcHostname;
    std::string srcNodeId;
    double dstPid;
    std::string dstHostname;
    std::string dstNodeId;
    std::vector<VirtualAddress> valist;
    VmResult ret = VMJsonUtil::GetNumber(msgJson, "srcPid", srcPid);
    ret |= VMJsonUtil::GetString(msgJson, "srcHostname", srcHostname);
    ret |= VMJsonUtil::GetNumber(msgJson, "dstPid", dstPid);
    ret |= VMJsonUtil::GetString(msgJson, "dstHostname", dstHostname);
    if (!msgJson.HasMember("valist") || !msgJson["valist"].IsArray()) {
        ret |= VM_ERROR;
    } else {
        ret |= ConvertToVaList(msgJson["valist"], valist);
    }
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to parse BorrowInfo.";
        return VM_ERROR;
    }
    ret = UbseNodeGetNodeIdByHostname(srcHostname, srcNodeId);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to GetNodeIdByHostname=" << srcHostname;
        return VM_ERROR;
    }
    ret = UbseNodeGetNodeIdByHostname(dstHostname, dstNodeId);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to GetNodeIdByHostname=" << dstHostname;
        return VM_ERROR;
    }
    if (srcPid > INT_MAX || srcPid < 0 || dstPid > INT_MAX || dstPid < 0) {
        UBSE_LOG_ERROR << "srcPid or dstPid out of range, srcPid=" << srcPid << ", dstPid=" << dstPid;
        return VM_ERROR;
    }
    borrowInfo.srcPid = static_cast<int>(srcPid);
    borrowInfo.srcNodeId = srcNodeId;
    borrowInfo.dstPid = static_cast<int>(dstPid);
    borrowInfo.dstNodeId = dstNodeId;
    borrowInfo.valist = valist;
    UBSE_LOG_INFO << "ConvertToBorrow=" << borrowInfo.ToJson();
    return VM_OK;
}

VmResult HamMigrate::ConvertToVaList(const Value &msgJson, std::vector<VirtualAddress> &valist)
{
    VirtualAddress tmpItem{};
    if (!msgJson.IsArray()) {
        UBSE_LOG_ERROR << "Failed to get array.";
        return VM_ERROR;
    }
    auto i = 0;
    double numVal;
    for (auto &item : msgJson.GetArray()) {
        if (item.IsNull()) {
            UBSE_LOG_ERROR << "Failed to get array item[" << i << "].";
            return VM_ERROR;
        }
        auto ret = VMJsonUtil::GetNumber(item, "start", numVal);
        tmpItem.start = numVal;
        ret |= VMJsonUtil::GetNumber(item, "length", numVal);
        tmpItem.length = numVal;
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Failed to get field of array item[" << i << "].";
            return VM_ERROR;
        }
        i++;
        valist.emplace_back(tmpItem);
    }
    return VM_OK;
}

VmResult HamMigrate::ConvertToClear(const Value &msgJson, ClearInfo &clearInfo)
{
    double type;
    double srcPid = 0;
    std::string srcHostname;
    std::string srcNodeId;
    VmResult ret = VMJsonUtil::GetNumber(msgJson, "type", type);
    ret |= VMJsonUtil::GetString(msgJson, "srcHostname", srcHostname);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to parse ClearInfo.";
        return VM_ERROR;
    }
    clearInfo.state = ClearType(static_cast<int>(type));
    if (clearInfo.state == ClearType::MIGRATED_CLEAR || clearInfo.state == ClearType::NOMIGRATE_CLEAR) {
        ret = VMJsonUtil::GetNumber(msgJson, "srcPid", srcPid);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Failed to parse srcPid.";
            return VM_ERROR;
        }
    }
    ret = UbseNodeGetNodeIdByHostname(srcHostname, srcNodeId);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to GetNodeIdByHostname=" << srcHostname;
        return VM_ERROR;
    }
    double intPart;
    double fracPart = std::modf(srcPid, &intPart);
    if (fracPart != 0.0) {
        UBSE_LOG_ERROR << "Invalid srcPid: not an integer, srcPid=" << srcPid;
        return VM_ERROR;
    }
    if (srcPid > INT_MAX || srcPid < 0) {
        UBSE_LOG_ERROR << "Invalid srcPid: out of valid range, srcPid=" << srcPid;
        return VM_ERROR;
    }
    clearInfo.srcPid = static_cast<int>(srcPid);
    clearInfo.srcNodeId = srcNodeId;
    return VM_OK;
}

std::string HamMigrate::GetMasterNodeId()
{
    UbseRoleInfo masterNode{};
    auto retCode = UbseGetMasterInfo(masterNode);
    if (retCode != VM_OK) {
        UBSE_LOG_ERROR << "[ham migrate] get ubseMasterNodeId conf failed, " << FormatRetCode(retCode);
        return "";
    }
    return masterNode.nodeId;
}

VmResult HamMigrate::CheckPid(BorrowInfo &borrowInfo)
{
    const ubse::com::UbseComEndpoint endpoint = {
        .moduleId = VM_MODULE_CODE, .serviceId = HAM_MIGRATE_MESSAGE_TO_MASTER, .address = GetMasterNodeId()};
    HamMigrateDstInfoMessage hamMigrateDstInfoMessage;
    hamMigrateDstInfoMessage.SetHamMigrateDstInfo(borrowInfo.dstPid, borrowInfo.dstNodeId);
    auto ret = hamMigrateDstInfoMessage.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "hamMigrateDstInfoMessage Serialize fail, "
                       << " ret = " << ret;
        return ret;
    }

    const UbseByteBuffer reqData = {.data = hamMigrateDstInfoMessage.SerializedData(),
                                    .len = hamMigrateDstInfoMessage.SerializedDataSize(),
                                    .freeFunc = nullptr};
    unsigned int result = VM_ERROR;
    UbseRpcSend(endpoint, reqData, &result, SrcNodeInfoReplyHandler);
    return result;
}

void HamMigrate::HandleBorrowFailure(HamMigrateVmInfo &hamMigrateVmInfo)
{
    hamMigrateVmInfo.timeout = time_point<system_clock>();
    HamMigrateVmInfoStorage::SetHamMigrateVmInfo(hamMigrateVmInfo);
    EnterClearQueue(hamMigrateVmInfo, true);
}

VmResult HamMigrate::MigrateAndTracking(const HostVmDomainInfo &hostVmDomainInfo, BorrowInfo &borrowInfo,
                                        BorrowResponse &borrowResponse)
{
    for (const auto &vmDomainInfo : hostVmDomainInfo.vmDomainInfos) {
        if (vmDomainInfo.pid != borrowInfo.srcPid) {
            continue;
        }
        HamMigrateVmInfo hamMigrateVmInfo;
        UpdateHamMigrateVmInfo(borrowInfo, vmDomainInfo, hamMigrateVmInfo);
        auto ret = DoProcessMigrate(hamMigrateVmInfo);
        if (ret != VM_OK) {
            return VM_ERROR;
        }
        ret = DoProcessTracking(hamMigrateVmInfo);
        if (ret != VM_OK) {
            if (hamMigrateVmInfo.vmOpState != VmOpState::NOPE) {
                EnterClearQueue(hamMigrateVmInfo);
            }
            return VM_ERROR;
        }
        bool numaExistFlag = false;
        for (auto &[_, vmDomainNumaInfo] : vmDomainInfo.numaMemInfo) {
            if (vmDomainNumaInfo.isLocal) {
                borrowInfo.srcSocket = vmDomainNumaInfo.socketId;
                borrowInfo.srcNuma = vmDomainNumaInfo.numaId;
                numaExistFlag = true;
                break;
            }
        }
        if (!numaExistFlag) {
            UBSE_LOG_ERROR << "[ham migrate] The numaMemInfo is empty.";
            return VM_ERROR;
        }
        ret = DoBorrowAddress(borrowInfo, hamMigrateVmInfo, borrowResponse);
        if (ret != VM_OK) {
            HandleBorrowFailure(hamMigrateVmInfo);
            return VM_ERROR;
        }
        return VM_OK;
    }
    return VM_ERROR;
}

VmResult HamMigrate::Borrow(BorrowInfo &borrowInfo, BorrowResponse &borrowResponse)
{
    if (HasTask(borrowInfo)) {
        return VM_ERROR;
    }
    auto ret = CheckPid(borrowInfo);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[ham migrate] CheckPid Failed, ret = " << ret;
        return ret;
    }
    HostVmDomainInfo hostVmDomainInfo;
    ret = ResourceQuery::GetVmDomainInfosFromGlobal(hostVmDomainInfo);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[ham migrate] Query VmInfo, pid = " << borrowInfo.srcPid
                       << ", nodeId = " << borrowInfo.srcNodeId << ", " << FormatRetCode(ret);
        return ret;
    }

    ret = MigrateAndTracking(hostVmDomainInfo, borrowInfo, borrowResponse);
    if (ret == VM_OK) {
        return ret;
    }
    UBSE_LOG_ERROR << "[ham migrate] can not find, pid=" << borrowInfo.srcPid << ", nodeId=" << borrowInfo.srcNodeId;
    return VM_ERROR;
}

bool HamMigrate::HasTask(const BorrowInfo &borrowInfo)
{
    HamMigrateVmInfo hamMigrateVmInfo;
    auto ret = HamMigrateVmInfoStorage::GetHamMigrateVmInfo(borrowInfo.srcNodeId, borrowInfo.srcPid, hamMigrateVmInfo);
    if (ret == VM_OK) {
        time_point<system_clock> timeZero;
        hamMigrateVmInfo.timeout = timeZero;
        HamMigrateVmInfoStorage::SetHamMigrateVmInfo(hamMigrateVmInfo);
        EnterClearQueue(hamMigrateVmInfo, true);
        UBSE_LOG_ERROR << "[ham migrate] vm is in the task queue, pid = " << borrowInfo.srcPid
                       << ", nodeId = " << borrowInfo.srcNodeId;
        return true;
    }
    return false;
}

bool HamMigrate::IsMigrating(const HamMigrateVmInfo &hamMigrateVmInfo)
{
    // Memory borrowing has completed, but migration has not yet succeeded
    return hamMigrateVmInfo.vmOpState == VmOpState::BORROWED_ADDRESS && hamMigrateVmInfo.opState == OpState::END &&
           (hamMigrateVmInfo.vmState == VmState::BORROWED_NOMIGRATE ||
            hamMigrateVmInfo.vmState == VmState::NOBORROW_NOMIGRATE);
}

std::string HamMigrate::GetPrefixLog(const HamMigrateVmInfo &hamMigrateVmInfo)
{
    std::ostringstream oss;
    oss << "nodeId = " << hamMigrateVmInfo.nodeId << ", ";
    oss << "pid = " << hamMigrateVmInfo.pid << ", ";
    oss << "uuid = " << hamMigrateVmInfo.uuid << ", ";
    return oss.str();
}

VmResult HamMigrate::GetLocalNumaInfoFromNumaMemInfo(const MemNumaInfo &numaMemInfo, int &numaId, int &socketId)
{
    int tmpNumaId;
    int tmpSocketId;
    bool numaExistFlag = false;
    for (auto &[_, vmDomainNumaInfo] : numaMemInfo) {
        if (vmDomainNumaInfo.isLocal) {
            tmpNumaId = vmDomainNumaInfo.numaId;
            tmpSocketId = vmDomainNumaInfo.socketId;
            numaExistFlag = true;
            break;
        }
    }
    if (!numaExistFlag) {
        UBSE_LOG_ERROR << "[ham migrate] The local numa not exist in numaMemInfo.";
        return VM_ERROR;
    }
    numaId = tmpNumaId;
    socketId = tmpSocketId;
    return VM_OK;
}

void HamMigrate::UpdateHamMigrateVmInfo(const BorrowInfo &borrowInfo, const VmDomainInfo &vmDomainInfo,
                                        HamMigrateVmInfo &hamMigrateVmInfo)
{
    hamMigrateVmInfo.dstNodeId = borrowInfo.dstNodeId;
    size_t remoteNumaCount = 0;
    for (const auto &[numaId, vmDomainNumaInfo] : vmDomainInfo.numaMemInfo) {
        if (!vmDomainNumaInfo.isLocal) {
            remoteNumaCount++;
        }
    }
    if (remoteNumaCount > 0) {
        hamMigrateVmInfo.vmState = VmState::BORROWED_NOMIGRATE;
    } else {
        hamMigrateVmInfo.vmState = VmState::NOBORROW_NOMIGRATE;
    }
    hamMigrateVmInfo.nodeId = vmDomainInfo.nodeId;
    int numaId = -1;
    int socketId = -1;
    auto ret = GetLocalNumaInfoFromNumaMemInfo(vmDomainInfo.numaMemInfo, numaId, socketId);
    if (ret != VM_OK) {
        UBSE_LOG_WARN << "[ham migrate] get numaId and socketId from numaMemInfo failed. VM pid=" << vmDomainInfo.pid;
    }
    hamMigrateVmInfo.socketId = socketId;
    hamMigrateVmInfo.numaId = numaId;

    hamMigrateVmInfo.pid = vmDomainInfo.pid;
    hamMigrateVmInfo.uuid = vmDomainInfo.uuid;
    UBSE_LOG_INFO << "[ham migrate] " << GetPrefixLog(hamMigrateVmInfo) << "remoteNumaCount = " << remoteNumaCount;
}

void HamMigrate::UpdateDstNodeState(HamMigrateVmInfo &hamMigrateVmInfo)
{
    HamMigrateVmInfo tempVmInfo;
    const auto ret =
        HamMigrateVmInfoStorage::GetHamMigrateVmInfo(hamMigrateVmInfo.nodeId, hamMigrateVmInfo.pid, tempVmInfo);
    if (ret == VM_OK) {
        hamMigrateVmInfo.dstNodeState = tempVmInfo.dstNodeState;
    }
}

VmResult HamMigrate::DoProcessMigrate(HamMigrateVmInfo &hamMigrateVmInfo)
{
    std::string vmInfoPrefixLog = GetPrefixLog(hamMigrateVmInfo);
    std::string numaLoc = hamMigrateVmInfo.GetNumaLoc();
    // Call memory subsystem interface to disable cold-hot page migration
    VmResult ret = HttpUtil::EnableProcessMigrate(hamMigrateVmInfo.pid, false);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[ham migrate] enable_process_migrate is false, " << vmInfoPrefixLog << FormatRetCode(ret);
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "[ham migrate] enable_process_migrate is false, " << vmInfoPrefixLog << FormatRetCode(ret);
    UpdateDstNodeState(hamMigrateVmInfo);
    hamMigrateVmInfo.vmOpState = VmOpState::DISABLE_PROCESS_MIGRATE;
    if (hamMigrateVmInfo.dstNodeState == NodeState::PANIC) {
        UBSE_LOG_ERROR << "[ham migrate] uuid = " << hamMigrateVmInfo.uuid
                       << ", dstNodeId = " << hamMigrateVmInfo.dstNodeId << ", PANIC.";
        return VM_ERROR;
    }
    return HamMigrateVmInfoStorage::SetHamMigrateVmInfo(hamMigrateVmInfo);
}

VmResult HamMigrate::DoProcessTracking(HamMigrateVmInfo &hamMigrateVmInfo)
{
    std::string vmInfoPrefixLog = GetPrefixLog(hamMigrateVmInfo);
    // Call memory subsystem interface to set 5ms deterministic scan
    VmResult ret = HttpUtil::AddProcessTracking(hamMigrateVmInfo.pid, HAM_SCAN_TIME, HAM_SCAN_TYPE);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[ham migrate] add_process_tracking, scanTime = " << HAM_SCAN_TIME
                       << "ms, type: " << HAM_SCAN_TYPE << ", " << vmInfoPrefixLog << FormatRetCode(ret);
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "[ham migrate] add_process_tracking, scanTime = " << HAM_SCAN_TIME
                  << "ms, type = " << HAM_SCAN_TYPE << ", " << vmInfoPrefixLog << FormatRetCode(ret);
    UpdateDstNodeState(hamMigrateVmInfo);
    hamMigrateVmInfo.vmOpState = VmOpState::PROCESS_TRACKING;
    if (hamMigrateVmInfo.dstNodeState == NodeState::PANIC) {
        UBSE_LOG_ERROR << "[ham migrate] uuid = " << hamMigrateVmInfo.uuid
                       << ", dstNodeId = " << hamMigrateVmInfo.dstNodeId << ", PANIC.";
        return VM_ERROR;
    }
    return HamMigrateVmInfoStorage::SetHamMigrateVmInfo(hamMigrateVmInfo);
}

VmResult HamMigrate::DoBorrowAddress(BorrowInfo &borrowInfo, HamMigrateVmInfo &hamMigrateVmInfo,
                                     BorrowResponse &borrowResponse)
{
    const std::string borrowName = HAM_BORROW_START_WITH + VmStringUtil::GenerateUUID();
    hamMigrateVmInfo.vmOpState = VmOpState::BORROWED_ADDRESS;
    hamMigrateVmInfo.borrowName = borrowName;
    hamMigrateVmInfo.opState = OpState::START;
    UpdateDstNodeState(hamMigrateVmInfo);
    hamMigrateVmInfo.timeout = system_clock::now() + seconds(VmConfiguration::GetInstance().GetHamMigrateMaxTimeout());
    EnterClearQueue(hamMigrateVmInfo);
    if (hamMigrateVmInfo.dstNodeState == NodeState::PANIC) {
        UBSE_LOG_ERROR << "[ham migrate] [borrow] uuid = " << hamMigrateVmInfo.uuid
                       << ", dstNodeId = " << hamMigrateVmInfo.dstNodeId << ", PANIC.";
        return VM_ERROR;
    }
    HamMigrateVmInfoStorage::SetHamMigrateVmInfo(hamMigrateVmInfo);
    borrowInfo.name = borrowName;
    if (const auto ret = DoUbseBorrowAddress(borrowInfo, borrowResponse); ret != VM_OK) {
        UBSE_LOG_ERROR << "[ham migrate] [borrow] uuid = " << hamMigrateVmInfo.uuid
                       << ", dstNodeId = " << hamMigrateVmInfo.dstNodeId << ", failed.";
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "[ham migrate] [borrow] name = " << hamMigrateVmInfo.borrowName << ", borrow success.";
    UpdateDstNodeState(hamMigrateVmInfo);
    hamMigrateVmInfo.opState = OpState::END;
    if (hamMigrateVmInfo.dstNodeState == NodeState::PANIC) {
        UBSE_LOG_ERROR << "[ham migrate] [borrow] uuid = " << hamMigrateVmInfo.uuid
                       << ", dstNodeId = " << hamMigrateVmInfo.dstNodeId << ", PANIC.";
        return VM_ERROR;
    }
    return HamMigrateVmInfoStorage::SetHamMigrateVmInfo(hamMigrateVmInfo);
}

VmResult HamMigrate::DoUbseBorrowAddress(const BorrowInfo &borrowInfo, BorrowResponse &borrowResponse)
{
    UbseMemBorrower borrower{.nodeId = borrowInfo.srcNodeId};
    uint32_t slotId;
    try {
        slotId = VmStringUtil::SafeStoul(borrowInfo.dstNodeId);
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "[ham migrate] [borrow] invalid soltId = " << borrowInfo.dstNodeId;
        return VM_ERROR;
    }

    UbseMemProcessLender lender{
        .slotId = slotId, .socketId = borrowInfo.srcSocket, .pid = static_cast<uint64_t>(borrowInfo.dstPid)};
    for (const auto &[start, length] : borrowInfo.valist) {
        lender.vaLists.emplace_back(UbseMemAddrBorrowLocAndSizeByPid{.addr = start, .size = length});
    }
    UbseMemAddrDesc desc;
    int flag = vm::NO_1; // Non-relay
    auto ubsRet = UbseMemAddrCreate(borrowInfo.name, borrower, lender, flag, desc);
    if (ubsRet != UBSE_OK) {
        UBSE_LOG_ERROR << "[ham migrate] [borrow] ubse mem addr borrow return error. "
                       << FormatRetCode(static_cast<uint32_t>(ubsRet));
        return VM_ERROR;
    }

    borrowResponse.name = borrowInfo.name;
    borrowResponse.scna = 0;
    borrowResponse.numaIds.emplace_back(desc.numaId);
    UBSE_LOG_INFO << "[ham migrate] [borrow] ubse mem addr borrow success.";
    return VM_OK;
}

VmResult HamMigrate::Rollback(HamMigrateVmInfo &hamMigrateVmInfo)
{
    VmResult ret = RollbackBorrowAddress(hamMigrateVmInfo);
    ret |= RollbackProcessTracking(hamMigrateVmInfo);
    ret |= RollbackProcessMigrate(hamMigrateVmInfo);
    if (ret == VM_OK) {
        return HamMigrateVmInfoStorage::DelHamMigrateVmInfo(hamMigrateVmInfo.nodeId, hamMigrateVmInfo.pid);
    }
    // Retry reached maximum attempts, memory return failed; continue restoring cold-hot scan
    if (hamMigrateVmInfo.count == MAX_RETRY_COUNT && hamMigrateVmInfo.vmOpState == VmOpState::BORROWED_ADDRESS) {
        UBSE_LOG_WARN << "[ham migrate] [rollback] " << GetPrefixLog(hamMigrateVmInfo) << "failed to retry "
                      << hamMigrateVmInfo.count << " times, skip rollback borrow, retry rollback process_tracking.";
        hamMigrateVmInfo.vmOpState = VmOpState::PROCESS_TRACKING;
        ReSetReTry(hamMigrateVmInfo);
        return VM_ERROR;
    }
    if (hamMigrateVmInfo.count == MAX_RETRY_COUNT) {
        std::ostringstream oss;
        oss << "the maximum number of retry times is reached, retry is canceled, ";
        oss << "count = " << hamMigrateVmInfo.count << ", ";
        oss << GetPrefixLog(hamMigrateVmInfo);
        std::string vmInfoPrefixLog = oss.str();
        UBSE_LOG_WARN << "[ham migrate] [rollback] " << vmInfoPrefixLog << FormatRetCode(ret);
        return HamMigrateVmInfoStorage::DelHamMigrateVmInfo(hamMigrateVmInfo.nodeId, hamMigrateVmInfo.pid);
    }
    return ret;
}

VmResult HamMigrate::RollbackBorrowAddress(HamMigrateVmInfo &hamMigrateVmInfo)
{
    if (hamMigrateVmInfo.vmOpState != VmOpState::BORROWED_ADDRESS) {
        return VM_OK;
    }
    std::ostringstream oss;
    oss << "borrowName = " << hamMigrateVmInfo.borrowName << ", " << GetPrefixLog(hamMigrateVmInfo);
    const std::string vmInfoPrefixLog = oss.str();
    const auto ret = UbseRollbackBorrowAddress(hamMigrateVmInfo);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[ham migrate] [rollback] [return] " << vmInfoPrefixLog << FormatRetCode(ret);
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "[ham migrate] [rollback] [return] " << vmInfoPrefixLog << FormatRetCode(ret);
    hamMigrateVmInfo.vmOpState = VmOpState::PROCESS_TRACKING;
    ReSetReTry(hamMigrateVmInfo);
    return HamMigrateVmInfoStorage::SetHamMigrateVmInfo(hamMigrateVmInfo);
}

VmResult HamMigrate::UbseRollbackBorrowAddress(const HamMigrateVmInfo &hamMigrateVmInfo)
{
    UbseMemBorrower borrower{.nodeId = hamMigrateVmInfo.nodeId};
    auto ubsRet = UbseMemAddrDelete(hamMigrateVmInfo.borrowName, borrower);
    if (ubsRet != UBSE_OK) {
        UBSE_LOG_ERROR << "[ham migrate] [return] ubse mem return error. "
                       << FormatRetCode(static_cast<uint32_t>(ubsRet));
        return VM_ERROR;
    }

    UBSE_LOG_INFO << "[ham migrate] [return] ubse mem return success.";
    return VM_OK;
}

VmResult HamMigrate::RollbackProcessTracking(HamMigrateVmInfo &hamMigrateVmInfo)
{
    if (hamMigrateVmInfo.vmOpState != VmOpState::PROCESS_TRACKING) {
        return VM_OK;
    }
    VmResult ret;
    std::string vmInfoPrefixLog = GetPrefixLog(hamMigrateVmInfo);
    if (hamMigrateVmInfo.vmState == VmState::BORROWED_NOMIGRATE) {
        // Migration unsuccessful, call memory subsystem interface to set 200ms cold-hot scan
        ret = HttpUtil::AddProcessTracking(hamMigrateVmInfo.pid, WATER_SCANT_TIME, WATER_SCAN_TYPE);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "[ham migrate] [rollback] add_process_tracking, scanTime = " << WATER_SCANT_TIME
                           << "ms, type = " << WATER_SCAN_TYPE << ", " << vmInfoPrefixLog << FormatRetCode(ret);
            return VM_ERROR;
        }
        UBSE_LOG_INFO << "[ham migrate] [rollback] add_process_tracking, scanTime = " << WATER_SCANT_TIME
                      << "ms, type = " << WATER_SCAN_TYPE << ", " << vmInfoPrefixLog << FormatRetCode(ret);
        hamMigrateVmInfo.vmOpState = VmOpState::DISABLE_PROCESS_MIGRATE;
        ReSetReTry(hamMigrateVmInfo);
        return HamMigrateVmInfoStorage::SetHamMigrateVmInfo(hamMigrateVmInfo);
    }
    if (hamMigrateVmInfo.vmState == VmState::NOBORROW_NOMIGRATE) {
        // Migration unsuccessful, call memory subsystem interface to remove cold-hot scan process
        ret = HttpUtil::RemoveProcessTracking(hamMigrateVmInfo.pid);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "[ham migrate] [rollback] remove_process_tracking, " << vmInfoPrefixLog
                           << FormatRetCode(ret);
            return VM_ERROR;
        }
        UBSE_LOG_INFO << "[ham migrate] [rollback] remove_process_tracking, " << vmInfoPrefixLog << FormatRetCode(ret);
    }
    hamMigrateVmInfo.vmOpState = VmOpState::NOPE;
    return HamMigrateVmInfoStorage::SetHamMigrateVmInfo(hamMigrateVmInfo);
}

VmResult HamMigrate::RollbackProcessMigrate(HamMigrateVmInfo &hamMigrateVmInfo)
{
    if (hamMigrateVmInfo.vmOpState != VmOpState::DISABLE_PROCESS_MIGRATE) {
        return VM_OK;
    }
    std::string vmPrefixLog = GetPrefixLog(hamMigrateVmInfo);
    VmResult ret = HttpUtil::EnableProcessMigrate(hamMigrateVmInfo.pid, true);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[ham migrate] [rollback] enable_process_migrate is true, " << vmPrefixLog
                       << FormatRetCode(ret);
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "[ham migrate] [rollback] enable_process_migrate is true, " << vmPrefixLog << FormatRetCode(ret);
    return VM_OK;
}

void HamMigrate::ReSetReTry(HamMigrateVmInfo &hamMigrateVmInfo)
{
    hamMigrateVmInfo.count = 1;
    time_point<system_clock> timeoutZero;
    hamMigrateVmInfo.timeout = timeoutZero;
}

VmResult HamMigrate::Clear(const ClearInfo &clearInfo)
{
    if (clearInfo.state == ClearType::NODE) {
        // Full cleanup of the current node
        std::vector<HamMigrateVmInfo> hamMigrateVmInfos;
        HamMigrateVmInfoStorage::GetHamMigrateVmInfos(clearInfo.srcNodeId, hamMigrateVmInfos);
        if (hamMigrateVmInfos.empty()) {
            UBSE_LOG_WARN << "[ham migrate] can not find vmInfo, nodeId = " << clearInfo.srcNodeId;
            return VM_OK;
        }
        for (auto &hamMigrateVmInfo : hamMigrateVmInfos) {
            time_point<system_clock> timeZero;
            hamMigrateVmInfo.timeout = timeZero;
        }
        HamMigrateVmInfoStorage::SetHamMigrateVmInfos(hamMigrateVmInfos);
        EnterClearQueue(hamMigrateVmInfos, true);
        return VM_OK;
    }
    HamMigrateVmInfo hamMigrateVmInfo;
    VmResult ret =
        HamMigrateVmInfoStorage::GetHamMigrateVmInfo(clearInfo.srcNodeId, clearInfo.srcPid, hamMigrateVmInfo);
    if (ret != VM_OK) {
        UBSE_LOG_WARN << "[ham migrate] can not find vmInfo, pid = " << clearInfo.srcPid
                      << ", nodeId = " << clearInfo.srcNodeId << ", " << FormatRetCode(ret);
        return VM_OK;
    }
    if (clearInfo.state == ClearType::MIGRATED_CLEAR) {
        // Clean up after successful VM migration
        if (hamMigrateVmInfo.vmState == VmState::BORROWED_NOMIGRATE) {
            hamMigrateVmInfo.vmState = VmState::BORROWED_MIGRATED;
        } else if (hamMigrateVmInfo.vmState == VmState::NOBORROW_NOMIGRATE) {
            hamMigrateVmInfo.vmState = VmState::NOBORROW_MIGRATED;
        }
    }
    time_point<system_clock> timeZero;
    hamMigrateVmInfo.timeout = timeZero;
    HamMigrateVmInfoStorage::SetHamMigrateVmInfo(hamMigrateVmInfo);
    EnterClearQueue(hamMigrateVmInfo, true);
    return VM_OK;
}

void HamMigrate::EnterClearQueue(std::vector<HamMigrateVmInfo> &hamMigrateVmInfos, const bool &isUpdate)
{
    if (hamMigrateVmInfos.empty()) {
        return;
    }
    std::unique_lock<std::mutex> clearLock(clearMutex);
    for (auto &hamMigrateVmInfo : hamMigrateVmInfos) {
        if (isUpdate) {
            clearQueue.Update(hamMigrateVmInfo);
        } else {
            clearQueue.Push(hamMigrateVmInfo);
        }
    }
    clearCv.notify_all();
}

void HamMigrate::EnterClearQueue(HamMigrateVmInfo &hamMigrateVmInfo, const bool &isUpdate)
{
    std::unique_lock<std::mutex> clearLock(clearMutex);
    if (isUpdate) {
        clearQueue.Update(hamMigrateVmInfo);
    } else {
        clearQueue.Push(hamMigrateVmInfo);
    }
    clearCv.notify_all();
}
} // namespace vm