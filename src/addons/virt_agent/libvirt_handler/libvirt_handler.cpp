/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

#include "libvirt_handler.h"

#include <ubse_api_server.h>
#include <ubse_api_server_def.h>
#include <ubse_com.h>
#include <ubse_error.h>
#include <ubse_logger.h>
#include <set>
#include "fast_recovery.h"
#include "ham_migrate.h"
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

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::com;
static const std::string HAM_MIGRATE = "ham_migrate";
static const std::string FAST_RECOVERY_SCENE = "fastRecovery";
static const std::string BORROW_ACTION = "borrow";
static const std::string CLEAR_ACTION = "clear";
static const unsigned int RESP_INFO_CODE_FOR_SUCCESS = 200;
VmResult LibvirtHandler::Start()
{
    VmResult ret =
        RegisterIpcHandler(UBS_VA_VM_MIGRATE, UBS_VA_HAM_NORTH, HamMigrateNorth, UBS_VA_VM_MIGRATE_PERMISSION);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "UbseRegIpcService failed, moduleId = " << UBS_VA_VM_MIGRATE
                       << ", opId = " << UBS_VA_HAM_NORTH;
        return ret;
    }
    ret = LibvirtHelper::GetInstance().Init();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "LibvirtHelper init failed. " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "LibvirtHandler started.";
    return VM_OK;
}

/**  param example
*   {"action":"borrow","srcHostname":"computer01","srcPid":xxx,"dstHostname":"computer02","dstPid":xxx,
*      "valist":[{"start":xxx,"length":xxx}]}
*   {"action":"clear","type":1,"srcHostname":"computer01","name":"xxx","srcPid":xxx,
*      "dstHostname":"computer02","dstPid":xxx}
*/
uint32_t LibvirtHandler::HamMigrateNorth(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    UbseIpcMessage resp{};
    Document msgJson;
    std::string scene{};
    std::string action{};
    uint32_t ret = ValidateAndParseRequest(req, msgJson, scene, action);
    if (ret != VM_OK) {
        return ret;
    }

    RespInfo respInfo;
    respInfo.code = RESP_INFO_CODE_FOR_SUCCESS;

    if (action == BORROW_ACTION && scene.empty()) {
        ret = HamMigrate::HandleHamMigrateBorrow(msgJson, respInfo, resp, context);
    } else if (action == CLEAR_ACTION && scene.empty()) {
        ret = HamMigrate::HandleHamMigrateClear(msgJson, respInfo, resp, context);
    } else if (action == BORROW_ACTION && scene == FAST_RECOVERY_SCENE) {
        ret = FastRecovery::HandleFastRecoveryBorrow(msgJson, respInfo, resp, context);
    } else if (action == CLEAR_ACTION && scene == FAST_RECOVERY_SCENE) {
        ret = FastRecovery::HandleFastRecoveryClear(msgJson, respInfo, resp, context);
    } else {
        UBSE_LOG_ERROR << "unknow action=" << action << ", scene=" << scene;
        return VM_ERROR;
    }

    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "HamMigrate or FastRecovery error";
    }
    return ret;
}

VmResult LibvirtHandler::ConvertToVaList(const Value &msgJson, std::vector<VirtualAddress> &valist)
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

VmResult LibvirtHandler::ValidateAndParseRequest(const UbseIpcMessage &req, Document &msgJson, std::string &scene,
                                                 std::string &action)
{
    if (req.buffer == nullptr) {
        return VM_ERROR;
    }
    std::string body(reinterpret_cast<char *>(req.buffer), req.length);
    UBSE_LOG_INFO << "HamMigrate request: " << body;
    msgJson.Parse(body.c_str());
    if (msgJson.HasParseError()) {
        UBSE_LOG_ERROR << "Bad Json Format: " << body;
        return VM_ERROR;
    }
    VMJsonUtil::GetString(msgJson, "scene", scene);
    if (VMJsonUtil::GetString(msgJson, "action", action) != VM_OK) {
        UBSE_LOG_ERROR << "Failed to get action from json str: " << body;
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult LibvirtHandler::ProcessResponse(const RespInfo &respInfo, UbseIpcMessage &resp, uint64_t requestId)
{
    std::string respJson = respInfo.ToJson();
    const char *res = respJson.c_str();
    size_t nodeIdLen = strlen(res);
    resp.length = static_cast<uint32_t>(nodeIdLen + 1);
    resp.buffer = new (std::nothrow) uint8_t[resp.length];
    if (resp.buffer == nullptr) {
        UBSE_LOG_ERROR << "Failed to allocate memory for response buffer.";
        return VM_ERROR;
    }
    errno_t copy_result = memcpy_s(resp.buffer, resp.length, res, nodeIdLen + 1);
    if (copy_result != 0) {
        UBSE_LOG_ERROR << "memcpy_s failed with error code=" << copy_result;
        SafeDeleteArray(resp.buffer);
        return VM_ERROR;
    }
    auto sendRet = SendResponse(VM_OK, requestId, resp);
    if (sendRet != VM_OK) {
        UBSE_LOG_ERROR << "libvirtHandler send response send failed. " << FormatRetCode(sendRet);
        SafeDeleteArray(resp.buffer);
        return sendRet;
    }
    SafeDeleteArray(resp.buffer);
    return VM_OK;
}

} // namespace vm