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

#ifndef UBS_ENGINE_UBSE_MEM_CONTROLLER_HELPER_H
#define UBS_ENGINE_UBSE_MEM_CONTROLLER_HELPER_H

#include <string>
#include <vector>
#include "ubse_mem_controller.h"
#include "ubse_mmi_interface.h"

namespace ubse::mem::controller {
using namespace ubse::adapter_plugins::mmi;

UbseResult UbseMemCreateWithLenderReqIsValid(const std::string &name, const UbseMemBorrower &borrower,
                                             const std::vector<UbseMemNumaLender> &lenders);

UbseResult ConvertUbseMemNumaCreateWithLenderReq(const std::string &name, const UbseMemBorrower &borrower,
                                                 const std::vector<UbseMemNumaLender> &lenders,
                                                 uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN],
                                                 UbseMemNumaBorrowReq &numaBorrowReq);

UbseResult UbseMemCreateReqIsValid(const std::string &name, const UbseMemBorrower &borrower,
                                   const UbseMemNumaCreateOpt &opt);

UbseResult ConvertUbseMemNumaCreateReq(const std::string &name, const UbseMemBorrower &borrower,
                                       const UbseMemNumaCreateOpt &opt, UbseMemNumaBorrowReq &numaBorrowReq);

UbseResult UbseMemCreateWithCandidateReqIsValid(const std::string &name, const UbseMemBorrower &borrower,
                                                const UbseMemNumaCandidateOpt &opt);

UbseResult ConvertUbseMemNumaCreateWithCandidateReq(const std::string &name, const UbseMemBorrower &borrower,
                                                    const UbseMemNumaCandidateOpt &opt,
                                                    UbseMemNumaBorrowReq &numaBorrowReq);

UbseResult UbseMemDeleteReqIsValid(const std::string &name, const UbseMemBorrower &borrower);

void ConvertUbseMemDeleteReq(const std::string &name, const UbseMemBorrower &borrower, UbseMemReturnReq &returnReq);

UbseResult UbseMemAddrCreateReqIsValid(const std::string &name, const UbseMemBorrower &borrower,
                                       const UbseMemProcessLender &lender);

void ConvertUbseMemAddrCreateReq(const std::string &name, const UbseMemBorrower &borrower,
                                 const UbseMemProcessLender &lender, uint32_t flag, uint8_t exportAccessMode,
                                 UbseMemAddrBorrowReq &addrBorrowReq);
} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_MEM_CONTROLLER_HELPER_H
