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

#include "fast_recovery.h"

#include <set>
#include <ubse_api_server_def.h>
#include <ubse_logger.h>
#include <ubse_error.h>
#include <ubse_node.h>
#include <ubse_mem_controller.h>
#include "libvirt_helper.h"
#include "libvirt_handler.h"
#include "resource_query.h"
#include "vm_configuration.h"
#include "vm_http_util.h"
#include "vm_json_util.h"
#include "vm_string_util.h"
#include "ham_migrate_dst_info_message.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("vm_plugin");
using namespace ubse::mem::controller;
using namespace ubse::nodeController;

static const int FAST_RECOVERY_CLEAR_RETRY_TIMES = 5;

VmResult FastRecovery::HandleFastRecoveryBorrow(const Document& msgJson, RespInfo& respInfo,
                                                UbseIpcMessage& resp, const UbseRequestContext& context)
{
    BorrowInfo borrowInfo{};
    auto ret = ConvertToBorrowFastRecovery(msgJson, borrowInfo);
    if (ret != VM_OK) {
        return ret;
    }
    BorrowResponse borrowResponse{};
    ret = BorrowFastRecovery(borrowInfo, borrowResponse);
    if (ret != VM_OK) {
        return ret;
    }
    respInfo.message = borrowResponse.ToJson();
    UBSE_LOG_INFO << "FastRecovery borrowResponse=" << respInfo.ToJson();
    return LibvirtHandler::ProcessResponse(respInfo, resp, context.requestId);
}

VmResult FastRecovery::HandleFastRecoveryClear(const Document& msgJson, RespInfo& respInfo,
                                               UbseIpcMessage& resp, const UbseRequestContext& context)
{
    std::string uuid{};
    auto ret = ConvertToClearFastRecovery(msgJson, uuid);
    if (ret != VM_OK) {
        return ret;
    }
    UbseMemBorrower borrower{.nodeId = VmConfiguration::GetInstance().GetNodeId()};
    for (int i = 0; i < FAST_RECOVERY_CLEAR_RETRY_TIMES; i++) {
        ret = UbseMemAddrDelete(uuid, borrower);
        if (ret == UBSE_ERR_UNIMPORT_SUCCESS || ret == UBSE_OK) {
            return LibvirtHandler::ProcessResponse(respInfo, resp, context.requestId);
        }
        UBSE_LOG_INFO << "HandleFastRecoveryClear retry, times=" << (i + 1) << ", ret=" << ret;
    }
    return ret;
}

VmResult FastRecovery::ConvertToBorrowFastRecovery(const Value &msgJson, vm::BorrowInfo &borrowInfo)
{
    std::string dstHostname;
    std::string dstNodeId;
    std::string uuid;
    std::vector<VirtualAddress> valist;
    VmResult ret = VMJsonUtil::GetString(msgJson, "uuid", uuid);
    ret |= VMJsonUtil::GetString(msgJson, "dstHostname", dstHostname);
    if (!msgJson.HasMember("valist") || !msgJson["valist"].IsArray()) {
        ret |= VM_ERROR;
    } else {
        ret |= LibvirtHandler::ConvertToVaList(msgJson["valist"], valist);
    }
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to parse BorrowInfo.";
        return VM_ERROR;
    }
    borrowInfo.valist = valist;
    borrowInfo.name = uuid;
    HostVmDomainInfo hostVmDomainInfo;
    ret = ResourceQuery::GetVmDomainInfosFromGlobal(hostVmDomainInfo);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Query VmInfo failed, " << FormatRetCode(ret);
        return ret;
    }
    borrowInfo.dstNodeId = hostVmDomainInfo.nodeId;
    for (const auto &vmDomainInfo : hostVmDomainInfo.vmDomainInfos) {
        if (vmDomainInfo.uuid == uuid) {
            borrowInfo.dstPid = vmDomainInfo.pid;
            break;
        }
    }
    if (borrowInfo.dstPid == 0) {
        UBSE_LOG_ERROR << "Failed get dstPid.";
        return VM_ERROR;
    }
    ret = UbseNodeGetNodeIdByHostname(dstHostname, dstNodeId);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to GetNodeIdByHostname=" << dstHostname;
        return VM_ERROR;
    }
    borrowInfo.srcNodeId = dstNodeId;
    UBSE_LOG_INFO << "ConvertToBorrow=" << borrowInfo.ToJson() << ", borrowInfo.dstPid=" << borrowInfo.dstPid;
    return VM_OK;
}

VmResult FastRecovery::ConvertToClearFastRecovery(const Value &msgJson, std::string &uuid)
{
    VmResult ret = VMJsonUtil::GetString(msgJson, "uuid", uuid);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to parse ClearInfo";
        return ret;
    }
    UBSE_LOG_INFO << "ConvertToClearFastRecovery: uuid = " << uuid;
    return VM_OK;
}

VmResult FastRecovery::BorrowFastRecovery(BorrowInfo& borrowInfo, BorrowResponse& borrowResponse)
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
        .slotId = slotId, .socketId = static_cast<int>(borrowInfo.srcSocket),
        .pid = static_cast<uint64_t>(borrowInfo.dstPid)};
    for (const auto &[start, length] : borrowInfo.valist) {
        lender.vaLists.emplace_back(UbseMemAddrBorrowLocAndSizeByPid{.addr = start, .size = length});
    }
    UbseMemAddrDesc desc;
    int flag = vm::NO_1; // Non-relay
    auto ubsRet = UbseMemAddrCreate(borrowInfo.name,
        borrower, lender, flag, static_cast<uint8_t>(ExportAccessMode::FAST_RECOVERY_MODE), desc);
    if (ubsRet != UBSE_OK) {
        UBSE_LOG_ERROR << "[fastrecovery] [borrow] ubse mem addr borrow return error. "
                        << FormatRetCode(static_cast<uint32_t>(ubsRet));
        return VM_ERROR;
    }
    borrowResponse.name = borrowInfo.name;
    borrowResponse.scna = 1;
    borrowResponse.numaIds.emplace_back(desc.numaId);
    UBSE_LOG_INFO << "[fastrecovery] [borrow] ubse mem addr borrow success.";
    return VM_OK;
}
}