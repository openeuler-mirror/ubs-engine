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

#include "case_conf_sdk_server.h"

#include <ubse_api_server.h>
#include <ubse_logger.h>
#include "ubs_virt_agent_object_def.h"
#include "vm_configuration.h"
#include "vm_sdk_def.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;
using namespace api::server;

VmResult VirtCaseConfSdk::Register()
{
    auto ret =
        RegisterIpcHandler(UBS_VA_CASE_CONF, UBS_VA_CASE_CONF_GET, GetCaseConfHandler, UBS_VA_CASE_CONF_PERMISSION);
    ret |= RegisterIpcHandler(UBS_VA_CASE_CONF, UBS_VA_CASE_CONF_SET, SetCaseConfHandler, UBS_VA_CASE_CONF_PERMISSION);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Registration of VirtCaseConfSdk failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_DEBUG << "VirtCaseConfSdk registration succeed.";
    return VM_OK;
}

uint32_t VirtCaseConfSdk::GetCaseConf(CaseConfInfo &caseConfInfo)
{
    UBSE_LOG_INFO << "Get caseConf and overCommitment start.";
    CaseAndOvercommitmentRatio caseConf{};
    auto res = CaseConf::GetInstance().QueryCaseAndOverCommitmentRatio(caseConf);
    auto nodeId = VmConfiguration::GetInstance().GetNodeId();
    if (res != VM_OK) {
        return VM_ERROR;
    }

    if (caseConf.curCase.empty()) {
        UBSE_LOG_ERROR << "Query caseConf from db is empty, curCase is empty.";
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "Query caseConf from db, curCase=" << caseConf.curCase
                  << ", overCommitmentRatio=" << std::to_string(caseConf.overCommitmentRatio);
    caseConfInfo.curCase = caseConf.curCase;
    caseConfInfo.overCommitmentRatio = std::to_string(caseConf.overCommitmentRatio);
    caseConfInfo.index = caseConf.index;
    caseConfInfo.migrateWaterLine = VmConfiguration::GetInstance().GetHighWatermark();
    caseConfInfo.host_id = nodeId;
    return VM_OK;
}

uint32_t VirtCaseConfSdk::PackGetCaseConfRsp(CaseConfInfo &caseConf, UbseIpcMessage &buffer)
{
    CaseConfGetMsg msg{caseConf};
    auto ret = msg.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemFragmentationMsg Serialize fail. " << FormatRetCode(ret);
        return ret;
    }
    buffer.buffer = new (std::nothrow) uint8_t[msg.SerializedDataSize()];
    if (buffer.buffer == nullptr) {
        UBSE_LOG_ERROR << "PackGetCaseConfRsp new buffer failed.";
        return VM_ERROR;
    }
    ret = memcpy_s(buffer.buffer, msg.SerializedDataSize(), msg.SerializedData(), msg.SerializedDataSize());
    if (ret != EOK) {
        SafeDeleteArray(buffer.buffer);
        UBSE_LOG_ERROR << "PackGetCaseConfRsp memcpy_s failed.";
        return VM_ERROR;
    }
    buffer.length = msg.SerializedDataSize();
    return VM_OK;
}

uint32_t VirtCaseConfSdk::GetCaseConfHandler(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    CaseConfInfo caseConf{};
    std::string migrateWaterLine{};
    auto ret = GetCaseConf(caseConf);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Call GetCaseConf fail. " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_DEBUG << "caseConf.curCase=" << caseConf.curCase << ", migrateWaterLine=" << caseConf.migrateWaterLine
                   << ", overCommitmentRatio=" << caseConf.overCommitmentRatio << ", index=" << caseConf.index
                   << ", host_id=" << caseConf.host_id;
    UbseIpcMessage resp{};
    ret = PackGetCaseConfRsp(caseConf, resp);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Response pack failed, " << FormatRetCode(ret);
        return ret;
    }
    ret = SendResponse(VM_OK, context.requestId, resp);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "GetCaseConfHandler response send failed, " << FormatRetCode(ret);
    }

    SafeDeleteArray(resp.buffer);
    UBSE_LOG_DEBUG << "GetCaseConfHandler send succeed.";
    return ret;
}

uint32_t VirtCaseConfSdk::SetCaseConfCheckReq(const std::string &reqBodyStr, CaseConfParam &caseParam,
                                              CaseConfResultParam &caseResult)
{
    if (reqBodyStr.empty()) {
        UBSE_LOG_ERROR << "RequestBody is null.";
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "ParseCaseConfPostRequest reqBody=" << reqBodyStr;
    if (!caseParam.FromJson(reqBodyStr)) {
        UBSE_LOG_ERROR << "Parse caseConf param json failed.";
        return VM_ERROR;
    }
    if (caseParam.caseType == OVER_COMMITMENT_CASE && caseParam.overCommitmentRatio > MEM_FRAGMENTATION_RATIO &&
        caseParam.overCommitmentRatio <= MAX_OVER_COMMITMENT_RATIO) {
        caseResult.msg = "Set overCommitment case success.";
    } else if (caseParam.caseType == MEM_FRAGMENTATION_CASE &&
               caseParam.overCommitmentRatio == MEM_FRAGMENTATION_RATIO) {
        caseResult.msg = "Set memFragmentation case success.";
    } else {
        UBSE_LOG_ERROR << "Check body failed, caseType=" << caseParam.caseType
                       << ", overCommitmentRatio=" << caseParam.overCommitmentRatio;
        return VM_ERROR;
    }
    return VM_OK;
}

uint32_t VirtCaseConfSdk::SetCaseConf(const std::string &reqBodyStr, CaseConfParam &caseParam,
                                      CaseConfResultParam &caseResult)
{
    auto res = SetCaseConfCheckReq(reqBodyStr, caseParam, caseResult);
    if (res != VM_OK) {
        return VM_ERROR;
    }
    CaseAndOvercommitmentRatio caseConf{};
    res = CaseConf::GetInstance().QueryCaseAndOverCommitmentRatio(caseConf);
    if (res != VM_OK) {
        return VM_ERROR;
    }
    if (caseConf.curCase == OVER_COMMITMENT_CASE || caseConf.curCase == MEM_FRAGMENTATION_CASE) {
        UBSE_LOG_INFO << "The caseConf and overCommitmentRatio have been set.";
        return VM_OK;
    }
    // If the scenario does not exist, set the scenario and save it to the database.
    res = SetCaseConfToDB(caseParam);
    if (res != VM_OK) {
        return VM_ERROR;
    }
    caseResult.ret = VM_OK;
    return VM_OK;
}

uint32_t VirtCaseConfSdk::PackSetCaseConfRsp(const CaseConfSetInfo &caseConfSetInfo, UbseIpcMessage &buffer)
{
    CaseConfSetMsg caseConfSetMsg{caseConfSetInfo};
    auto ret = caseConfSetMsg.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "CaseConfSetMsg Serialize fail. " << FormatRetCode(ret);
        return ret;
    }

    buffer.buffer = new (std::nothrow) uint8_t[caseConfSetMsg.SerializedDataSize()];
    if (buffer.buffer == nullptr) {
        UBSE_LOG_ERROR << "PackSetCaseConfRsp new buffer failed.";
        return VM_ERROR;
    }
    ret = memcpy_s(buffer.buffer, caseConfSetMsg.SerializedDataSize(), caseConfSetMsg.SerializedData(),
                   caseConfSetMsg.SerializedDataSize());
    if (ret != EOK) {
        SafeDeleteArray(buffer.buffer);
        UBSE_LOG_ERROR << "PackSetCaseConfRsp memcpy_s failed.";
        return VM_ERROR;
    }
    buffer.length = caseConfSetMsg.SerializedDataSize();
    return VM_OK;
}

uint32_t VirtCaseConfSdk::SetCaseConfHandler(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "Set caseConf and overCommitment start.";
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "Request is null.";
        return VM_ERROR_NULLPTR;
    }
    CaseConfParam caseParam;
    CaseConfResultParam caseResult;
    std::string reqBodyStr(reinterpret_cast<const char *>(req.buffer), req.length);
    auto ret = SetCaseConf(reqBodyStr, caseParam, caseResult);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "SetCaseConf failed, " << FormatRetCode(ret);
        return ret;
    }
    CaseConfSetInfo caseConfSetInfo{caseParam.caseType, caseParam.overCommitmentRatio, caseResult.msg, caseResult.ret};
    UbseIpcMessage resp{};
    ret = PackSetCaseConfRsp(caseConfSetInfo, resp);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "PackSetCaseConfRsp failed, " << FormatRetCode(ret);
        return ret;
    }

    ret = SendResponse(VM_OK, context.requestId, resp);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "SetCaseConfHandler response send failed, " << FormatRetCode(ret);
    }
    SafeDeleteArray(resp.buffer);
    UBSE_LOG_INFO << "Set caseConf and overCommitment end.";
    return ret;
}
} // namespace vm