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

#include "ubse_mem_controller.h"
#include "ubse_error.h"

namespace ubse::mem::controller {

// Configurable mock data for tests
static std::vector<UbseNumaMemoryDebtInfo> g_mockDebtInfos;
static std::vector<UbseNumaMemoryImportDebtInfo> g_mockImportDebtInfos;
static uint32_t g_mockNumaCreateError = UBSE_OK;
static uint32_t g_mockNumaDeleteError = UBSE_OK;
static uint32_t g_mockDebtInfoWithNodeError = UBSE_OK;

void MockSetDebtInfos(const std::vector<UbseNumaMemoryDebtInfo>& infos)
{
    g_mockDebtInfos = infos;
}
void MockSetImportDebtInfos(const std::vector<UbseNumaMemoryImportDebtInfo>& infos)
{
    g_mockImportDebtInfos = infos;
}
void MockClearDebtInfos()
{
    g_mockDebtInfos.clear();
    g_mockImportDebtInfos.clear();
}
void MockSetNumaCreateError(uint32_t err)
{
    g_mockNumaCreateError = err;
}
void MockSetNumaDeleteError(uint32_t err)
{
    g_mockNumaDeleteError = err;
}
void MockSetDebtInfoWithNodeError(uint32_t err)
{
    g_mockDebtInfoWithNodeError = err;
}
void MockResetAllErrors()
{
    g_mockDebtInfos.clear();
    g_mockImportDebtInfos.clear();
    g_mockNumaCreateError = UBSE_OK;
    g_mockNumaDeleteError = UBSE_OK;
    g_mockDebtInfoWithNodeError = UBSE_OK;
}

UbseResult UbseGetNumaMemDebtInfoWithNode(const std::string& nodeId, std::vector<UbseNumaMemoryDebtInfo>& debtInfos)
{
    if (g_mockDebtInfoWithNodeError != UBSE_OK) {
        return g_mockDebtInfoWithNodeError;
    }
    if (!g_mockDebtInfos.empty()) {
        debtInfos = g_mockDebtInfos;
    }
    return UBSE_OK;
}

UbseResult UbseMemNumaDelete(const std::string& name, const UbseMemBorrower& borrower)
{
    return g_mockNumaDeleteError;
}

UbseResult UbseMemNumaCreate(const std::string& name, const UbseMemBorrower& borrower, const UbseMemNumaCreateOpt& opt,
                             UbseMemNumaDesc& desc)
{
    return g_mockNumaCreateError;
}

UbseResult UbseMemNumaCreateWithCandidate(const std::string& name, const UbseMemBorrower& borrower,
                                          const UbseMemNumaCandidateOpt& opt, UbseMemNumaDesc& desc)
{
    return g_mockNumaCreateError;
}

UbseResult UbseGetNumaMemDebtInfo(std::vector<UbseNumaMemoryDebtInfo>& debtInfos)
{
    return UBSE_OK;
}

UbseResult UbseGetAllNodeNumaInfo(std::vector<UbseNodeNumaInfo>& numaNodeInfoList)
{
    return UBSE_OK;
}

uint32_t UbseQueryResult(const std::string& name, UbseMemResult& result, UbseMemBorrowType borrowType)
{
    return 0;
}

UbseResult UbseGetNumaMemImportDebtInfoWithLocalNode(std::vector<UbseNumaMemoryImportDebtInfo>& debtInfos)
{
    if (!g_mockImportDebtInfos.empty()) {
        debtInfos = g_mockImportDebtInfos;
        return UBSE_OK;
    }
    return UBSE_OK;
}

UbseResult UbseMemNumaCreateWithLender(const std::string& name, const UbseMemBorrower& borrower,
                                       const std::vector<UbseMemNumaLender>& lenders,
                                       uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN], UbseMemNumaDesc& desc)
{
    return UBSE_OK;
}

UbseResult UbseMemAddrCreate(const std::string& name, const UbseMemBorrower& borrower,
                             const UbseMemProcessLender& lender, uint32_t flag, UbseMemAddrDesc& desc)
{
    return UBSE_OK;
}

UbseResult UbseMemAddrDelete(const std::string& name, const UbseMemBorrower& borrower)
{
    return UBSE_OK;
}

UbseResult UbseMemDebtCircleCheck(const std::string& srcNodeId, const std::string& dstNodeId, bool& isCircle)
{
    isCircle = false;
    return UBSE_OK;
}

UbseResult UbseGetNodeNumaInfoByNodeId(const std::string& nodeId, std::vector<UbseNodeNumaInfo>& numaNodeInfoList)
{
    return UBSE_OK;
}

UbseResult UbseGetAddrMemDebtInfoWithNode(const std::string& nodeId, std::vector<UbseMemAddrDesc>& debtInfos)
{
    return UBSE_OK;
}
} // namespace ubse::mem::controller
