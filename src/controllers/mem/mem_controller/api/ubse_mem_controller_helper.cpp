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

#include "ubse_mem_controller_helper.h"
#include <securec.h>
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_util.h"
#include "ubse_request_id_util.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::utils;

const uint8_t UBSE_MAX_LENDER_CNT = 2;
const uint32_t UBSE_MIN_MEM_SIZE = 4 * 1024 * 1024; // 128 MB

UbseResult UbseMemCreateWithLenderReqIsValid(const std::string& name, const UbseMemBorrower& borrower,
                                             const std::vector<UbseMemNumaLender>& lenders)
{
    if (!util::CheckName(name)) {
        return UBSE_ERR_INVALID_ARG;
    }
    if (borrower.nodeId.empty()) {
        UBSE_LOG_ERROR << "borrower nodeId empty";
        return UBSE_ERR_INVALID_ARG;
    }
    if (lenders.empty() || lenders.size() > UBSE_MAX_LENDER_CNT) {
        UBSE_LOG_ERROR << "Invalid lenders cnt: " << lenders.size();
        return UBSE_ERR_INVALID_ARG;
    }
    for (auto lender : lenders) {
        if (lender.size < UBSE_MIN_MEM_SIZE) {
            UBSE_LOG_ERROR << "Invalid lender size: " << lender.size;
            return UBSE_ERR_INVALID_ARG;
        }
    }
    return UBSE_OK;
}

static std::string GetCurNodeId()
{
    ubse::election::UbseRoleInfo currentRoleInfo{};
    UbseGetCurrentNodeInfo(currentRoleInfo); // 内部记录是否获取 ID 失败
    return currentRoleInfo.nodeId;
}

static uint64_t GenRequestId()
{
    uint8_t slotId = 0;
    ubse::election::UbseRoleInfo roleInfo{};
    auto ret = ubse::election::UbseGetCurrentNodeInfo(roleInfo);
    if (ret == UBSE_OK) {
        uint64_t nodeId = std::stoul(roleInfo.nodeId);
        slotId = static_cast<uint8_t>(nodeId);
    }
    static auto requestIdUtil = UbseRequestIdUtil(ubse::utils::UbseRequestType::INNER_REQUEST);
    return requestIdUtil.GenerateRequestId(slotId);
}

UbseResult ConvertUbseMemNumaCreateWithLenderReq(const std::string& name, const UbseMemBorrower& borrower,
                                                 const std::vector<UbseMemNumaLender>& lenders,
                                                 uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN],
                                                 UbseMemNumaBorrowReq& numaBorrowReq)
{
    numaBorrowReq.name = name;
    numaBorrowReq.requestNodeId = GetCurNodeId();
    numaBorrowReq.importNodeId = borrower.nodeId;
    for (auto lender : lenders) {
        numaBorrowReq.lenderLocs.push_back({std::to_string(lender.slotId), static_cast<uint32_t>(lender.numaId)});
        numaBorrowReq.lenderSizes.push_back(lender.size);
        numaBorrowReq.size += lender.size;
    }
    numaBorrowReq.srcSocket = borrower.affinitySocketId;
    numaBorrowReq.udsInfo.gid = 0;
    numaBorrowReq.udsInfo.uid = borrower.uid;
    numaBorrowReq.udsInfo.pid = 0;
    numaBorrowReq.udsInfo.username = borrower.username;
    numaBorrowReq.requestId = GenRequestId();
    if (memcpy_s(numaBorrowReq.usrInfo, UBSE_MAX_USR_INFO_LEN, usrInfo, UBSE_MAX_USR_INFO_LEN) != UBSE_OK) {
        UBSE_LOG_ERROR << "MemCopy fail when copy usrInfo, name is " << name;
        return UBSE_ERR_INVALID_ARG;
    }
    return SetDefaultMemBorrowPrivData(numaBorrowReq.ubseMemPrivData);
}

UbseResult UbseMemCreateReqIsValid(const std::string& name, const UbseMemBorrower& borrower,
                                   const UbseMemNumaCreateOpt& opt)
{
    if (!util::CheckName(name)) {
        return UBSE_ERR_INVALID_ARG;
    }
    if (borrower.nodeId.empty()) {
        UBSE_LOG_ERROR << "borrower nodeId empty";
        return UBSE_ERR_INVALID_ARG;
    }
    if (opt.size < UBSE_MIN_MEM_SIZE) {
        UBSE_LOG_ERROR << "Invalid size: " << opt.size;
        return UBSE_ERR_INVALID_ARG;
    }
    return UBSE_OK;
}

UbseResult ConvertUbseMemNumaCreateReq(const std::string& name, const UbseMemBorrower& borrower,
                                       const UbseMemNumaCreateOpt& opt, UbseMemNumaBorrowReq& numaBorrowReq)
{
    numaBorrowReq.name = name;
    numaBorrowReq.requestNodeId = GetCurNodeId();
    numaBorrowReq.importNodeId = borrower.nodeId;
    numaBorrowReq.size = opt.size;
    numaBorrowReq.distance = static_cast<ubse::adapter_plugins::mmi::UbseMemDistance>(opt.distance);
    numaBorrowReq.srcSocket = borrower.affinitySocketId;
    numaBorrowReq.highWatermark = opt.highWatermark;
    numaBorrowReq.udsInfo.gid = 0;
    numaBorrowReq.udsInfo.uid = borrower.uid;
    numaBorrowReq.udsInfo.pid = 0;
    numaBorrowReq.udsInfo.username = borrower.username;
    numaBorrowReq.requestId = GenRequestId();
    if (memcpy_s(numaBorrowReq.usrInfo, UBSE_MAX_USR_INFO_LEN, opt.usrInfo, UBSE_MAX_USR_INFO_LEN) != UBSE_OK) {
        UBSE_LOG_ERROR << "MemCopy fail when copy usrInfo, name is " << name;
        return UBSE_ERR_INVALID_ARG;
    }
    return SetDefaultMemBorrowPrivData(numaBorrowReq.ubseMemPrivData);
}

UbseResult UbseMemCreateWithCandidateReqIsValid(const std::string& name, const UbseMemBorrower& borrower,
                                                const UbseMemNumaCandidateOpt& opt)
{
    if (!util::CheckName(name)) {
        return UBSE_ERR_INVALID_ARG;
    }
    if (borrower.nodeId.empty()) {
        UBSE_LOG_ERROR << "borrower nodeId empty";
        return UBSE_ERR_INVALID_ARG;
    }
    if (opt.size < UBSE_MIN_MEM_SIZE) {
        UBSE_LOG_ERROR << "Invalid size: " << opt.size;
        return UBSE_ERR_INVALID_ARG;
    }
    if (opt.slotIds.empty()) {
        UBSE_LOG_ERROR << "Invalid slotIds size: " << opt.slotIds.size();
        return UBSE_ERR_INVALID_ARG;
    }
    return UBSE_OK;
}

UbseResult ConvertUbseMemNumaCreateWithCandidateReq(const std::string& name, const UbseMemBorrower& borrower,
                                                    const UbseMemNumaCandidateOpt& opt,
                                                    UbseMemNumaBorrowReq& numaBorrowReq)
{
    numaBorrowReq.name = name;
    numaBorrowReq.requestNodeId = GetCurNodeId();
    numaBorrowReq.importNodeId = borrower.nodeId;
    numaBorrowReq.size = opt.size;
    numaBorrowReq.distance = static_cast<ubse::adapter_plugins::mmi::UbseMemDistance>(opt.distance);
    numaBorrowReq.srcSocket = borrower.affinitySocketId;
    numaBorrowReq.highWatermark = opt.highWatermark;
    numaBorrowReq.udsInfo.gid = 0;
    numaBorrowReq.udsInfo.uid = borrower.uid;
    numaBorrowReq.udsInfo.pid = 0;
    numaBorrowReq.udsInfo.username = borrower.username;
    numaBorrowReq.requestId = GenRequestId();
    if (memcpy_s(numaBorrowReq.usrInfo, UBSE_MAX_USR_INFO_LEN, opt.usrInfo, UBSE_MAX_USR_INFO_LEN) != UBSE_OK) {
        UBSE_LOG_ERROR << "MemCopy fail when copy usrInfo, name is " << name;
        return UBSE_ERR_INVALID_ARG;
    }
    numaBorrowReq.candidateNodeList = opt.slotIds;
    return SetDefaultMemBorrowPrivData(numaBorrowReq.ubseMemPrivData);
}

UbseResult UbseMemDeleteReqIsValid(const std::string& name, const UbseMemBorrower& borrower)
{
    if (!util::CheckName(name)) {
        return UBSE_ERR_INVALID_ARG;
    }
    if (borrower.nodeId.empty()) {
        UBSE_LOG_ERROR << "borrower nodeId empty";
        return UBSE_ERR_INVALID_ARG;
    }
    return UBSE_OK;
}

void ConvertUbseMemDeleteReq(const std::string& name, const UbseMemBorrower& borrower, UbseMemReturnReq& returnReq)
{
    returnReq.name = name;
    returnReq.requestNodeId = GetCurNodeId();
    returnReq.requestId = GenRequestId();
    returnReq.importNodeId = borrower.nodeId;
}

UbseResult UbseMemAddrCreateReqIsValid(const std::string& name, const UbseMemBorrower& borrower,
                                       const UbseMemProcessLender& lender)
{
    if (!util::CheckName(name)) {
        return UBSE_ERR_INVALID_ARG;
    }
    if (borrower.nodeId.empty()) {
        UBSE_LOG_ERROR << "borrower nodeId empty";
        return UBSE_ERR_INVALID_ARG;
    }
    if (lender.pid == 0) {
        UBSE_LOG_ERROR << "lender pid equal 0";
        return UBSE_ERR_INVALID_ARG;
    }
    for (auto val : lender.vaLists) {
        if (val.size < UBSE_MIN_MEM_SIZE) {
            UBSE_LOG_ERROR << "Invalid size: " << val.size;
            return UBSE_ERR_INVALID_ARG;
        }
    }
    return UBSE_OK;
}

UbseResult ConvertUbseMemAddrCreateReq(const std::string& name, const UbseMemBorrower& borrower,
                                       const UbseMemProcessLender& lender, uint32_t flag,
                                       UbseMemAddrBorrowReq& addrBorrowReq)
{
    addrBorrowReq.name = name;
    addrBorrowReq.requestNodeId = GetCurNodeId();
    addrBorrowReq.importNodeId = borrower.nodeId;
    addrBorrowReq.srcSocket = borrower.affinitySocketId;
    addrBorrowReq.exportNodeId = std::to_string(lender.slotId);
    addrBorrowReq.dstSocket = lender.socketId;
    addrBorrowReq.exportPid = lender.pid;
    for (auto val : lender.vaLists) {
        addrBorrowReq.exportAddrList.push_back({val.addr, val.size});
    }
    addrBorrowReq.requestId = GenRequestId();
    return SetDefaultMemBorrowPrivData(addrBorrowReq.ubseMemPrivData, flag);
}
} // namespace ubse::mem::controller
