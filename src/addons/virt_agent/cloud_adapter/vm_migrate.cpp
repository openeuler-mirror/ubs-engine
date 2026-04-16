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
#include "vm_migrate.h"
#include <ubse_logger.h>
#include <ubse_api_server.h>
#include "resource_query.h"
#include "vm_http_util.h"
#include "resource_collect.h"
#include "vm_json_util.h"
#include "vm_sdk_def.h"
#include "ubs_virt_agent_object_def.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace api::server;
using namespace ubse::log;

VmResult VmMigrate::Register()
{
    auto ret = RegisterIpcHandler(UBS_VA_VM_MIGRATE, UBS_VA_PAGE_FLOW_AND_UPDATE_STATUS, UpdatePageFlowAndStatus,
                                  UBS_VA_VM_MIGRATE_PERMISSION);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Registration of UpdatePageFlowAndStatus failed, " << FormatRetCode(ret);
        return ret;
    }
    return VM_OK;
}

uint32_t VmMigrate::UpdatePageFlowAndStatus(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr || req.length == 0) {
        UBSE_LOG_ERROR << "req.buffer is null or req.length is zero.";
        return VM_INVALID_PARAM_ERROR;
    }
    UBSE_LOG_INFO << "Update page flow and status start.";
    UbseIpcMessage resp{};
    MemMigrateMsg memMigrateMsg(req.buffer, req.length);
    auto ret = memMigrateMsg.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Mem migrate message deserialize failed. " << FormatRetCode(ret);
        return ret;
    }
    MemMigrateInputParams inputParams = memMigrateMsg.GetInputParams();
    UBSE_LOG_INFO << "opt=" << inputParams.opt << ", uuid=" << inputParams.uuid;
    ret = ProcessRequest(inputParams, resp);
    UBSE_LOG_INFO << "Update page flow and status process request ret=" << ret;
    auto sendRet = SendResponse(ret, context.requestId, resp);
    if (sendRet != VM_OK) {
        UBSE_LOG_ERROR << "Update page flow and status send response send failed. " << FormatRetCode(sendRet);
        SafeDeleteArray(resp.buffer);
        return sendRet;
    }

    SafeDeleteArray(resp.buffer);
    UBSE_LOG_INFO << "Update page flow and status end.";
    return sendRet;
}

VmResult VmMigrate::ProcessRequest(MemMigrateInputParams &inputParams, UbseIpcMessage &response)
{
    std::string opt = inputParams.opt;
    std::string uuid = inputParams.uuid;

    std::string nodeId{};
    pid_t pid{};
    NumaMemInfoMap numaMemInfomap{};
    PageFlowResultParam pageFlowRes{};
    auto ret = FindNodeIdPid(uuid, nodeId, pid, numaMemInfomap);
    if (ret != VM_OK || pid <=0) {
        UBSE_LOG_ERROR << "Find pid failed.";
        return VM_ERROR;
    }

    if (opt == NONE_AND_MIGRATING || opt == NONE_AND_MIGRATE_SUCCESS || opt == NONE_AND_MIGRATE_FAILED) {
        ret = UpdateVmStatusByMigrateStatus(opt, numaMemInfomap, uuid, pid, response);
        return ret;
    }
    bool enableFlow = opt == ENABLE_AND_MIGRATE_FAILED;
    ret = HttpUtil::EnableProcessMigrate(pid, enableFlow);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "EnableProcessMigrate failed. enableFlow=" << enableFlow;
        return ret;
    }
    if (!enableFlow) {
        ret = ToUpdateVmStatus(numaMemInfomap, uuid, pid, VmMigrateStatus::MIGRATING);
        (void)BuildIpcResponse("disable page flow and update status for migrate", response);
    } else {
        ret = ToUpdateVmStatus(numaMemInfomap, uuid, pid, VmMigrateStatus::MIGRATEABLE);
        (void)BuildIpcResponse("enable page flow and update status for migrate", response);
    }
    return ret;
}

VmResult VmMigrate::ToUpdateVmStatus(const NumaMemInfoMap &numaMemInfoMap, const std::string &uuid, pid_t pid,
                                     VmMigrateStatus vmMigrateStatus)
{
    VmResult ret = VM_ERROR;
    int times = RETRY_TIMES;
    auto &vmResourceCollect = ResourceCollect::GetInstance();
    while (ret && times > 0) {
        ret = vmResourceCollect.UpdateVMStatus(numaMemInfoMap, uuid, pid, vmMigrateStatus);
        if (ret != VM_OK) {
            times--;
            if (times == 0) {
                UBSE_LOG_ERROR << "Update vm status failed.";
                return VM_ERROR;
            }
        }
    }
    UBSE_LOG_INFO << "Update vm status success.";
    return VM_OK;
}

VmResult VmMigrate::FindNodeIdPid(const std::string &uuid, std::string &nodeId, pid_t &pid,
                                  NumaMemInfoMap &numaMemInfoMap)
{
    HostVmDomainInfo hostVmDomainInfo{};
    if (auto ret = ResourceQuery::GetVmDomainInfosFromGlobal(hostVmDomainInfo); ret != VM_OK) {
        return ret;
    }
    for (const auto &vmDomainInfo : hostVmDomainInfo.vmDomainInfos) {
        if (vmDomainInfo.uuid == uuid) {
            nodeId = hostVmDomainInfo.nodeId;
            pid = vmDomainInfo.pid;
            numaMemInfoMap = vmDomainInfo.numaMemInfo;
            UBSE_LOG_INFO << "find nodeId=" << nodeId << ", pid=" << pid;
            return VM_OK;
        }
    }
    return VM_ERROR;
}

std::string PageFlowResultParam::ToJson()
{
    JSON_MAP resultMap;

    resultMap.emplace("ret", std::to_string(this->ret));
    resultMap.emplace("msg", this->msg);
    JSON_STR result;
    if (!VMJsonUtil::VMConvertMap2JsonStr(resultMap, result)) {
        UBSE_LOG_ERROR << "VMConvertMap2JsonStr error.";
        return "";
    }
    return result;
}

VmResult VmMigrate::BuildIpcResponse(const std::string &message, UbseIpcMessage &resp)
{
    resp.length = message.length();
    resp.buffer = new (std::nothrow) uint8_t[resp.length];
    if (resp.buffer == nullptr) {
        UBSE_LOG_ERROR << "Ipc response new failed.";
        return VM_ERROR;
    }
    auto ret = memcpy_s(resp.buffer, resp.length, message.c_str(), message.length());
    if (ret != EOK) {
        SafeDeleteArray(resp.buffer);
        UBSE_LOG_ERROR << "Ipc response memcpy_s failed.";
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult VmMigrate::UpdateVmStatusByMigrateStatus(const std::string &opt, const NumaMemInfoMap &numaMemInfoMap,
                                                  const std::string &uuid, pid_t pid, UbseIpcMessage &response)
{
    VmResult ret = VM_OK;
    std::string message = "";
    if (opt == NONE_AND_MIGRATING) {
        ret = ToUpdateVmStatus(numaMemInfoMap, uuid, pid, VmMigrateStatus::MIGRATING);
    }
    if (opt == NONE_AND_MIGRATE_SUCCESS || opt == NONE_AND_MIGRATE_FAILED) {
        ret = ToUpdateVmStatus(numaMemInfoMap, uuid, pid, VmMigrateStatus::MIGRATEABLE);
    }

    if (ret != VM_OK) {
        message = "update vm status failed";
    } else {
        message = "update vm status success";
    }
    BuildIpcResponse(message, response);
    return ret;
}
} // namespace vm