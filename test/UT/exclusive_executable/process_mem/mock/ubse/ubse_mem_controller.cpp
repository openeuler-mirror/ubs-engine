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

UbseResult UbseGetNumaMemDebtInfoWithNode(const std::string& nodeId, std::vector<UbseNumaMemoryDebtInfo>& debtInfos)
{
    return UBSE_OK;
}

UbseResult UbseMemNumaDelete(const std::string& name, const UbseMemBorrower& borrower)
{
    return UBSE_OK;
}

UbseResult UbseMemNumaCreate(const std::string& name, const UbseMemBorrower& borrower, const UbseMemNumaCreateOpt& opt,
                             UbseMemNumaDesc& desc)
{
    return UBSE_OK;
}

UbseResult UbseMemNumaCreateWithCandidate(const std::string& name, const UbseMemBorrower& borrower,
                                          const UbseMemNumaCandidateOpt& opt, UbseMemNumaDesc& desc)
{
    return UBSE_OK;
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
