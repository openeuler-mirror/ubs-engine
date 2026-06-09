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

#include "ubse_mem_controller.h"
#include "plugin_services/mem/ubse_mem_service.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_service_registry.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::service;
using namespace ubse::service::mem;
uint32_t UbseQueryResult(const std::string &name, UbseMemResult &result, UbseMemBorrowType borrowType)
{
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return memService->UbseQueryResult(name, result, borrowType);
}

UbseResult UbseGetNumaMemDebtInfoWithNode(const std::string &nodeId, std::vector<UbseNumaMemoryDebtInfo> &debtInfos)
{
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return memService->UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos);
}

UbseResult UbseGetNumaMemDebtInfo(std::vector<UbseNumaMemoryDebtInfo> &debtInfos)
{
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return memService->UbseGetNumaMemDebtInfo(debtInfos);
}

UbseResult UbseGetNumaMemImportDebtInfoWithLocalNode(std::vector<UbseNumaMemoryImportDebtInfo> &debtInfos)
{
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return memService->UbseGetNumaMemImportDebtInfoWithLocalNode(debtInfos);
}

UbseResult UbseMemNumaCreateWithLender(const std::string &name, const UbseMemBorrower &borrower,
                                       const std::vector<UbseMemNumaLender> &lenders,
                                       uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN], UbseMemNumaDesc &desc)
{
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return memService->UbseMemNumaCreateWithLender(name, borrower, lenders, usrInfo, desc);
}

UbseResult UbseMemNumaCreate(const std::string &name, const UbseMemBorrower &borrower, const UbseMemNumaCreateOpt &opt,
                             UbseMemNumaDesc &desc)
{
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return memService->UbseMemNumaCreate(name, borrower, opt, desc);
}

UbseResult UbseMemNumaCreateWithCandidate(const std::string &name, const UbseMemBorrower &borrower,
                                          const UbseMemNumaCandidateOpt &opt, UbseMemNumaDesc &desc)
{
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return memService->UbseMemNumaCreateWithCandidate(name, borrower, opt, desc);
}

UbseResult UbseMemNumaDelete(const std::string &name, const UbseMemBorrower &borrower)
{
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return memService->UbseMemNumaDelete(name, borrower);
}

UbseResult UbseMemAddrCreate(const std::string &name, const UbseMemBorrower &borrower,
                             const UbseMemProcessLender &lender, uint32_t flag, uint8_t exportAccessMode,
                             UbseMemAddrDesc &desc)
{
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    // 将参数封装到 UbseMemAddrCreateOpt 中
    service::mem::UbseMemAddrCreateOpt opt{
        .borrower = borrower,
        .lender = lender,
        .flag = flag,
        .exportAccessMode = exportAccessMode
    };
    return memService->UbseMemAddrCreate(name, opt, desc);
}

UbseResult UbseMemAddrDelete(const std::string &name, const UbseMemBorrower &borrower)
{
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return memService->UbseMemAddrDelete(name, borrower);
}

UbseResult UbseGetAllNodeNumaInfo(std::vector<UbseNodeNumaInfo> &numaNodeInfoList)
{
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return memService->UbseGetAllNodeNumaInfo(numaNodeInfoList);
}

UbseResult UbseGetNodeNumaInfoByNodeId(const std::string &nodeId, std::vector<UbseNodeNumaInfo> &numaNodeInfoList)
{
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return memService->UbseGetNodeNumaInfoByNodeId(nodeId, numaNodeInfoList);
}

UbseResult UbseMemDebtCircleCheck(const std::string &srcNodeId, const std::string &dstNodeId, bool &isCircle)
{
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return memService->UbseMemDebtCircleCheck(srcNodeId, dstNodeId, isCircle);
}

UbseResult UbseGetAddrMemDebtInfoWithNode(const std::string &nodeId, std::vector<UbseMemAddrDesc> &debtInfos)
{
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return memService->UbseGetAddrMemDebtInfoWithNode(nodeId, debtInfos);
}
} // namespace ubse::mem::controller